#pragma once

// Production fleet-action helper; only the native fleet method body and base-card decision are substituted.
#include "../src/mod/features/fleet_actions.cc"

namespace
{
FleetState                                         fleet_native_state          = FleetState::Docked;
bool                                               fleet_native_default_result = true;
std::vector<ActionBehaviour>                       fleet_native_requests;
std::vector<Action>                                fleet_native_keys;
bool fleet_native_primary_mouse = false;
std::map<Il2CppClass*, std::vector<Il2CppObject*>> fleet_native_widgets;
std::string                                        fleet_native_saved_section, fleet_native_saved_key;
bool                                               fleet_native_saved_value = true;
config_edit::Value                                 fleet_native_saved_any{true}, fleet_native_session_any{true};
int                                                fleet_native_save_calls = 0, fleet_native_session_calls = 0;
std::string                                        fleet_native_session_section, fleet_native_session_key;
bool                                               fleet_native_capture = false;
int64_t                                            fleet_native_target  = 100;
int                                                fleet_native_mines   = 0;
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
struct FleetMissionButton {
  Il2CppObject header;
  bool         interactable = true;
};
struct FleetMissionListener {
  Il2CppObject        header;
  FleetMissionButton* button = nullptr;
};
struct FleetMissionWidget {
  Il2CppObject          header;
  bool                  visible  = true;
  FleetMissionListener* listener = nullptr;
};
struct FleetMissionNavigation {
  Il2CppObject        header;
  FleetMissionWidget* mission = nullptr;
  FleetNativeViewer*  course  = nullptr;
};
bool fleet_mission_active  = true;
int  fleet_mission_presses = 0, fleet_normal_presses = 0;
bool FleetMissionActive(Il2CppObject*, const MethodInfo*)
{ return fleet_mission_active; }
SemaphoreButtonListener* FleetMissionListenerFor(FleetMissionWidget* widget, const MethodInfo*)
{ return reinterpret_cast<SemaphoreButtonListener*>(widget->listener); }
void FleetMissionPress(FleetMissionButton* button, const MethodInfo*)
{
  // Stand-in for the native Button.Press body, which owns its eligibility checks.
  if (button->interactable)
    ++fleet_mission_presses;
}
void FleetNormalPress(Il2CppObject*, const MethodInfo*)
{ ++fleet_normal_presses; }

struct FleetGuardList {
  Il2CppObject header{};
  int32_t      count = 0, reads = 0;
  bool         throws = false;
};
struct FleetGuardQueue {
  Il2CppObject    header{};
  int64_t         fleet   = 999;
  FleetGuardList* actions = nullptr;
};
struct FleetGuardVector {
  Il2CppObject                    header{};
  Il2CppArrayBounds*              bounds = nullptr;
  il2cpp_array_size_t             size   = 2;
  std::array<FleetGuardQueue*, 2> items{};
};
struct FleetGuardManager {
  Il2CppObject      header{};
  FleetGuardVector* queues = nullptr;
};
struct FleetGuardPanel {
  Il2CppObject     header{};
  FleetPlayerData* fleet = nullptr;
};
struct FleetGuardBar {
  Il2CppObject     header{};
  FleetGuardPanel* panel = nullptr;
};
struct FleetGuardTarget {
  Il2CppObject  header{};
  Il2CppObject* deployed = nullptr;
};
struct FleetGuardPreScan {
  Il2CppObject           header{};
  FleetNativeVisibility* visibility   = nullptr;
  Il2CppObject*          buttons      = nullptr;
  Il2CppObject*          queue_button = nullptr;
};
struct FleetGuardQueueButton {
  Il2CppObject          header{};
  FleetMissionListener* listener = nullptr;
};
struct FleetGuardCourse {
  Il2CppObject           header{};
  FleetNativeVisibility* visibility = nullptr;
  FleetMissionListener*  join       = nullptr;
};
FleetGuardManager* fleet_guard_manager      = nullptr;
FleetGuardList*    fleet_guard_actions      = nullptr;
FleetGuardTarget*  fleet_guard_target       = nullptr;
Il2CppObject*      fleet_guard_hull         = nullptr;
int32_t            fleet_guard_index        = 0;
bool               fleet_guard_index_throws = false;
int                fleet_guard_enqueues = 0, fleet_guard_clears = 0, fleet_guard_scans = 0;

int32_t FleetGuardCount(FleetGuardList* list, const MethodInfo*)
{
  ++list->reads;
  if (list->throws)
    throw Il2CppExceptionWrapper(&thrown);
  return list->count;
}
int32_t FleetGuardIndex(FleetPlayerData*, const MethodInfo*)
{
  if (fleet_guard_index_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return fleet_guard_index;
}
FleetPlayerData* FleetGuardSelected(FleetGuardPanel* panel, const MethodInfo*)
{ return panel->fleet; }
ActionQueueManager* FleetGuardInstance(const MethodInfo*)
{ return reinterpret_cast<ActionQueueManager*>(fleet_guard_manager); }
bool FleetGuardUnlocked(ActionQueueManager*, const MethodInfo*)
{ return true; }
bool FleetGuardFull(ActionQueueManager*, FleetPlayerData*, const MethodInfo*)
{ return false; }
void FleetGuardClear(ActionQueueManager*, FleetPlayerData*, const MethodInfo*)
{
  ++fleet_guard_clears;
  fleet_guard_actions->count = 0;
}
void FleetGuardEnqueue(FleetMissionButton* button, const MethodInfo*)
{
  if (button->interactable) {
    ++fleet_guard_enqueues;
    ++fleet_guard_actions->count;
  }
}
BattleTargetData* FleetGuardContext(Il2CppObject*, const MethodInfo*)
{ return reinterpret_cast<BattleTargetData*>(fleet_guard_target); }
HullSpec* FleetGuardHull(FleetDeployedData*, const MethodInfo*)
{ return reinterpret_cast<HullSpec*>(fleet_guard_hull); }
HullType FleetGuardHullType(HullSpec*, const MethodInfo*)
{ return HullType::Destroyer; }
void FleetGuardScan(ScanEngageButtonsWidget*, const MethodInfo*)
{ ++fleet_guard_scans; }

void FleetMissionCases()
{
  auto&       behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  const auto& active =
      AddMethod(behaviour, "get_isActiveAndEnabled", reinterpret_cast<void*>(&FleetMissionActive), T(bool_class), {});
  AddProperty(behaviour, "isActiveAndEnabled", &active);
  auto& button = Define("UnityEngine.UI", "Button", IL2CPP_TYPE_CLASS, &behaviour);
  AddMethod(button, "Press", reinterpret_cast<void*>(&FleetMissionPress), T(void_class), {});
  auto& listener = Define("Digit.Client.UI", "SemaphoreButtonListener", IL2CPP_TYPE_CLASS, &component_class);
  AddField(listener, "_button", button, offsetof(FleetMissionListener, button));
  auto& mission = Define("Digit.Prime.Navigation", "MissionLocationWidget", IL2CPP_TYPE_CLASS, &behaviour);
  AddField(mission, "_isVisible", bool_class, offsetof(FleetMissionWidget, visible));
  const auto& getter =
      AddMethod(mission, "get_SetCourseButton", reinterpret_cast<void*>(&FleetMissionListenerFor), T(listener), {});
  AddProperty(mission, "SetCourseButton", &getter);
  auto& visibility = Define("Digit.Client.UI", "VisibilityController", IL2CPP_TYPE_CLASS, &object_class);
  AddField(visibility, "_state", int_class, offsetof(FleetNativeVisibility, state));
  auto& course = Define("Digit.Prime.Navigation", "SetCourseWidget", IL2CPP_TYPE_CLASS, &behaviour);
  AddField(course, "_visibilityController", visibility, offsetof(FleetNativeViewer, visibility));
  auto& navigation =
      Define("Digit.Prime.Navigation", "NavigationInteractionUIViewController", IL2CPP_TYPE_CLASS, &component_class);
  AddField(navigation, "_missionLocationWidget", mission, offsetof(FleetMissionNavigation, mission));
  AddField(navigation, "_setCourseWidget", course, offsetof(FleetMissionNavigation, course));
  AddMethod(navigation, "OnSetCourseButtonClick", reinterpret_cast<void*>(&FleetNormalPress), T(void_class), {});
  FleetMissionButton     native_button{{&button.klass}};
  FleetMissionListener   native_listener{{&listener.klass}, &native_button};
  FleetMissionWidget     native_mission{{&mission.klass}, true, &native_listener};
  FleetNativeVisibility  native_visibility{{&visibility.klass}, VisibilityState::Hidden};
  FleetNativeViewer      native_course{{&course.klass}, &native_visibility};
  FleetMissionNavigation native_navigation{{&navigation.klass}, &native_mission, &native_course};
  fleet_native_widgets[&navigation.klass]               = {&native_navigation.header};
  auto* fleet                                           = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
  Config::Get().control.primary_requires_visible_target = true;
  Require(ActOnViewer({.primary = true}, fleet) && fleet_mission_presses == 1 && fleet_normal_presses == 0,
          "visible mission GO uses its native button while the ordinary set-course card is hidden");
  Require(!ActOnViewer({.secondary = true}, fleet) && fleet_mission_presses == 1,
          "secondary action does not fly to a mission marker");
  native_button.interactable = false;
  Require(ActOnViewer({.primary = true}, fleet) && fleet_mission_presses == 1 && fleet_normal_presses == 0,
          "native rejection of mission GO cannot fall through to ordinary navigation");
  native_button.interactable = true;
  native_mission.listener    = nullptr;
  Require(ActOnViewer({.primary = true}, fleet) && fleet_mission_presses == 1 && fleet_normal_presses == 0,
          "visible mission without a button cannot reuse a stale ordinary destination");
  native_mission.listener = &native_listener;
  native_mission.visible  = false;
  Require(!ActOnViewer({.primary = true}, fleet) && fleet_mission_presses == 1,
          "hidden mission widget does not activate its retained GO button");
  native_mission.visible = true;
  fleet_mission_active   = false;
  Require(!ActOnViewer({.primary = true}, fleet) && fleet_mission_presses == 1,
          "inactive mission widget does not activate its retained GO button");
  fleet_mission_active    = true;
  native_mission.visible  = false;
  native_visibility.state = VisibilityState::Visible;
  Require(ActOnViewer({.primary = true}, fleet) && fleet_normal_presses == 1 && fleet_mission_presses == 1,
          "ordinary visible set-course navigation keeps its own native handler");
  std::cout << "Mission marker primary action and ordinary navigation passed\n";
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
  auto& native_behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  AddMethod(native_behaviour, "get_isActiveAndEnabled", reinterpret_cast<void*>(&FleetMissionActive), T(bool_class),
            {});
  AddProperty(native_behaviour, "isActiveAndEnabled", native_behaviour.methods.back());
  auto& generic_button = Define("Digit.Client.UI", "GenericButtonWidget", IL2CPP_TYPE_CLASS, &native_behaviour);
  auto& scan_buttons   = Define("Digit.Prime.Combat", "ScanEngageButtonsWidget", IL2CPP_TYPE_CLASS, &native_behaviour);
  AddField(viewer, "_scanEngageButtonsWidget", scan_buttons, offsetof(FleetGuardPreScan, buttons));
  AddField(viewer, "_addToQueueButtonWidget", generic_button, offsetof(FleetGuardPreScan, queue_button));
  AddField(viewer_base, "_visibilityController", visibility, offsetof(FleetNativeViewer, visibility));
  AddField(visibility, "_state", int_class, offsetof(FleetNativeVisibility, state));
  AddMethod(mine, "MineClicked", reinterpret_cast<void*>(&FleetNativeMine), T(void_class), {});
  AddMethod(viewer_base, "HideAllViewers", reinterpret_cast<void*>(&FleetNativeHide), T(void_class), {});
  AddMethod(target, "get_TargetID", reinterpret_cast<void*>(&FleetNativeTarget), T(long_class), {});
  Il2CppObject      target_object{&target.klass};
  FleetGuardPreScan viewer_object{{&viewer.klass}};
  auto*             context = reinterpret_cast<BattleTargetData*>(&target_object);
  auto*             card    = reinterpret_cast<PreScanTargetWidget*>(&viewer_object);
  input_frame               = 100;
  const auto before_replay  = warp_roots.size();
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

  // Exercise the production dispatcher against selected-slot queue storage and native controls.
  auto& bar          = Define("Digit.Prime.HUD", "FleetBarViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& manager      = Define("Prime.ActionQueue", "ActionQueueManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& queue_type   = Define("Prime.ActionQueue", "ActionQueueInstance", IL2CPP_TYPE_CLASS, &object_class);
  auto& queue_list   = Define("Fixture", "PrimaryQueueList", IL2CPP_TYPE_CLASS, &object_class);
  auto& queue_vector = Define("Fixture", "PrimaryQueueVector", IL2CPP_TYPE_SZARRAY, &array_class);
  queue_vector.klass.byval_arg.data.type = T(queue_type);
  AddField(bar, "_fleetPanelController", panel, offsetof(FleetGuardBar, panel));
  AddMethod(panel, "get_fleet", reinterpret_cast<void*>(&FleetGuardSelected), T(fleet_data_class), {});
  AddProperty(panel, "fleet", panel.methods.back());
  AddMethod(fleet_data_class, "get_Index", reinterpret_cast<void*>(&FleetGuardIndex), T(int_class), {});
  AddProperty(fleet_data_class, "Index", fleet_data_class.methods.back());
  AddField(queue_type, "<PlayerFleetId>k__BackingField", long_class, offsetof(FleetGuardQueue, fleet));
  AddField(queue_type, "_actionQueue", queue_list, offsetof(FleetGuardQueue, actions));
  AddField(manager, "_battleQueue", queue_vector, offsetof(FleetGuardManager, queues));
  manager.fields.back().type = T(queue_vector);
  AddMethod(queue_list, "get_Count", reinterpret_cast<void*>(&FleetGuardCount), T(int_class), {});
  AddMethod(manager, "get_Instance", reinterpret_cast<void*>(&FleetGuardInstance), T(manager), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager, "Instance", manager.methods.back());
  AddMethod(manager, "ClearQueue", reinterpret_cast<void*>(&FleetGuardClear), T(void_class), {T(fleet_data_class)});
  AddMethod(manager, "IsQueueUnlocked", reinterpret_cast<void*>(&FleetGuardUnlocked), T(bool_class), {});
  AddMethod(manager, "IsQueueFull", reinterpret_cast<void*>(&FleetGuardFull), T(bool_class), {T(fleet_data_class)});

  auto& button = Define("UnityEngine.UI", "Button", IL2CPP_TYPE_CLASS, &native_behaviour);
  AddMethod(button, "Press", reinterpret_cast<void*>(&FleetGuardEnqueue), T(void_class), {});
  auto& listener = Define("Digit.Client.UI", "SemaphoreButtonListener", IL2CPP_TYPE_CLASS, &component_class);
  AddField(listener, "_button", button, offsetof(FleetMissionListener, button));
  auto& course = Define("Digit.Prime.Navigation", "SetCourseWidget", IL2CPP_TYPE_CLASS, &native_behaviour);
  AddField(course, "_visibilityController", visibility, offsetof(FleetGuardCourse, visibility));
  AddField(course, "_joinWaveDefenseButton", listener, offsetof(FleetGuardCourse, join));
  auto& mission = Define("Digit.Prime.Navigation", "MissionLocationWidget", IL2CPP_TYPE_CLASS, &native_behaviour);
  auto& navigation =
      Define("Digit.Prime.Navigation", "NavigationInteractionUIViewController", IL2CPP_TYPE_CLASS, &component_class);
  AddField(navigation, "_setCourseWidget", course, offsetof(FleetMissionNavigation, course));
  AddField(navigation, "_missionLocationWidget", mission, offsetof(FleetMissionNavigation, mission));
  AddMethod(navigation, "OnSetCourseButtonClick", reinterpret_cast<void*>(&FleetNormalPress), T(void_class), {});

  FleetGuardList  own_actions{{&queue_list.klass}, 16}, other_actions{{&queue_list.klass}, 16};
  FleetGuardQueue own_queue{{&queue_type.klass}, 999, &own_actions},
      other_queue{{&queue_type.klass}, 1000, &other_actions};
  FleetGuardVector       native_queues{{&queue_vector.klass}, nullptr, 2, {&own_queue, &other_queue}};
  FleetGuardManager      native_manager{{&manager.klass}, &native_queues};
  FleetGuardPanel        native_panel{{&panel.klass}, fleet};
  FleetGuardBar          native_bar{{&bar.klass}, &native_panel};
  FleetNativeVisibility  course_visibility{{&visibility.klass}, VisibilityState::Visible};
  FleetGuardCourse       native_course{{&course.klass}, &course_visibility};
  FleetMissionNavigation native_navigation{
      {&navigation.klass}, nullptr, reinterpret_cast<FleetNativeViewer*>(&native_course)};
  fleet_guard_manager                     = &native_manager;
  fleet_guard_actions                     = &own_actions;
  fleet_native_widgets[&bar.klass]        = {&native_bar.header};
  fleet_native_widgets[&navigation.klass] = {&native_navigation.header};
  fleet_native_state                      = FleetState::IdleInSpace;
  auto& protect_queue                     = Config::Get().control.primary_protects_queue;
  Require(!protect_queue, "primary queue protection remains opt-in by default");
  const auto moves_before = fleet_normal_presses;
  ActOnSelectedFleet({.primary = true});
  Require(fleet_normal_presses == moves_before + 1 && own_actions.reads == 0 && other_actions.reads == 0,
          "disabled protection preserves primary movement without inspecting queues");
  protect_queue = true;
  for (const int32_t count : {1, 16}) {
    own_actions.count = count;
    const auto reads  = own_actions.reads;
    ActOnSelectedFleet({.primary = true});
    Require(fleet_normal_presses == moves_before + 1 && own_actions.reads == reads + 1 && other_actions.reads == 0,
            "one or many selected-fleet targets suppress primary with one count read and no other-slot scan");
  }
  ActOnSelectedFleet({.secondary = true});
  Require(own_actions.reads == 2, "non-primary input performs no queue observation");
  own_actions.count = 0;
  ActOnSelectedFleet({.primary = true});
  Require(fleet_normal_presses == moves_before + 2 && other_actions.reads == 0,
          "another fleet's nonempty queue does not block the empty selected queue");
  own_actions.count           = 16;
  own_queue.fleet             = 1000;
  const auto mismatched_reads = own_actions.reads;
  ActOnSelectedFleet({.primary = true});
  Require(fleet_normal_presses == moves_before + 3 && own_actions.reads == mismatched_reads,
          "a rebound slot with a different owner cannot block the selected fleet");
  own_queue.fleet = 999;
  for (const int32_t count : {-1, 0}) {
    own_actions.count = count;
    ActOnSelectedFleet({.primary = true});
  }
  own_actions.throws = true;
  ActOnSelectedFleet({.primary = true});
  own_actions.throws = false;
  Require(fleet_normal_presses == moves_before + 6, "negative, empty and unavailable counts preserve native primary");
  own_actions.count = 16;
  fleet_guard_index = 2;
  ActOnSelectedFleet({.primary = true});
  fleet_guard_index        = 0;
  fleet_guard_index_throws = true;
  ActOnSelectedFleet({.primary = true});
  fleet_guard_index_throws = false;
  Require(fleet_normal_presses == moves_before + 8, "out-of-range and unavailable selected slots preserve primary");
  const auto clear_reads = own_actions.reads;
  ActOnSelectedFleet({.queue_clear = true});
  Require(fleet_guard_clears == 1 && own_actions.count == 0 && own_actions.reads == clear_reads,
          "explicit queue clearing remains available and does not inspect the queue as primary input");

  // The shared primary/queue chord must still press the native add-to-queue control.
  AddField(generic_button, "_semaphoreButtonListener", listener, offsetof(FleetGuardQueueButton, listener));
  AddMethod(scan_buttons, "get_Context", reinterpret_cast<void*>(&FleetGuardContext), T(target), {});
  AddProperty(scan_buttons, "Context", scan_buttons.methods.back());
  AddMethod(scan_buttons, "OnScanButtonClicked", reinterpret_cast<void*>(&FleetGuardScan), T(void_class), {});
  auto& deployed    = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& hull        = Define("Digit.PrimeServer.Models", "HullSpec", IL2CPP_TYPE_CLASS, &object_class);
  auto& hull_type   = Define("Digit.PrimeServer.Models", "HullType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  hull_type.is_enum = true;
  AddField(target, "TargetFleetDeployedData", deployed, offsetof(FleetGuardTarget, deployed));
  AddMethod(deployed, "get_Hull", reinterpret_cast<void*>(&FleetGuardHull), T(hull), {});
  AddProperty(deployed, "Hull", deployed.methods.back());
  AddMethod(hull, "get_Type", reinterpret_cast<void*>(&FleetGuardHullType), T(hull_type), {});
  AddProperty(hull, "Type", hull.methods.back());
  Il2CppObject          native_deployed{&deployed.klass}, native_hull{&hull.klass}, native_buttons{&scan_buttons.klass};
  FleetGuardTarget      native_target{{&target.klass}, &native_deployed};
  FleetMissionButton    native_button{{&button.klass}};
  FleetMissionListener  native_listener{{&listener.klass}, &native_button};
  FleetGuardQueueButton native_queue_button{{&generic_button.klass}, &native_listener};
  FleetNativeVisibility target_visibility{{&visibility.klass}, VisibilityState::Visible};
  FleetGuardPreScan     native_scan{{&viewer.klass}, &target_visibility, &native_buttons, &native_queue_button.header};
  fleet_guard_target                  = &native_target;
  fleet_guard_hull                    = &native_hull;
  fleet_native_widgets[&viewer.klass] = {&native_scan.header};
  own_actions.count                   = 16;
  ActOnSelectedFleet({.primary = true, .queue = true, .recall_cancel = true});
  Require(fleet_guard_enqueues == 1 && own_actions.count == 17 && fleet_normal_presses == moves_before + 8,
          "shared primary/queue binding still adds a target without moving or cancelling an idle fleet");
  target_visibility.state = VisibilityState::Visible;
  const auto scan_reads   = own_actions.reads;
  ActOnSelectedFleet({.secondary = true});
  Require(fleet_guard_scans == 1 && own_actions.reads == scan_reads,
          "secondary target scanning remains independent of queue protection");
  protect_queue       = false;
  fleet_guard_manager = nullptr;
  fleet_guard_actions = nullptr;
  fleet_guard_target  = nullptr;
  fleet_guard_hull    = nullptr;
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
{ return !fleet_native_capture && std::ranges::contains(fleet_native_keys, action); }
bool IsDown(Action action, bool allow_mouse)
{ return IsDown(action) && (allow_mouse || action != Action::kActionPrimary || !fleet_native_primary_mouse); }
bool IsPressed(Action action)
{ return IsDown(action); }
bool IsDownWithShift(Action action)
{ return IsDown(action); }
bool CaptureBlocksShortcuts() noexcept
{ return fleet_native_capture; }
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
  fleet_native_saved_any     = value;
  ++fleet_native_save_calls;
  if (const auto* boolean = std::get_if<bool>(&value))
    fleet_native_saved_value = *boolean;
}
void RetainSessionIntent(const char* section, const char* key, config_edit::Value value) noexcept
{
  fleet_native_session_section = section;
  fleet_native_session_key     = key;
  fleet_native_session_any     = std::move(value);
  ++fleet_native_session_calls;
}
} // namespace runtime_config
