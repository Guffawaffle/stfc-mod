#include "keyboard_layout.h"
#include "config.h"
#include "file.h"
#include "key.h"
#include "keyboard_layout_mapping.h"
#include "keyboard_layout_notifications.h"
#include "keyboard_layout_windows.h"
#include "keyboard_layout_preview_windows.h"
#include "modifierkey.h"
#include "str_utils.h"

#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace keyboard_layout
{
namespace
{
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
  struct Shortcut {
    std::string name, chord;
    std::string explicit_modifiers;
    KeyCode     key;
  };
  struct ResolvedKey {
    KeyCode     key = KeyCode::None;
    std::string physical, display, status = "pending";
    ChordCandidate candidate;
  };
  bool                                    diagnostics = false;
  std::vector<Shortcut>                   shortcuts;
  std::array<ResolvedKey, LayoutKeyCount> resolved_keys;
  const MethodInfo *                      name_method = nullptr, *display_method = nullptr;
#endif

  bool                             enabled     = false;
  bool                             failed      = false;
  bool                             initialized = false;
  bool                             vars_ready  = false;
  RefreshState                     refresh;
  unsigned                         generation = 0;
  std::string                      layout_name;
  std::string                      status = "physical";
  std::string                      reason = "configured_physical";
  std::array<bool, LayoutKeyCount> requested_keys{};
  BindingState                     bindings;
  std::array<bool, LayoutKeyCount> required_shift{};
  toml::table                      vars_snapshot;
  const MethodInfo *               current_method, *layout_method, *find_method, *key_method;
  int (*frame_count)() = nullptr;

  void WriteDiagnostics(toml::table& vars)
  {
    vars.insert_or_assign("keyboard_mapping", toml::table{
                                                  {"effective_mode", !enabled ? "physical"
                                                                     : failed ? "unavailable"
                                                                              : "layout"},
                                                  {"refresh", !enabled       ? "not_queried"
                                                              : !initialized ? "pending"
                                                              : failed       ? "disabled"
                                                                             : "device_notifications"},
                                                  {"layout", layout_name},
                                                  {"status", status},
                                                  {"reason", reason},
                                                  {"generation", generation},
                                              });
    vars.erase("shortcuts_resolved");
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
    if (!enabled || !diagnostics)
      return;
    toml::table resolved;
    for (const auto& shortcut : shortcuts) {
      auto* alternatives = resolved[shortcut.name].as_array();
      if (!alternatives) {
        resolved.insert(shortcut.name, toml::array{});
        alternatives = resolved[shortcut.name].as_array();
      }
      toml::table entry{{"configured", shortcut.chord}};
      const auto  index = static_cast<int>(shortcut.key);
      const auto& key   = resolved_keys[index];
      entry.insert("configured_character", std::string(1, static_cast<char>(index)));
      entry.insert("status", key.status);
      entry.insert("physical_us_key", key.physical);
      entry.insert("layout_display_name", key.display);
      entry.insert("legacy_key_code", static_cast<int>(key.key));
      const auto preview = PreviewChord(key.candidate, shortcut.explicit_modifiers);
      entry.insert("chord_preview", toml::table{
          {"dispatch_active", key.status == "resolved_windows_chord"}, {"source", "windows_layout_candidate"},
          {"status", preview.status}, {"configured", shortcut.chord},
          {"explicit_modifiers", preview.explicit_modifiers},
          {"required_modifiers", CandidateModifiers(key.candidate.required_modifiers)},
          {"required_modifier_mask", static_cast<int64_t>(key.candidate.required_modifiers)},
          {"physical_us_key", CandidatePhysicalLabel(key.candidate.physical_key)},
          {"legacy_key_code", static_cast<int>(key.candidate.physical_key)},
          {"scan_code", static_cast<int64_t>(key.candidate.scan_code)},
          {"base_key_label", key.candidate.base_label}, {"base_key_is_dead", key.candidate.base_key_is_dead},
          {"required_press", preview.required_press}, {"suggested_press", preview.suggested_press},
          {"layout", layout_name}, {"generation", generation}});
      alternatives->push_back(std::move(entry));
    }
    vars.insert_or_assign("shortcuts_resolved", std::move(resolved));
#endif
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
    status = "unavailable";
    reason = why;
    layout_name.clear();
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
    for (auto& key : resolved_keys)
      key = {KeyCode::None, {}, {}, "unavailable"};
#endif
    bindings.Clear();
    required_shift = {};
    Publish();
  }

  void Disable(std::string_view why)
  {
    failed = true;
    notifications::Stop();
    spdlog::warn("[KeyboardLayout] {}; layout bindings disabled until restart; no polling fallback", why);
    Unavailable(why);
  }

  Il2CppObject* Invoke(const MethodInfo* method, void* self = nullptr, void** args = nullptr,
                       bool* lookup_failed = nullptr)
  {
    if (failed || !method)
      return nullptr;
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(method, self, args, &exception);
    if (exception) {
      if (lookup_failed)
        *lookup_failed = true;
      else
        failed = true;
      if (!lookup_failed)
        spdlog::warn("[KeyboardLayout] Unity method {} failed; layout bindings disabled until restart", method->name);
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
      if (diagnostics) {
        char message[2048]{};
        char trace[4096]{};
        il2cpp_format_exception(exception, message, sizeof(message) - 1);
        il2cpp_format_stack_trace(exception, trace, sizeof(trace) - 1);
        spdlog::warn("[KeyboardLayout] exception: {}; stack: {}", message, trace);
      }
#endif
    }
    return exception ? nullptr : result;
  }

  void Initialize()
  {
    initialized    = true;
    auto keyboard  = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard");
    auto key       = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem.Controls", "KeyControl");
    current_method = keyboard.GetMethodInfo("get_current", 0);
    layout_method  = keyboard.GetMethodInfo("get_keyboardLayout", 0);
    find_method    = keyboard.GetMethodInfo("FindKeyOnCurrentKeyboardLayout", 1);
    key_method     = key.GetMethodInfo("get_keyCode", 0);
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
    if (diagnostics) {
      auto control   = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputControl");
      name_method    = control.GetMethodInfo("get_name", 0);
      display_method = control.GetMethodInfo("get_displayName", 0);
    }
#endif
    frame_count = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    if (!current_method || !layout_method || !find_method || !key_method || !frame_count) {
      Disable("missing_unity_api");
      return;
    }
    const auto subscription = notifications::Start(refresh);
    if (subscription != notifications::Result::Started) {
      Disable(subscription == notifications::Result::Unsupported ? "notifications_unsupported"
                                                                 : "notification_subscription_failed");
      return;
    }
    spdlog::info("[KeyboardLayout] refresh=device_notifications");
  }

  void Update()
  {
    if (!initialized)
      Initialize();
    if (failed || !refresh.Consume())
      return;
    // Initialization and notifications are the only reasons to query Unity.
    auto* keyboard = Invoke(current_method);
    if (!keyboard) {
      if (failed)
        Disable("unity_invocation_failed");
      else
        Unavailable("keyboard_absent");
      return;
    }
    auto* layout = reinterpret_cast<Il2CppString*>(Invoke(layout_method, keyboard));
    if (!layout || layout->length == 0) {
      if (failed)
        Disable("unity_invocation_failed");
      else
        Unavailable("layout_unavailable");
      return;
    }

    // Resolve only configured printable keys, once per keyboard/layout generation.
    // No retained managed objects, per-frame strings, or silent physical fallback.
    LayoutKeys keys{};
    std::array<bool, LayoutKeyCount> next_shift{};
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
    std::array<ResolvedKey, LayoutKeyCount> next;
#endif
    bool all_resolved = true;
    for (std::size_t index = 0; index < requested_keys.size() && !failed; ++index) {
      if (!requested_keys[index])
        continue;
      char  character[]{static_cast<char>(index), '\0'};
      ChordCandidate candidate;
#if _WIN32
      candidate = ResolveWindowsChordCandidate(character[0], to_string(layout));
      // Windows translates the character to a VK plus modifiers, then a scan
      // position. Named controls never enter this path. Do not invoke Unity's
      // exception-prone display-name search for a known native translation.
      if (candidate.status == "candidate" || candidate.status == "modifier_policy_required") {
        keys[index] = candidate.status == "candidate" ? candidate.physical_key : KeyCode::None;
        next_shift[index] = keys[index] != KeyCode::None && (candidate.required_modifiers & 1) != 0;
        all_resolved &= keys[index] != KeyCode::None;
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
        if (diagnostics) {
          auto& result = next[index];
          result.key = keys[index];
          result.candidate = candidate;
          result.status = keys[index] != KeyCode::None ? "resolved_windows_chord" : candidate.status;
          result.physical = CandidatePhysicalLabel(candidate.physical_key);
          result.display = candidate.base_label;
        }
#endif
        continue;
      }
#else
      candidate.status = "platform_not_implemented";
#endif
      auto* text = il2cpp_string_new(character);
      void* args[]{text};
      // Unity 1.14.2's search dereferences its null IMESelected slot when no
      // earlier key matches. An unmatched symbol must not poison other bindings
      // or unsubscribe layout notifications. Retry it only on the next generation.
      bool  lookup_failed = false;
      auto* control       = Invoke(find_method, keyboard, args, &lookup_failed);
      KeyCode resolved_key = KeyCode::None;
      if (control) {
        auto* boxed = Invoke(key_method, control);
        if (boxed)
          resolved_key = ToLegacyKey(*static_cast<int*>(il2cpp_object_unbox(boxed)));
      }
      bool dead_key_fallback = false;
#if _WIN32
      if (!control && !failed) {
        resolved_key = ResolveWindowsDeadKey(character[0], to_string(layout));
        dead_key_fallback = resolved_key != KeyCode::None;
      }
#endif
      if (lookup_failed && !dead_key_fallback)
        spdlog::warn("[KeyboardLayout] lookup failed for character '{}' ({}) on layout '{}'", character, index,
                     to_string(layout));
      keys[index] = resolved_key;
      all_resolved &= resolved_key != KeyCode::None;
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
      if (diagnostics) {
        auto& result  = next[index];
        result.key    = resolved_key;
        result.candidate = candidate;
        result.status = dead_key_fallback              ? "resolved_windows_dead_key"
                        : lookup_failed                   ? "lookup_failed"
                        : !control                      ? "key_unavailable"
                        : resolved_key == KeyCode::None ? "unsupported_physical_key"
                                                        : "resolved";
        if (control && !failed) {
          // Diagnostic metadata never determines binding validity.
          bool  detail_failed = false;
          auto* name          = reinterpret_cast<Il2CppString*>(Invoke(name_method, control, nullptr, &detail_failed));
          auto* display = reinterpret_cast<Il2CppString*>(Invoke(display_method, control, nullptr, &detail_failed));
          if (name)
            result.physical = to_string(name);
          if (display)
            result.display = to_string(display);
          if (!name || !display || detail_failed)
            spdlog::warn("[KeyboardLayout] diagnostic metadata unavailable for character '{}'", character);
        }
      }
#endif
    }
    if (failed) {
      Disable("unity_invocation_failed");
      return;
    }
    layout_name = to_string(layout);
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
    if (diagnostics)
      resolved_keys = std::move(next);
#endif
    bindings.Replace(keys, Key::Pressed, frame_count());
    required_shift = next_shift;
    status = all_resolved ? "resolved" : "partial";
    reason = all_resolved ? "layout_lookup" : "unresolved_keys_disabled";
    Publish();
  }
} // namespace

void Configure(std::string_view mode, bool detailed_diagnostics)
{
  enabled = mode == "layout";
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
  diagnostics = detailed_diagnostics;
#else
  (void)detailed_diagnostics;
#endif
  status = enabled ? "pending" : "physical";
  reason = enabled ? "awaiting_game_input" : "configured_physical";
}

void RegisterShortcut(std::string_view name, std::string_view chord, KeyCode key,
                      const std::vector<ModifierKey>& modifiers)
{
  if (!enabled || !IsLayoutKey(key))
    return;
  requested_keys[static_cast<int>(key)] = true;
#if defined(_KEYBOARD_LAYOUT_DIAGNOSTICS)
  if (diagnostics) {
    shortcuts.push_back({std::string(name), std::string(chord), PreviewModifierTokens(modifiers), key});
  }
#else
  (void)name;
  (void)chord;
  (void)modifiers;
#endif
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
  return bindings.Resolve(configured, frame_count, Key::Pressed);
}

ResolvedChord ResolveChord(KeyCode configured)
{
  const auto key = Resolve(configured);
  return {key, enabled && IsLayoutKey(configured)
                   && required_shift[static_cast<int>(configured)]};
}

} // namespace keyboard_layout
