#include "keyboard_layout.h"
#include "config.h"
#include "file.h"
#include "key.h"
#include "keyboard_layout_mapping.h"
#include "keyboard_layout_notifications.h"
#include "str_utils.h"

#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace keyboard_layout
{
namespace
{
  struct Shortcut {
    std::string name, chord;
    KeyCode     key;
  };
  struct ResolvedKey {
    KeyCode     key = KeyCode::None;
    std::string physical, display, status = "pending";
  };

  bool                   enabled           = false;
  bool                   failed            = false;
  bool                   initialized       = false;
  bool                   vars_ready        = false;
  bool                   event_refresh     = false;
  RefreshState           refresh;
  unsigned               generation        = 0;
  uintptr_t              keyboard_identity = 0; // Identity only; never retained/dereferenced as a managed pointer.
  std::u16string         layout_identity;
  std::string            layout_name;
  std::string            status = "physical";
  std::string            reason = "configured_physical";
  std::vector<Shortcut>  shortcuts;
  std::array<ResolvedKey, LayoutKeyCount> resolved_keys;
  std::array<bool, LayoutKeyCount> requested_keys{};
  BindingState           bindings;
  toml::table            vars_snapshot;
  const MethodInfo *     current_method, *layout_method, *find_method, *key_method, *name_method, *display_method;
  int (*frame_count)() = nullptr;

  void WriteDiagnostics(toml::table& vars)
  {
    vars.insert_or_assign(
        "keyboard_mapping",
        toml::table{
            {"requested_mode", enabled ? "layout" : "physical"},
            {"effective_mode", enabled ? "layout" : "physical"},
            {"provider", enabled ? "Unity.InputSystem.Keyboard.FindKeyOnCurrentKeyboardLayout" : "legacy KeyCode"},
            {"refresh", !enabled ? "not_queried" : !initialized ? "pending"
                                           : event_refresh ? "device_notifications" : "layout_name_polling"},
            {"layout", layout_name},
            {"status", status},
            {"reason", reason},
            {"generation", generation},
            {"scope", "configured printable keys; explicit modifiers and named controls unchanged"},
            {"physical_key_reference", "US-QWERTY positions, not the user's printed keycaps"},
        });
    toml::table resolved;
    for (const auto& shortcut : shortcuts) {
      auto* alternatives = resolved[shortcut.name].as_array();
      if (!alternatives) {
        resolved.insert(shortcut.name, toml::array{});
        alternatives = resolved[shortcut.name].as_array();
      }
      toml::table entry{{"configured", shortcut.chord}};
      if (IsLayoutKey(shortcut.key)) {
        const auto  index = static_cast<int>(shortcut.key);
        const auto& key   = resolved_keys[index];
        entry.insert("configured_character", std::string(1, static_cast<char>(index)));
        entry.insert("status", enabled ? key.status : "physical");
        entry.insert("physical_us_key", enabled ? key.physical : std::string(1, static_cast<char>(index)));
        entry.insert("layout_display_name", enabled ? key.display : "not_queried");
        entry.insert("legacy_key_code", static_cast<int>(enabled ? key.key : shortcut.key));
      } else {
        entry.insert("status", "unchanged_named_control");
        entry.insert("legacy_key_code", static_cast<int>(shortcut.key));
      }
      alternatives->push_back(std::move(entry));
    }
    vars.insert_or_assign("shortcuts_resolved", std::move(resolved));
  }

  void Publish()
  {
    ++generation;
    if (vars_ready) {
      WriteDiagnostics(vars_snapshot);
      Config::Save(vars_snapshot, File::Vars());
    }
    spdlog::info("[KeyboardLayout] status={} layout='{}' generation={} reason={}", status, layout_name, generation,
                 reason);
  }

  void Unavailable(std::string_view why)
  {
    if (status == "unavailable" && reason == why)
      return;
    status            = "unavailable";
    reason            = why;
    keyboard_identity = 0;
    layout_identity.clear();
    layout_name.clear();
    for (auto& key : resolved_keys)
      key = {KeyCode::None, {}, {}, "unavailable"};
    bindings.Replace(LayoutKeys{}, [](KeyCode) { return false; });
    Publish();
  }

  Il2CppObject* Invoke(const MethodInfo* method, void* self = nullptr, void** args = nullptr)
  {
    if (failed || !method)
      return nullptr;
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(method, self, args, &exception);
    if (exception) {
      failed = true;
      spdlog::warn("[KeyboardLayout] Unity method {} failed; layout bindings disabled until restart", method->name);
    }
    return exception ? nullptr : result;
  }

  void Initialize()
  {
    initialized    = true;
    auto keyboard  = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard");
    auto key       = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem.Controls", "KeyControl");
    auto control   = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputControl");
    current_method = keyboard.GetMethodInfo("get_current", 0);
    layout_method  = keyboard.GetMethodInfo("get_keyboardLayout", 0);
    find_method    = keyboard.GetMethodInfo("FindKeyOnCurrentKeyboardLayout", 1);
    key_method     = key.GetMethodInfo("get_keyCode", 0);
    name_method    = control.GetMethodInfo("get_name", 0);
    display_method = control.GetMethodInfo("get_displayName", 0);
    frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    if (!current_method || !layout_method || !find_method || !key_method || !name_method || !display_method
        || !frame_count) {
      failed = true;
      Unavailable("missing_unity_api");
      return;
    }
    event_refresh = notifications::Start(refresh);
    spdlog::info("[KeyboardLayout] refresh={}", event_refresh ? "device_notifications" : "layout_name_polling");
  }

  void Update()
  {
    if (!initialized)
      Initialize();
    if (failed) {
      notifications::Stop();
      return;
    }
    const auto tick = refresh.Begin(frame_count());
    if (!tick.CheckKeyboard())
      return;
    // Notifications can arrive after another consumer queried this frame. Refresh
    // then too, but never clear transition/held-key protection twice in one frame.
    if (tick.new_frame)
      bindings.BeginFrame(Key::Pressed);
    auto* keyboard = Invoke(current_method);
    notifications::Watch(reinterpret_cast<uintptr_t>(keyboard));
    if (!keyboard) {
      Unavailable(failed ? "unity_invocation_failed" : "keyboard_absent");
      return;
    }
    const bool same_keyboard = keyboard_identity == reinterpret_cast<uintptr_t>(keyboard);
    if (event_refresh && !tick.invalidated && same_keyboard)
      return;
    auto* layout = reinterpret_cast<Il2CppString*>(Invoke(layout_method, keyboard));
    if (!layout || layout->length == 0) {
      Unavailable(failed ? "unity_invocation_failed" : "layout_unavailable");
      return;
    }
    const std::u16string_view identity(reinterpret_cast<const char16_t*>(layout->chars), layout->length);
    if (!tick.invalidated && same_keyboard && layout_identity == identity)
      return;

    // Resolve only configured printable keys, once per keyboard/layout generation.
    // No retained managed objects, per-frame strings, or silent physical fallback.
    LayoutKeys keys{};
    std::array<ResolvedKey, LayoutKeyCount> next;
    bool                   all_resolved = true;
    for (std::size_t index = 0; index < requested_keys.size() && !failed; ++index) {
      if (!requested_keys[index])
        continue;
      char  character[]{static_cast<char>(index), '\0'};
      auto* text = il2cpp_string_new(character);
      void* args[]{text};
      auto* control = Invoke(find_method, keyboard, args);
      auto& result  = next[index];
      result.status = "key_unavailable";
      if (control) {
        auto* boxed   = Invoke(key_method, control);
        auto* name    = reinterpret_cast<Il2CppString*>(Invoke(name_method, control));
        auto* display = reinterpret_cast<Il2CppString*>(Invoke(display_method, control));
        if (boxed && name && display) {
          result.key      = ToLegacyKey(*static_cast<int*>(il2cpp_object_unbox(boxed)));
          result.physical = to_string(name);
          result.display  = to_string(display);
          result.status   = result.key == KeyCode::None ? "unsupported_physical_key" : "resolved";
        }
      }
      keys[index] = result.key;
      all_resolved &= result.key != KeyCode::None;
    }
    if (failed) {
      Unavailable("unity_invocation_failed");
      return;
    }
    keyboard_identity = reinterpret_cast<uintptr_t>(keyboard);
    layout_identity   = identity;
    layout_name       = to_string(layout);
    resolved_keys     = std::move(next);
    bindings.Replace(keys, Key::Pressed);
    status = all_resolved ? "resolved" : "partial";
    reason = all_resolved ? "layout_lookup" : "unresolved_keys_disabled";
    Publish();
  }
} // namespace

void Configure(std::string_view mode)
{
  enabled = mode == "layout";
  status  = enabled ? "pending" : "physical";
  reason  = enabled ? "awaiting_game_input" : "configured_physical";
}

void RegisterShortcut(std::string_view name, std::string_view chord, KeyCode key)
{
  shortcuts.push_back({std::string(name), std::string(chord), key});
  if (IsLayoutKey(key))
    requested_keys[static_cast<int>(key)] = true;
}

void InitializeDiagnostics(toml::table& vars)
{
  WriteDiagnostics(vars);
  if (enabled) {
    vars_snapshot = vars;
    vars_ready    = true;
  }
}

KeyCode Resolve(KeyCode configured)
{
  if (!enabled || !IsLayoutKey(configured))
    return configured;
  Update();
  return bindings.Resolve(configured);
}
} // namespace keyboard_layout
