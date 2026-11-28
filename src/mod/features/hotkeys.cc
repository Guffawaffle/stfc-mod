// The order the mod's keyboard shortcuts are handled in, each frame from the frame tick in mod.cc, and the hotkey
// switches: the mode toggles, restart and quit, Escape, and the shortcuts that change a setting. The other
// shortcuts live with their features: screens.cc, fleet_actions.cc, chat.cc, camera.cc.

#include "config.h"
#include "features/features.h"
#include "features/fleet_watch.h"
#include "frame.h"
#include "runtime_config.h"
#include "settings/basic_gameplay_settings.h"
#include "shortcuts.h"

#include "game/app.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>

#if _WIN32
#include <Windows.h>
#endif

namespace
{
// The frame tick dispatches shortcuts whether or not this feature installed; this says whether it did.
bool hotkeys_installed = false;
struct EscapePress {
  std::chrono::steady_clock::time_point when;
  int                                   window_ms;
};
std::optional<EscapePress> escape_first_press;
// Pair resets do not make an already observed key-down eligible again in the same input frame.
int32_t escape_down_frame = -1;

// A debug-only scalar sample helps distinguish text ownership, capture and stuck modifiers in intermittent
// reports. Reuse the dispatcher's focus observation; do not move focus queries ahead of native callbacks.
void TraceHotkeyInput(std::optional<il2cpp::CallResult<bool>> focus)
{
#ifdef _MODDBG
  if (!spdlog::should_log(spdlog::level::debug) || !unity::Input::anyKeyDown())
    return;
  static std::optional<std::chrono::steady_clock::time_point> last;
  const auto                                                  now = std::chrono::steady_clock::now();
  if (last && now - *last < std::chrono::seconds(2))
    return;
  last = now;
  spdlog::debug("[Hotkeys] input-state frame={} capture={} text_sampled={} text_known={} text_focused={} modifier={}",
                unity::Time::frameCount(), Shortcut::CaptureBlocksShortcuts(), focus.has_value(),
                focus && focus->succeeded, focus && focus->succeeded && focus->value, unity::Key::IsModified());
#endif
}

// --- Setting shortcuts ---

template <auto Section, auto Setting, gameplay_settings::Boolean Option> void ToggleSetting()
{
  auto& section = Config::Get().*Section;
  gameplay_settings::ToggleBoolean(section.*Setting, Option, [](const gameplay_settings::Key& key, bool value) {
    runtime_config::SaveSetting(key.section, key.name, value, key.delay);
  });
}

template <spdlog::level::level_enum Level> void SetLogLevel()
{
  spdlog::set_level(Level);
  spdlog::flush_on(Level);
}

struct SettingShortcut {
  Action shortcut;
  void (*run)();
  // Held shortcuts repeat every frame while the keys are down; the rest fire once per press.
  bool held = false;
};

// Shortcuts that change a setting. The game sees their keys too. The first pressed one runs.
constexpr SettingShortcut kSettingShortcuts[] = {
    {Action::kUiScaleUp, [] { AdjustUiScale(true); }, true},
    {Action::kUiScaleDown, [] { AdjustUiScale(false); }, true},
    {Action::kUiShipScaleUp, [] { AdjustUiShipScale(true); }, true},
    {Action::kUiShipScaleDown, [] { AdjustUiShipScale(false); }, true},
    {Action::kUiViewerScaleUp, [] { AdjustUiViewerScale(true); }, true},
    {Action::kUiViewerScaleDown, [] { AdjustUiViewerScale(false); }, true},
    {Action::kToggleAutoConfirmInstantWarp, CycleInstantWarpConfirmation},
    {Action::kTogglePreviewLocate,
     ToggleSetting<&Config::ui, &Config::Ui::disable_preview_locate, gameplay_settings::Boolean::PreviewLocate>},
    {Action::kTogglePreviewRecall,
     ToggleSetting<&Config::ui, &Config::Ui::disable_preview_recall, gameplay_settings::Boolean::PreviewRecall>},
    {Action::kToggleCargoDefault,
     ToggleSetting<&Config::ui, &Config::Ui::show_cargo_default, gameplay_settings::Boolean::CargoDefault>},
    {Action::kToggleCargoPlayer,
     ToggleSetting<&Config::ui, &Config::Ui::show_player_cargo, gameplay_settings::Boolean::CargoPlayer>},
    {Action::kToggleCargoStation,
     ToggleSetting<&Config::ui, &Config::Ui::show_station_cargo, gameplay_settings::Boolean::CargoStation>},
    {Action::kToggleCargoHostile,
     ToggleSetting<&Config::ui, &Config::Ui::show_hostile_cargo, gameplay_settings::Boolean::CargoHostile>},
    {Action::kToggleCargoArmada,
     ToggleSetting<&Config::ui, &Config::Ui::show_armada_cargo, gameplay_settings::Boolean::CargoArmada>},
    {Action::kLogLevelOff, SetLogLevel<spdlog::level::off>},
    {Action::kLogLevelError, SetLogLevel<spdlog::level::err>},
    {Action::kLogLevelWarn, SetLogLevel<spdlog::level::warn>},
    {Action::kLogLevelInfo, SetLogLevel<spdlog::level::info>},
    {Action::kLogLevelDebug, SetLogLevel<spdlog::level::debug>},
    {Action::kLogLevelTrace, SetLogLevel<spdlog::level::trace>},
    {Action::kReloadSettings, [] { Config::Reload(); }},
};

bool SettingShortcuts()
{
  for (const auto& setting : kSettingShortcuts) {
    if (setting.held ? Shortcut::IsPressed(setting.shortcut) : Shortcut::IsDown(setting.shortcut)) {
      setting.run();
      return true;
    }
  }
  return false;
}

// --- Escape exit ---

void ResetHotkeys()
{ escape_first_press.reset(); }

void RetireEscapeInput()
{
  ResetHotkeys();
  // Recovery or another input owner must not leave this down event eligible after a same-frame handoff.
  if (unity::Key::Down(unity::KeyCode::Escape))
    escape_down_frame = unity::Time::frameCount();
}

// Only eligible fresh Escape presses reach this comparison; idle/held frames do not sample the clock.
// Unlike ship locate's strict window, Escape accepts its inclusive endpoint and consumes a completed pair.
bool EscapeDoubleTapAt(std::chrono::steady_clock::time_point now, int window_ms)
{
  if (window_ms <= 0) {
    ResetHotkeys();
    return false;
  }
  if (!escape_first_press || escape_first_press->window_ms != window_ms || now < escape_first_press->when
      || now - escape_first_press->when > std::chrono::milliseconds(window_ms)) {
    escape_first_press = EscapePress{now, window_ms};
    return false;
  }
  // Consume the pair so a quick third press starts a new one.
  ResetHotkeys();
  return true;
}

// With the block enabled, only an eligible pair of distinct presses may reach the native exit handling.
bool SwallowEscape()
{
  const auto& config = Config::Get();
  if (!config.ui.disable_escape_exit) {
    RetireEscapeInput();
    return false;
  }
  if (config.ui.disable_escape_exit_timer <= 0) {
    RetireEscapeInput();
    return unity::Key::Pressed(unity::KeyCode::Escape);
  }
  if (escape_first_press && escape_first_press->window_ms != config.ui.disable_escape_exit_timer)
    ResetHotkeys();
  if (!unity::Key::Pressed(unity::KeyCode::Escape))
    return false;
  // A held key stays swallowed; only distinct presses count towards a double tap.
  if (!unity::Key::Down(unity::KeyCode::Escape))
    return true;
  const auto frame = unity::Time::frameCount();
  if (frame < 0) {
    ResetHotkeys();
    return true;
  }
  if (frame == escape_down_frame)
    return true;
  escape_down_frame = frame;
  return !EscapeDoubleTapAt(std::chrono::steady_clock::now(), config.ui.disable_escape_exit_timer);
}

// --- Settings during tutorials ---

struct SettingsTutorialRequest {
  il2cpp::GcHandle sections, manager, loader, controller, mask;
  SectionID        origin = SectionID::AppInit;
  bool             active = false, blocking = false, touch = false, mask_active = false;
  bool             owns_flags = false, owns_mask = false, released = false, entered = false, restoring = false;
  int              frames = 300;
};
// Operation-local shared custody survives a managed callback cancelling the request. Terminal static destruction
// must not touch IL2CPP; the feature reset only forgets this old session's custody, without invoking game objects.
auto& settings_tutorial_request   = *new std::shared_ptr<SettingsTutorialRequest>;
bool  settings_tutorial_installed = false, settings_tutorial_busy = false;

struct SettingsTutorialCall {
  SettingsTutorialCall()
  { settings_tutorial_busy = true; }
  ~SettingsTutorialCall()
  { settings_tutorial_busy = false; }
};

bool SettingsTutorialCurrent(const std::shared_ptr<SettingsTutorialRequest>& request)
{ return settings_tutorial_request == request && !ModSessionSuspended(); }

bool SettingsTutorialDelivery()
{
  return std::ranges::any_of(FeatureInstallations(), [](const auto& feature) {
    return feature.name && std::string_view(feature.name) == "SettingsTutorialInput" && feature.frame_ready;
  });
}

void ForgetSettingsTutorialInput()
{ settings_tutorial_request.reset(); }

// Unavailable observations retain custody for a later retry. A confirmed replacement abandons old state; it
// cannot authorize writing prior tutorial flags into the replacement manager or enabling a replacement mask.
il2cpp::CallResult<bool> SettingsTutorialOwners(const std::shared_ptr<SettingsTutorialRequest>& request)
{
  const auto sections = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  const auto manager  = Hub::Class::TryCall<"get_TutorialManager", TutorialManager*>();
  if (!SettingsTutorialCurrent(request))
    return {true, false};
  if (!sections || !manager)
    return {};
  if (sections.value != request->sections.Get() || manager.value != request->manager.Get() || !manager.value
      || manager.value->MaskLoader() != request->loader.Get())
    return {true, false};
  const auto controller = request->loader.Get<TutorialMaskUILoader>()->TryController();
  if (!SettingsTutorialCurrent(request))
    return {true, false};
  if (!controller)
    return {};
  if (controller.value != request->controller.Get() || !controller.value)
    return {true, false};
  const auto mask            = controller.value->TryCall<"get_gameObject", unity::GameObject*>();
  const auto alive           = unity::TryAlive(request->mask.Get());
  const auto latest_sections = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  const auto latest_manager  = Hub::Class::TryCall<"get_TutorialManager", TutorialManager*>();
  if (!SettingsTutorialCurrent(request))
    return {true, false};
  if (!mask || !alive || !latest_sections || !latest_manager)
    return {};
  return {true, mask.value == request->mask.Get() && alive.value && latest_sections.value == sections.value
                    && latest_manager.value == manager.value && manager.value->MaskLoader() == request->loader.Get()};
}

void RestoreSettingsTutorialInput(const std::shared_ptr<SettingsTutorialRequest>& request)
{
  request->restoring = true;
  request->released  = false;
  const auto owners  = SettingsTutorialOwners(request);
  if (!owners)
    return;
  if (!owners.value) {
    if (SettingsTutorialCurrent(request))
      ForgetSettingsTutorialInput();
    return;
  }
  // Only undo values we wrote. A native owner that already raised a gate keeps its current restriction.
  if (request->owns_flags) {
    if (!TutorialManager::Active())
      TutorialManager::SetActive(request->active);
    if (!TutorialManager::BlockingInput())
      TutorialManager::SetBlockingInput(request->blocking);
    if (!TouchKit::BlockTouch())
      TouchKit::SetBlockTouch(request->touch);
    request->owns_flags = false;
  }
  if (request->owns_mask) {
    const auto active  = request->mask.Get<unity::GameObject>()->TryCall<"get_activeSelf", bool>();
    const auto current = SettingsTutorialOwners(request);
    if (!SettingsTutorialCurrent(request) || !active || !current)
      return;
    if (!current.value) {
      ForgetSettingsTutorialInput();
      return;
    }
    if (!active.value
        && !request->mask.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(request->mask_active))
      return; // A throw after native activation is resolved by observing activeSelf on the next frame.
  }
  if (SettingsTutorialCurrent(request))
    ForgetSettingsTutorialInput();
}

void SettingsTutorialInputFrame()
{
  const auto request = settings_tutorial_request;
  if (!request || settings_tutorial_busy || ModSessionSuspended())
    return;
  SettingsTutorialCall call;
  if (!request->owns_flags && !request->restoring) {
    ForgetSettingsTutorialInput();
    return; // A preparation exception never leaves a partially captured request eligible for frame work.
  }
  const auto owners = SettingsTutorialOwners(request);
  if (!owners)
    return;
  if (!owners.value) {
    if (SettingsTutorialCurrent(request))
      ForgetSettingsTutorialInput();
    return;
  }
  const auto section = request->sections.Get<SectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  if (!section || !SettingsTutorialCurrent(request))
    return;
  if (section.value == SectionID::GameSettings)
    request->entered = true;
  else if (request->entered || section.value != request->origin || --request->frames == 0)
    request->restoring = true;
  // A native reassertion ends this override rather than repeatedly clearing another input owner's restriction.
  if (request->restoring || TutorialManager::Active() || TutorialManager::BlockingInput() || TouchKit::BlockTouch())
    RestoreSettingsTutorialInput(request);
}

bool TutorialManager_get_IsBlockingInput_Hook(auto original)
{
  const auto request = settings_tutorial_request;
  if (!request || !request->released || settings_tutorial_busy || ModSessionSuspended())
    return original();
  SettingsTutorialCall call;
  const auto           section = request->sections.Get<SectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  const auto           owners  = SettingsTutorialOwners(request);
  if (SettingsTutorialCurrent(request) && section && owners && owners.value
      && (section.value == SectionID::GameSettings || (!request->entered && section.value == request->origin))) {
    // Managed owner observations can reassert a gate between frame delivery and this getter.
    if (TutorialManager::Active() || TutorialManager::BlockingInput() || TouchKit::BlockTouch()) {
      request->released  = false;
      request->restoring = true;
      return original();
    }
    return false;
  }
  return original();
}

bool InstallSettingsTutorialInput()
{
  il2cpp::HookSet hooks("SettingsTutorialInput");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(TutorialManager::Class::StaticMethod<bool()>("get_IsBlockingInput")),
                  TutorialManager_get_IsBlockingInput_Hook);
  settings_tutorial_installed = hooks.Commit();
  return settings_tutorial_installed;
}

// --- Scopely hotkeys ---

// With Scopely hotkeys off, the game never registers its own input actions, so its shortcuts cannot fire.
void ShortcutsManager_InitializeActions_Hook(auto original, ShortcutsManager* _this)
{
  if (Config::Get().control.use_scopely_hotkeys)
    original(_this);
}

// Native popup confirmation is independent of the configurable shortcut dispatcher. In particular, Space and
// Enter must stay with a focused text field instead of also activating a reward window's confirm button.
void KeyboardConfirmHandler_OnConfirmPerformed_Hook(auto original, KeyboardConfirmHandler* _this,
                                                    unity::InputActionCallbackContext context)
{
  unity::Key::Poll();
  if (Shortcut::CaptureBlocksShortcuts() || unity::Key::IsInputFocused())
    return;
  original(_this, context);
}

bool InstallKeyboardConfirm()
{
  il2cpp::HookSet hooks("KeyboardConfirm");
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(KeyboardConfirmHandler::Class::Method<void(unity::InputActionCallbackContext)>(
                      "OnConfirmPerformed")),
                  KeyboardConfirmHandler_OnConfirmPerformed_Hook);
  return hooks.Commit();
}

bool InstallHotkeys()
{
  il2cpp::HookSet hooks("Hotkeys");
  IL2CPP_STAGE(hooks, ShortcutsManager::Class::Method("InitializeActions"), ShortcutsManager_InitializeActions_Hook);
  // The badges only make sense when the game's own input actions are under the mod's control.
  if (hooks.Commit())
    InstallKeybindBadges();
  InstallFleetActions();
#ifdef _MODDBG
  fleet_watch::InstallRuntimeProbe();
#endif
  // The dispatch itself needs no hook: the frame tick runs it. The hint badges and pre-scan cargo are extras whose
  // own sets report themselves.
  hotkeys_installed = true;
  return true;
}
} // namespace

// Called only by the accepted Settings screen action, after the ordinary typing/capture matcher has run.
void ShowSettings()
{
  if (settings_tutorial_busy || ModSessionSuspended())
    return;
  if (!settings_tutorial_installed || !SettingsTutorialDelivery()) {
    Hub::GotoSection(SectionID::GameSettings);
    return;
  }
  SettingsTutorialCall call;
  if (settings_tutorial_request)
    return; // One transition owns the saved gates, including repeated Settings presses.
  auto request              = std::make_shared<SettingsTutorialRequest>();
  settings_tutorial_request = request;
  const auto sections       = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  if (!sections || !sections.value || !SettingsTutorialCurrent(request)) {
    if (SettingsTutorialCurrent(request))
      ForgetSettingsTutorialInput();
    return;
  }
  request->sections = il2cpp::GcHandle::Strong(sections.value);
  const auto origin = sections.value->TryCall<"get_CurrentSection", SectionID>();
  if (!request->sections || !origin || !SettingsTutorialCurrent(request)) {
    if (SettingsTutorialCurrent(request))
      ForgetSettingsTutorialInput();
    return;
  }
  request->origin    = origin.value;
  const auto capture = [&] {
    const auto manager = Hub::Class::TryCall<"get_TutorialManager", TutorialManager*>();
    if (!manager || !manager.value || !SettingsTutorialCurrent(request) || !TutorialManager::HasInputFields()
        || !TouchKit::HasInputField() || !SettingsTutorialCurrent(request))
      return false;
    request->manager = il2cpp::GcHandle::Strong(manager.value);
    request->loader  = il2cpp::GcHandle::Strong(manager.value->MaskLoader());
    if (!request->manager || !request->loader || !TutorialManager::Active())
      return false;
    const auto controller = request->loader.Get<TutorialMaskUILoader>()->TryController();
    if (!controller || !controller.value || !SettingsTutorialCurrent(request))
      return false;
    request->controller = il2cpp::GcHandle::Strong(controller.value);
    const auto mask     = controller.value->TryCall<"get_gameObject", unity::GameObject*>();
    if (!mask || !mask.value || !SettingsTutorialCurrent(request))
      return false;
    request->mask     = il2cpp::GcHandle::Strong(mask.value);
    const auto active = mask.value->TryCall<"get_activeSelf", bool>();
    const auto owners = SettingsTutorialOwners(request);
    if (!request->controller || !request->mask || !active || !owners || !owners.value)
      return false;
    request->active      = TutorialManager::Active();
    request->blocking    = TutorialManager::BlockingInput();
    request->touch       = TouchKit::BlockTouch();
    request->mask_active = active.value;
    return request->active && SettingsTutorialCurrent(request);
  };
  if (capture()) {
    request->owns_flags = true;
    TutorialManager::SetActive(false);
    TutorialManager::SetBlockingInput(false);
    TouchKit::SetBlockTouch(false);
    request->released = true;
    if (request->mask_active) {
      request->owns_mask = true;
      const auto hidden  = request->mask.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(false);
      if (!SettingsTutorialCurrent(request))
        return;
      const auto owners = SettingsTutorialOwners(request);
      if (!hidden || !owners || !owners.value) {
        RestoreSettingsTutorialInput(request);
        return;
      }
    }
  }
  if (!SettingsTutorialCurrent(request))
    return;
  const auto current_sections = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  if (!current_sections || current_sections.value != request->sections.Get() || !SettingsTutorialCurrent(request)) {
    if (request->owns_flags)
      RestoreSettingsTutorialInput(request);
    else if (SettingsTutorialCurrent(request))
      ForgetSettingsTutorialInput();
    return;
  }
  const auto current_section = request->sections.Get<SectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  const auto current_manager = Hub::Class::TryCall<"get_TutorialManager", TutorialManager*>();
  const auto owners          = request->mask ? SettingsTutorialOwners(request) : il2cpp::CallResult<bool>{true, true};
  if (!SettingsTutorialCurrent(request))
    return;
  if (!current_section || current_section.value != request->origin
      || (request->manager && (!current_manager || current_manager.value != request->manager.Get())) || !owners
      || !owners.value) {
    if (request->owns_flags)
      RestoreSettingsTutorialInput(request);
    else
      ForgetSettingsTutorialInput();
    return;
  }
  const auto opened = request->sections.Get<SectionManager>()->TryCall<"TriggerSectionChange", bool>(
      SectionID::GameSettings, static_cast<void*>(nullptr), false, false, true);
  if (!SettingsTutorialCurrent(request))
    return;
  if (!request->owns_flags)
    ForgetSettingsTutorialInput(); // Ordinary Settings navigation never acquires tutorial input ownership.
  else if (!opened || !opened.value)
    RestoreSettingsTutorialInput(request);
}

bool HandleShortcuts()
{
  // Capture also owns mode toggles, awaited buttons, ship selection and Escape. Let native UI keep its Update.
  if (Shortcut::CaptureBlocksShortcuts()) {
    RetireEscapeInput();
    TraceHotkeyInput(std::nullopt);
    CancelAwaitedButton();
    return false;
  }
  if (!hotkeys_installed) {
    RetireEscapeInput();
    return false;
  }
  if (Shortcut::IsDown(Action::kDisableHotKeys, Shortcut::TextInputPolicy::AllowKeyboard)) {
    RetireEscapeInput();
    HideKeybindBadges();
    gameplay_settings::SetBoolean(Config::Get().control.hotkeys_enabled, false, gameplay_settings::Boolean::Hotkeys,
                                  [](const gameplay_settings::Key& key, bool value) {
                                    runtime_config::SaveSetting(key.section, key.name, value, key.delay);
                                  });
    spdlog::warn("[Hotkeys] mod hotkeys off");
    return true;
  }
  if (Shortcut::IsDown(Action::kEnableHotKeys, Shortcut::TextInputPolicy::AllowKeyboard)) {
    RetireEscapeInput();
    gameplay_settings::SetBoolean(Config::Get().control.hotkeys_enabled, true, gameplay_settings::Boolean::Hotkeys,
                                  [](const gameplay_settings::Key& key, bool value) {
                                    runtime_config::SaveSetting(key.section, key.name, value, key.delay);
                                  });
    spdlog::warn("[Hotkeys] mod hotkeys on");
    return true;
  }

  if (PressAwaitedButton())
    return true;

  // Switched off, or with Scopely's (the game's own) hotkeys, the mod leaves the keys to the game. Returning true
  // here would skip the game's ScreenManager.Update, which also runs the back button, orientation and the rest of
  // the UI's per-frame work.
  // Awaited native button callbacks can reload (and replace) Config.
  if (!Config::Get().control.hotkeys_enabled || Config::Get().control.use_scopely_hotkeys) {
    RetireEscapeInput();
    return false;
  }

  // Keys typed into a text field (chat, search) are text, not shortcuts.
  const auto focus  = unity::Key::TryIsInputFocused();
  const bool typing = !focus || focus.value;
  TraceHotkeyInput(focus);
  if (!typing && Shortcut::IsDown(Action::kToggleShortcutHints) && ToggleKeybindBadges())
    return true;

  if (!typing && Shortcut::IsDown(Action::kRestart)) {
    spdlog::info("[Hotkeys] clearing the localisation cache and restarting");
    LanguageManager::ClearCache();
    if (auto* app = Hub::App())
      app->Reload();
    return true;
  }
  if (!typing && Shortcut::IsDown(Action::kQuit)) {
    // On Windows the process ends here: TerminateProcess does not return.
#if _WIN32
    runtime_config::ForceClose();
#else
    if (auto* app = Hub::App())
      app->Quit();
#endif
    return true;
  }

  if (SelectShipShortcuts())
    return true;

  // The game only clears a focused text field from its back button; Escape should too.
  const bool in_chat = Hub::IsInChat();
  if (typing || in_chat)
    RetireEscapeInput();
  if (unity::Key::Pressed(unity::KeyCode::Escape) && (typing || in_chat)) {
    const auto cleared = unity::Key::ClearInputFocus();
    // Unknown focus or a failed clear must leave native Escape recovery available. Keep the existing full-chat
    // handling when text focus is known absent; do not fall through to the mod's exit-key suppression.
    return cleared && (cleared.value || in_chat);
  }

  if (in_chat) {
    if (ChatChannelShortcuts())
      return true;
  } else if (!typing) {
    if (FleetShortcuts())
      return true;
    OpenChatShortcut();
    if (ScreenShortcuts())
      return true;
    // Frozen PLAY puts Manage Ships after the entire settings else-if chain.
    // Both keep native Update; a shared chord must still execute only the first.
    if (!SettingShortcuts())
      LateScreenShortcut();
  }

  return FleetActionShortcuts() || SwallowEscape();
}

const Feature kHotkeys{"Hotkeys", &Config::Patches::hotkeyhooks, InstallHotkeys,
                       nullptr,   InstallRequirement::kOptional, ResetHotkeys};
const Feature kKeyboardConfirm{"KeyboardConfirm", &Config::Patches::hotkeyhooks, InstallKeyboardConfirm};
const Feature kSettingsTutorialInput{"SettingsTutorialInput",       &Config::Patches::hotkeyhooks,
                                     InstallSettingsTutorialInput,  SettingsTutorialInputFrame,
                                     InstallRequirement::kOptional, ForgetSettingsTutorialInput};
