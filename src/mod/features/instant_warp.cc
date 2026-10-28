// [ui] auto_confirm_instant_warp and the instant_warp_* ship filters: answers the warp-or-jump prompt for the ship,
// previews that answer on the system card's visit button, and the shortcut that cycles the default.

#include "config.h"
#include "features/features.h"
#include "runtime_config.h"
#include "settings/basic_gameplay_settings.h"
#include "text.h"
#include "warp_click_intent.h"

#include "game/fleet.h"
#include "game/navigation.h"
#include "game/ui.h"
#include "game/viewers.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>
#include <unity/tracker.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
// Indexed by InstantWarpConfirmation, for the log.
constexpr auto& kInstantWarpNames = gameplay_settings::warp_names;

WarpClickIntent warp_click_intent;

template <class T> il2cpp::GcHandle InstantWarpRoot(const il2cpp::CallResult<T*>& observed)
{
  auto result = observed && observed.value ? il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(observed.value))
                                           : il2cpp::GcHandle{};
  if constexpr (requires { T::Class::Raw(); }) {
    if (result && !result.template Get<il2cpp::Object>()->template Is<T>())
      result.Reset();
  }
  return result;
}

bool WarpChoiceModifierHeld()
{
  unity::Key::Poll();
#if __APPLE__
  return unity::Key::Pressed(unity::KeyCode::LeftCommand) || unity::Key::Pressed(unity::KeyCode::RightCommand);
#else
  return unity::Key::Pressed(unity::KeyCode::LeftControl) || unity::Key::Pressed(unity::KeyCode::RightControl);
#endif
}

void NavigationManager_OnMoveFleetAction_ChoiceHook(auto original, NavigationManager* _this,
                                                    il2cpp::Nullable<unity::Vector3> position, int64_t node_id,
                                                    int64_t fleet_id, bool force_move)
{
  const bool ask = WarpChoiceModifierHeld() && (unity::Input::GetMouseButton(0) || unity::Input::GetMouseButtonUp(0));
  // Clear even for ordinary/invalid moves, before any managed call can reenter.
  warp_click_intent.Begin(ask, fleet_id, node_id);
  if (ask && fleet_id == -1) {
    auto       manager  = il2cpp::GcHandle::Strong(FleetsManager::Instance());
    const auto selected = manager ? manager.Get<FleetsManager>()->TryCall<"GetSelectedFleetData", FleetPlayerData*>()
                                  : il2cpp::CallResult<FleetPlayerData*>{};
    auto       fleet    = il2cpp::GcHandle::Strong(selected ? selected.value : nullptr);
    const auto id       = fleet ? planned_warp_binding::Scalar<"get_Id", int64_t>(fleet.Get<FleetPlayerData>())
                                : il2cpp::CallResult<int64_t>{};
    warp_click_intent.Begin(true, id ? id.value : 0, node_id);
  }
  original(_this, position, node_id, fleet_id, force_move);
}

bool ConsumeWarpChoice(CourseData* course)
{
  // Detach the intent before acquiring managed references or invoking getters.
  auto pending = std::exchange(warp_click_intent, {});
  if (!pending.fleet || pending.unmatched)
    return pending.Consume(false, 0, 0);
  auto       root   = il2cpp::GcHandle::Strong(course);
  const auto fleet  = root ? root.Get<CourseData>()->TryFleetId() : il2cpp::CallResult<int64_t>{};
  const auto target = root ? root.Get<CourseData>()->TryTarget() : il2cpp::CallResult<CourseNode*>{};
  auto       node   = il2cpp::GcHandle::Strong(target ? target.value : nullptr);
  const auto id     = node ? node.Get<CourseNode>()->TryId() : il2cpp::CallResult<int64_t>{};
  return pending.Consume(fleet && id, fleet ? fleet.value : 0, id ? id.value : 0);
}

// --- Course prompt ---

// The action auto-confirm takes for `fleet`: always-ask wins, then auto-jump, then auto-warp, then the default.
InstantWarpConfirmation ResolveConfirmation(FleetPlayerData* fleet)
{
  auto source = fleet ? il2cpp::GcHandle::Strong(fleet) : il2cpp::GcHandle{};
  if (!source)
    return InstantWarpConfirmation::kNone;
  const auto  name    = source.Get<FleetPlayerData>()->DisplayName();
  const auto& ui      = Config::Get().ui;
  const auto  words   = ShipNameWords(name);
  const auto  matches = [&words, &name](const ShipFilter& filter) -> std::optional<bool> {
    if (filter.all)
      return true;
    if (filter.names.empty())
      return false;
    // A named policy cannot be ruled out using an unavailable localization.
    if (name.empty())
      return std::nullopt;
    return std::ranges::any_of(filter.names, [&](const auto& pattern) { return ShipNameMatches(words, pattern); });
  };
  for (const auto [filter, action] : {std::pair{&ui.instant_warp_always_ask, InstantWarpConfirmation::kNone},
                                      std::pair{&ui.instant_warp_auto_jump, InstantWarpConfirmation::kJump},
                                      std::pair{&ui.instant_warp_auto_warp, InstantWarpConfirmation::kWarp}}) {
    const auto match = matches(*filter);
    if (!match)
      return InstantWarpConfirmation::kNone;
    if (*match)
      return action;
  }
  return ui.auto_confirm_instant_warp;
}

void CoursePromptPopupViewController_AboutToShow_Hook(auto original, CoursePromptPopupViewController* _this)
{
  original(_this);
  auto owner = il2cpp::GcHandle::Strong(_this);
  auto widget =
      owner ? InstantWarpRoot(owner.Get<CoursePromptPopupViewController>()->TryPopupWidget()) : il2cpp::GcHandle{};
  auto context =
      widget ? InstantWarpRoot(
                   widget.Get<CoursePromptPopupWidget>()->TryCall<"get_Context", CoursePromptPopupDataContext*>())
             : il2cpp::GcHandle{};
  auto course =
      context ? InstantWarpRoot(context.Get<CoursePromptPopupDataContext>()->TryCourseData()) : il2cpp::GcHandle{};
  const bool ask = ConsumeWarpChoice(course.Get<CourseData>());
  const auto instant =
      context ? context.Get<CoursePromptPopupDataContext>()->TryHasInstantWarp() : il2cpp::CallResult<bool>{};
  if (ask || !instant || !instant.value || !course)
    return;
  auto fleet = InstantWarpRoot(course.Get<CourseData>()->TryCall<"get_PlayerFleet", FleetPlayerData*>());
  if (!fleet)
    return;
  const auto action = ResolveConfirmation(fleet.Get<FleetPlayerData>());
  if (action == InstantWarpConfirmation::kNone)
    return;
  // Localization and managed getters can reenter. Never answer a different
  // popup, rebound context, course or fleet using the earlier decision.
  const auto current_widget = owner.Get<CoursePromptPopupViewController>()->TryPopupWidget();
  const auto current_context =
      widget.Get<CoursePromptPopupWidget>()->TryCall<"get_Context", CoursePromptPopupDataContext*>();
  const auto current_course  = context.Get<CoursePromptPopupDataContext>()->TryCourseData();
  const auto current_fleet   = course.Get<CourseData>()->TryCall<"get_PlayerFleet", FleetPlayerData*>();
  const auto current_instant = context.Get<CoursePromptPopupDataContext>()->TryHasInstantWarp();
  if (!current_widget || current_widget.value != widget.Get<CoursePromptPopupWidget>() || !current_context
      || current_context.value != context.Get<CoursePromptPopupDataContext>() || !current_course
      || current_course.value != course.Get<CourseData>() || !current_fleet
      || current_fleet.value != fleet.Get<FleetPlayerData>() || !current_instant || !current_instant.value)
    return;
  spdlog::debug("[InstantWarp] answering the prompt with {}", kInstantWarpNames[std::to_underlying(action)]);
  switch (action) {
    case InstantWarpConfirmation::kWarp:
      widget.Get<CoursePromptPopupWidget>()->InitiateRegularWarp();
      break;
    case InstantWarpConfirmation::kJump:
      widget.Get<CoursePromptPopupWidget>()->OnInstantWarpButtonClick();
      break;
    case InstantWarpConfirmation::kNone:
      break;
  }
}

// --- System card label ---
//
// The system card's visit button reads WARP or JUMP when auto-confirm would pick one. This runs on the frame tick
// rather than another UI detour, and only overrides the label text: never the button's context, timer, resource
// cost, or click handler.

std::optional<std::string> CurrentLabelText(TextLocalizer* label)
{
  const auto observed = label->TryCall<"CurrentText", Il2CppString*>();
  auto       root     = InstantWarpRoot(observed);
  return root ? std::optional{il2cpp::ToUtf8(reinterpret_cast<Il2CppString*>(root.Get()))} : std::nullopt;
}

// The label text this file set, so it is only cleared while it still reads that.
struct OwnedLabel {
  il2cpp::GcHandle handle;
  std::string      text;

  TextLocalizer* Get() const
  { return handle.Get<TextLocalizer>(); }

  void Clear()
  {
    auto       source   = Get() ? il2cpp::GcHandle::Strong(Get()) : il2cpp::GcHandle{};
    auto*      label    = source.Get<TextLocalizer>();
    const auto previous = std::exchange(text, {});
    handle.Reset();
    const auto overridden = unity::Alive(label) ? label->TryTextOverride() : il2cpp::CallResult<bool>{};
    if (!previous.empty() && overridden && overridden.value && CurrentLabelText(label) == previous)
      (void)label->TryCall<"ClearTextOverride", void>();
  }

  void Apply(TextLocalizer* label, const char* desired)
  {
    auto source = label ? il2cpp::GcHandle::Strong(label) : il2cpp::GcHandle{};
    if (!source || !desired) {
      Clear();
      return;
    }
    if (Get() != label) {
      Clear();
      // Don't take ownership from an existing text override.
      const auto overridden = label->TryTextOverride();
      if (!overridden || overridden.value)
        return;
      handle = il2cpp::GcHandle::Weak(label);
      if (!handle)
        return;
    }
    const auto overridden = label->TryTextOverride();
    const auto current    = CurrentLabelText(label);
    if (!overridden || !current || (!text.empty() && overridden.value && *current != text)
        || (text.empty() && overridden.value)) {
      Clear();
      return;
    }
    if (text == desired && overridden.value && *current == desired)
      return;
    auto* value      = il2cpp::NewString(desired);
    auto  value_root = value ? il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(value)) : il2cpp::GcHandle{};
    if (value_root
        && label->TryCall<"OverrideLocalizedText", void(Il2CppString*)>(
            reinterpret_cast<Il2CppString*>(value_root.Get())))
      text = desired;
    else
      Clear();
  }
};
// Never destroyed: freeing a GC handle in a static destructor can run after the runtime has shut down.
auto& action_label = *new OwnedLabel;

void InstantWarpFrame()
{
  // Ten times a second is quick enough for a label.
  static Throttle throttle(std::chrono::milliseconds(100));
  if (!throttle.Due())
    return;

  if (WarpChoiceModifierHeld()) {
    action_label.Clear(); // Native localized Set Course text while requesting a choice.
    return;
  }

  // Without a default or a jump/warp filter every prompt resolves to "ask"; skip the fleet lookups.
  const auto& ui    = Config::Get().ui;
  const auto  empty = [](const ShipFilter& filter) { return !filter.all && filter.names.empty(); };
  if (ui.auto_confirm_instant_warp == InstantWarpConfirmation::kNone && empty(ui.instant_warp_auto_jump)
      && empty(ui.instant_warp_auto_warp)) {
    action_label.Clear();
    return;
  }

  auto*       observed_widget = unity::Newest<StarNodeObjectViewerWidget>();
  auto        widget          = observed_widget ? il2cpp::GcHandle::Strong(observed_widget) : il2cpp::GcHandle{};
  auto        button          = widget && unity::Alive(widget.Get<StarNodeObjectViewerWidget>())
                                        && widget.Get<StarNodeObjectViewerWidget>()->isActiveAndEnabled()
                                    ? InstantWarpRoot(widget.Get<StarNodeObjectViewerWidget>()->TryVisitButton())
                                    : il2cpp::GcHandle{};
  const char* desired         = nullptr;
  // The ordinary visit path uses the currently selected fleet. The card's cached popup CourseData is only filled on
  // click, so it cannot preview. Toll and wormhole buttons confirm their own way and keep the game's text.
  if (button && unity::Alive(button.Get<GenericButtonWidget>())
      && button.Get<GenericButtonWidget>()->isActiveAndEnabled() && button.Get<GenericButtonWidget>()->Interactable()) {
    auto fleets     = InstantWarpRoot(FleetsManager::Class::TryCall<"get_Instance", FleetsManager*>());
    auto deployment = InstantWarpRoot(DeploymentManager::Class::TryCall<"get_Instance", DeploymentManager*>());
    auto fleet = fleets && unity::Alive(fleets.Get<FleetsManager>())
                     ? InstantWarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetSelectedFleetData", FleetPlayerData*>())
                     : il2cpp::GcHandle{};
    const auto has_instant =
        fleet && deployment && unity::Alive(deployment.Get<DeploymentManager>())
            ? deployment.Get<DeploymentManager>()->TryCall<"HasInstantWarpAbility", bool(FleetPlayerData*)>(
                  fleet.Get<FleetPlayerData>())
            : il2cpp::CallResult<bool>{};
    if (has_instant && has_instant.value) {
      switch (ResolveConfirmation(fleet.Get<FleetPlayerData>())) {
        case InstantWarpConfirmation::kWarp:
          desired = "WARP";
          break;
        case InstantWarpConfirmation::kJump:
          desired = "JUMP";
          break;
        case InstantWarpConfirmation::kNone:
          break;
      }
      const auto current = fleets.Get<FleetsManager>()->TryCall<"GetSelectedFleetData", FleetPlayerData*>();
      if (!current || current.value != fleet.Get<FleetPlayerData>())
        desired = nullptr;
    }
  }
  if (desired) {
    const auto current_button = widget.Get<StarNodeObjectViewerWidget>()->TryVisitButton();
    if (unity::Newest<StarNodeObjectViewerWidget>() != widget.Get<StarNodeObjectViewerWidget>() || !current_button
        || current_button.value != button.Get<GenericButtonWidget>())
      desired = nullptr;
  }
  auto label = desired ? InstantWarpRoot(button.Get<GenericButtonWidget>()->TryMainText()) : il2cpp::GcHandle{};
  action_label.Apply(unity::Alive(label.Get<TextLocalizer>()) ? label.Get<TextLocalizer>() : nullptr, desired);
}

bool InstallInstantWarp()
{
  if (!unity::Input::MouseButtonStateAvailable()) {
    spdlog::warn("[InstantWarp] modified-click input unavailable; native choice retained");
    return false;
  }
  il2cpp::HookSet hooks("InstantWarp");
  IL2CPP_STAGE(hooks, NavigationManager::MoveFleetActionMethod(), NavigationManager_OnMoveFleetAction_ChoiceHook);
  IL2CPP_STAGE(hooks, CoursePromptPopupViewController::Class::Method("AboutToShow", 0),
               CoursePromptPopupViewController_AboutToShow_Hook);
  return hooks.Commit();
}
} // namespace

void CycleInstantWarpConfirmation()
{
  auto& setting = Config::Get().ui.auto_confirm_instant_warp;
  static_assert(std::to_underlying(InstantWarpConfirmation::kNone) == 0
                && std::to_underlying(InstantWarpConfirmation::kWarp) == 1
                && std::to_underlying(InstantWarpConfirmation::kJump) == 2);
  if (!gameplay_settings::CycleWarp(setting, [](const gameplay_settings::Key& key, std::string value) {
        runtime_config::SaveSetting(key.section, key.name, std::move(value), key.delay);
      }))
    return;
  spdlog::info("[InstantWarp] answering prompts with {}", kInstantWarpNames[std::to_underlying(setting)]);
}

// Always installed: its only hook answers a prompt the player asked the mod to answer.
const Feature kInstantWarp{"InstantWarp", nullptr, InstallInstantWarp, InstantWarpFrame};
