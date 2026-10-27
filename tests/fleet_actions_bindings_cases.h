#pragma once

// Production fleet-action helper; only the native fleet method body and base-card decision are substituted.
#include "../src/mod/features/fleet_actions.cc"

namespace
{
FleetState                                         fleet_native_state          = FleetState::Docked;
bool                                               fleet_native_default_result = true;
std::vector<ActionBehaviour>                       fleet_native_requests;
std::vector<Action>                                fleet_native_keys;
std::map<Il2CppClass*, std::vector<Il2CppObject*>> fleet_native_widgets;
std::string                                        fleet_native_saved_section, fleet_native_saved_key;
bool                                               fleet_native_saved_value = true;
int64_t                                            fleet_native_target      = 100;
int                                                fleet_native_mines       = 0;
struct FleetNativeVisibility {
  Il2CppObject    header;
  VisibilityState state = VisibilityState::Visible;
};
struct FleetNativeViewer {
  Il2CppObject           header;
  FleetNativeVisibility* visibility = nullptr;
};
void FleetNativeMine(MiningObjectViewerWidget*, const MethodInfo*)
{ ++fleet_native_mines; }
void FleetNativeHide(FleetNativeViewer* viewer, const MethodInfo*)
{ viewer->visibility->state = VisibilityState::Hidden; }
int64_t FleetNativeTarget(BattleTargetData*, const MethodInfo*)
{ return fleet_native_target; }
FleetState FleetNativeState(FleetPlayerData*, const MethodInfo*)
{ return fleet_native_state; }
bool FleetNativeRequest(FleetLocalViewController*, il2cpp::Object*, ActionType, int64_t, ActionBehaviour behaviour,
                        void*, const MethodInfo*)
{
  fleet_native_requests.push_back(behaviour);
  return behaviour == ActionBehaviour::Default ? fleet_native_default_result : true;
}
void FleetNativeCases()
{
  auto& panel  = Define("Digit.Prime.Ships", "FleetLocalViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& state  = Define("Digit.PrimeServer.Models", "FleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& action = Define("Digit.PrimeServer.Models", "ActionType", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(ActionType));
  auto& behaviour = Define("Digit.PrimeServer.Models", "ActionBehaviour", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  state.is_enum = behaviour.is_enum = true;
  const auto& current =
      AddMethod(fleet_data_class, "get_CurrentState", reinterpret_cast<void*>(&FleetNativeState), T(state), {});
  AddProperty(fleet_data_class, "CurrentState", &current);
  AddMethod(panel, "RequestAction", reinterpret_cast<void*>(&FleetNativeRequest), T(bool_class),
            {T(object_class), T(action), T(long_class), T(behaviour), T(object_class)});
  Il2CppObject         object{&panel.klass};
  auto*                owner = reinterpret_cast<FleetLocalViewController*>(&object);
  auto*                fleet = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
  constexpr std::array repairable{FleetState::Docked, FleetState::Destroyed};
  Require(RequestFleetAction(owner, fleet, ActionType::Repair, repairable, FleetState::Repairing)
              && fleet_native_requests == std::vector{ActionBehaviour::Default},
          "successful native repair does not submit an additional help action absent from frozen source");
  fleet_native_requests.clear();
  fleet_native_state = FleetState::Repairing;
  Require(RequestFleetAction(owner, fleet, ActionType::Repair, repairable, FleetState::Repairing)
              && fleet_native_requests == std::vector{ActionBehaviour::AskHelp},
          "already repairing requests alliance help through the original native action");
  fleet_native_requests.clear();
  fleet_native_state = FleetState::IdleInSpace;
  Require(!RequestFleetAction(owner, fleet, ActionType::Repair, repairable, FleetState::Repairing)
              && fleet_native_requests.empty(),
          "repair outside supported fleet states sends no action");
  fleet_native_state          = FleetState::Destroyed;
  fleet_native_default_result = false;
  Require(!RequestFleetAction(owner, fleet, ActionType::Repair, repairable, FleetState::Repairing)
              && fleet_native_requests == std::vector{ActionBehaviour::Default},
          "native rejection remains rejection and does not submit help for a nonrepairing fleet");
  Config::Get().control.queue_enabled = true;
  fleet_native_keys                   = {Action::kToggleQueue};
  recovery_requests.Remember(fleet, {.valid = true, .fleet = 999, .front = 100, .count = 1}, Clock::now());
  const auto roots = warp_roots.size();
  Require(FleetShortcuts() && !Config::Get().control.queue_enabled && fleet_native_saved_section == "control"
              && fleet_native_saved_key == "queue_enabled" && !fleet_native_saved_value
              && warp_roots.size() + 1 == roots,
          "queue shortcut uses registered persistence and retires old recovery identity");
  int        queue_original = 0;
  const auto native_queue   = [&](auto*) {
    ++queue_original;
    return true;
  };
  Require(!ActionQueueManager_IsQueueUnlocked_Hook(native_queue, nullptr) && queue_original == 0,
          "queue disable leaves native unlock uncalled");
  Config::Get().control.queue_enabled = true;
  Require(ActionQueueManager_IsQueueUnlocked_Hook(native_queue, nullptr) && queue_original == 1,
          "queue enable preserves native unlock result");
  Require(!ActionQueueManager_IsQueueUnlocked_Hook([](auto*) { return false; }, nullptr),
          "mod cannot unlock a natively locked queue");
  fleet_native_keys.clear();
  auto& target      = Define("Digit.PrimeServer.Models", "BattleTargetData", IL2CPP_TYPE_CLASS, &object_class);
  auto& viewer_base = Define("Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& visibility  = Define("Digit.Client.UI", "VisibilityController", IL2CPP_TYPE_CLASS, &object_class);
  auto& viewer      = Define("Digit.Prime.Combat", "PreScanTargetWidget", IL2CPP_TYPE_CLASS, &viewer_base);
  auto& mine        = Define("Digit.Prime.ObjectViewer", "MiningObjectViewerWidget", IL2CPP_TYPE_CLASS, &viewer_base);
  AddField(viewer_base, "_visibilityController", visibility, offsetof(FleetNativeViewer, visibility));
  AddField(visibility, "_state", int_class, offsetof(FleetNativeVisibility, state));
  AddMethod(mine, "MineClicked", reinterpret_cast<void*>(&FleetNativeMine), T(void_class), {});
  AddMethod(viewer_base, "HideAllViewers", reinterpret_cast<void*>(&FleetNativeHide), T(void_class), {});
  AddMethod(target, "get_TargetID", reinterpret_cast<void*>(&FleetNativeTarget), T(long_class), {});
  Il2CppObject target_object{&target.klass}, viewer_object{&viewer.klass};
  auto*        context     = reinterpret_cast<BattleTargetData*>(&target_object);
  auto*        card        = reinterpret_cast<PreScanTargetWidget*>(&viewer_object);
  input_frame              = 100;
  const auto before_replay = warp_roots.size();
  DeferFleetPress({.queue = true}, fleet, card, context);
  Require(pending_replay && pending_replay->press.queue && !pending_replay->press.primary
              && warp_roots.size() == before_replay + 3,
          "deferred queue input keeps its action and owns the exact fleet/card/target");
  Require(FleetReplayMatches(*pending_replay, 101, fleet, card, context, 999, 100),
          "matching original source is eligible only on the next frame");
  Require(!FleetReplayMatches(*pending_replay, 102, fleet, card, context, 999, 100)
              && !FleetReplayMatches(*pending_replay, 101, fleet, card, context, 999, 101)
              && !FleetReplayMatches(*pending_replay, 101, fleet, card, context, 1000, 100)
              && !FleetReplayMatches(*pending_replay, 101, nullptr, card, context, 999, 100),
          "late input, same-object rebind, changed fleet and unavailable source cannot replay");
  input_frame = 102;
  RetireFleetReplay();
  Require(!pending_replay && warp_roots.size() == before_replay,
          "frame retirement releases requests even when shortcut dispatch was skipped");
  warp_root_failure = true;
  DeferFleetPress({.primary = true}, fleet, card, context);
  warp_root_failure = false;
  Require(!pending_replay && warp_roots.size() == before_replay, "failed custody cannot leave a deferred action");
  DeferFleetPress({.primary = true, .retry = true}, fleet, card, context);
  Require(!pending_replay, "a replay cannot schedule another replay");
  FleetNativeVisibility mine_visibility{{&visibility.klass}}, scan_visibility{{&visibility.klass}};
  FleetNativeViewer     mining_card{{&mine.klass}, &mine_visibility};
  FleetNativeViewer     scan_card{{&viewer.klass}, &scan_visibility};
  fleet_native_widgets[&mine.klass]                     = {&mining_card.header};
  Config::Get().control.primary_requires_visible_target = false;
  Require(ActOnPreScan(card, {.primary = true, .retry = true}, fleet, nullptr) && fleet_native_mines == 0,
          "saved pre-scan action cannot redirect into a newly shown mining overlay");
  fleet_native_widgets[&viewer.klass] = {&scan_card.header};
  Require(TargetViewers::HideFirst() && mine_visibility.state == VisibilityState::Hidden
              && scan_visibility.state == VisibilityState::Visible,
          "first Escape dismissal preserves source viewer-type precedence");
  Require(TargetViewers::HideFirst() && scan_visibility.state == VisibilityState::Hidden,
          "next dismissal reaches the underlying pre-scan instead of closing both layers");
  fleet_native_widgets.clear();
  std::cout << "Fleet production repair/helper request order and native rejection passed\n";
}
} // namespace

namespace base_card
{
bool KeepOpenForRecall(int64_t)
{ return false; }
} // namespace base_card

namespace Shortcut
{
bool IsDown(Action action)
{ return std::ranges::contains(fleet_native_keys, action); }
bool IsDownWithShift(Action action)
{ return IsDown(action); }
bool CaptureBlocksShortcuts() noexcept
{ return false; }
} // namespace Shortcut

namespace unity::detail
{
std::vector<Il2CppObject*> Instances(Il2CppClass* type)
{ return fleet_native_widgets[type]; }
Il2CppObject* Newest(Il2CppClass* type)
{
  const auto& values = fleet_native_widgets[type];
  return values.empty() ? nullptr : values.back();
}
} // namespace unity::detail

namespace mining_preview
{
bool Cycle()
{ return false; }
bool SuppressAutomaticCargo(RewardsButtonWidget*)
{ return false; }
} // namespace mining_preview

namespace runtime_config
{
void SaveSetting(const char* section, const char* key, config_edit::Value value, std::chrono::milliseconds) noexcept
{
  fleet_native_saved_section = section;
  fleet_native_saved_key     = key;
  fleet_native_saved_value   = std::get<bool>(value);
}
} // namespace runtime_config
