#pragma once

// Actual screens.cc helpers and dispatcher in the existing controlled IL2CPP host.
// Native section changes, Canvas visibility and Button.Press are counted substitutes, not Unity UI qualification.
#include "../src/mod/features/screens.cc"

namespace
{
struct ScreenActionsController {
  Il2CppObject               header;
  unity::Button*             ship    = nullptr;
  unity::Button*             officer = nullptr;
  GenericButtonWidget*       presets = nullptr;
  ShipManagementViewContext* context = nullptr;
  bool                       active  = true;
};
struct ScreenActionsPanel {
  Il2CppObject     header;
  FleetPlayerData* fleet = nullptr;
};
struct ScreenActionsBar {
  Il2CppObject              header;
  FleetLocalViewController* panel = nullptr;
};
struct ScreenActionsContext {
  Il2CppObject header;
  int32_t      count = 3, index = 1;
  bool         increment = false;
};
Il2CppObject  screen_actions_sections{}, screen_actions_other_sections{}, screen_actions_canvas{};
Il2CppObject* screen_actions_current_sections = &screen_actions_sections;
SectionID     screen_actions_section = SectionID::Navigation_System, screen_actions_destination = SectionID::AppInit;
bool          screen_actions_visible = true, screen_actions_press_throws = false, screen_actions_accepted = true;
bool          screen_actions_press_throws_after = false;
bool          screen_actions_index_throws = false, screen_actions_count_throws = false;
int           screen_actions_presses = 0, screen_actions_requests = 0, screen_actions_navigations = 0;
int           screen_actions_index_writes = 0;
std::function<void()> screen_actions_visibility_callback, screen_actions_press_callback;
std::function<void()> screen_actions_section_callback, screen_actions_direction_callback;

Il2CppObject* ScreenActionsSections(const MethodInfo*)
{ return screen_actions_current_sections; }
SectionID ScreenActionsSection(Il2CppObject*, const MethodInfo*)
{
  if (screen_actions_section_callback)
    screen_actions_section_callback();
  return screen_actions_section;
}
Il2CppObject* ScreenActionsCanvas(Il2CppObject*, Il2CppReflectionType*, const MethodInfo*)
{ return &screen_actions_canvas; }
bool ScreenActionsVisible(Il2CppObject*, const MethodInfo*)
{
  if (screen_actions_visibility_callback)
    screen_actions_visibility_callback();
  return screen_actions_visible;
}
bool ScreenActionsActive(ScreenActionsController* self, const MethodInfo*)
{ return self->active; }
void ScreenActionsPress(Il2CppObject* self, const MethodInfo*)
{
  Require(std::ranges::any_of(warp_roots, [self](const auto& root) { return root.second == self; }),
          "native screen button invocation retains operation-local button custody");
  if (screen_actions_press_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++screen_actions_presses;
  if (screen_actions_press_callback)
    screen_actions_press_callback();
  if (screen_actions_press_throws_after)
    throw Il2CppExceptionWrapper(&thrown);
}
Il2CppObject* ScreenActionsFleet(ScreenActionsPanel* self, const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(self->fleet); }
bool ScreenActionsManage(ScreenActionsPanel* self, il2cpp::Object* target, ActionType action, int64_t index,
                         ActionBehaviour behaviour, void* callback, const MethodInfo*)
{
  Require(target == self->fleet && action == ActionType::Manage && index == 0 && behaviour == ActionBehaviour::Default
              && !callback,
          "screen navigation retains the selected-fleet native Manage request and call arguments");
  ++screen_actions_requests;
  return screen_actions_accepted;
}
bool ScreenActionsNavigate(Il2CppObject*, SectionID destination, void*, bool forced, bool back, bool same,
                           const MethodInfo*)
{
  Require(!forced && !back && same, "screen shortcuts preserve native section-change flags");
  ++screen_actions_navigations;
  screen_actions_destination = destination;
  return true;
}
int32_t ScreenActionsCount(ScreenActionsContext* self, const MethodInfo*)
{
  if (screen_actions_count_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return self->count;
}
int32_t ScreenActionsIndex(ScreenActionsContext* self, const MethodInfo*)
{
  if (screen_actions_index_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return self->index;
}
void ScreenActionsDirection(ScreenActionsContext* self, bool increment, const MethodInfo*)
{
  self->increment = increment;
  if (screen_actions_direction_callback)
    screen_actions_direction_callback();
}
void ScreenActionsSetIndex(ScreenActionsContext* self, int32_t index, const MethodInfo*)
{
  ++screen_actions_index_writes;
  self->index = index;
}

void ScreenActionsNativeCases(std::string_view mode, void (*extra_case)() = nullptr)
{
  auto&       behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  const auto& active =
      AddMethod(behaviour, "get_isActiveAndEnabled", reinterpret_cast<void*>(&ScreenActionsActive), T(bool_class), {});
  AddProperty(behaviour, "isActiveAndEnabled", &active);
  auto& canvas = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &behaviour);
  AddMethod(component_class, "GetComponentInParent", reinterpret_cast<void*>(&ScreenActionsCanvas), T(canvas),
            {T(type_class)});
  const auto& visible =
      AddMethod(canvas, "get_Visible", reinterpret_cast<void*>(&ScreenActionsVisible), T(bool_class), {});
  AddProperty(canvas, "Visible", &visible);
  auto& button = Define("UnityEngine.UI", "Button", IL2CPP_TYPE_CLASS, &behaviour);
  AddMethod(button, "Press", reinterpret_cast<void*>(&ScreenActionsPress), T(void_class), {});
  auto& management =
      Define("Digit.Prime.Ships", "ShipManagementScreenStateViewController", IL2CPP_TYPE_CLASS, &behaviour);
  AddField(management, "_swapShipButton", button, offsetof(ScreenActionsController, ship));
  AddField(management, "_officerIconButton", button, offsetof(ScreenActionsController, officer));
  auto& assignment =
      Define("Digit.Prime.OfficerAssignment", "OfficerAssignmentViewController", IL2CPP_TYPE_CLASS, &behaviour);
  auto& generic  = Define("Digit.Client.UI", "GenericButtonWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& listener = Define("Digit.Client.UI", "SemaphoreButtonListener", IL2CPP_TYPE_CLASS, &object_class);
  AddField(assignment, "_officerPresetsButton", generic, offsetof(ScreenActionsController, presets));
  struct ReferenceObject {
    Il2CppObject  header;
    Il2CppObject* value;
  };
  AddField(generic, "_semaphoreButtonListener", listener, offsetof(ReferenceObject, value));
  AddField(listener, "_button", button, offsetof(ReferenceObject, value));
  auto&       hub          = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto&       sections     = Define("Digit.Client.Sections", "SectionManager", IL2CPP_TYPE_CLASS, &object_class);
  const auto& get_sections = AddMethod(hub, "get_SectionManager", reinterpret_cast<void*>(&ScreenActionsSections),
                                       T(sections), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "SectionManager", &get_sections);
  const auto& get_section =
      AddMethod(sections, "get_CurrentSection", reinterpret_cast<void*>(&ScreenActionsSection), T(int_class), {});
  AddProperty(sections, "CurrentSection", &get_section);
  AddMethod(sections, "TriggerSectionChange", reinterpret_cast<void*>(&ScreenActionsNavigate), T(bool_class),
            {T(int_class), T(object_class), T(bool_class), T(bool_class), T(bool_class)});
  auto& panel = Define("Digit.Prime.Ships", "FleetLocalViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(panel, "get_fleet", reinterpret_cast<void*>(&ScreenActionsFleet), T(fleet_data_class), {});
  auto& bar = Define("Digit.Prime.HUD", "FleetBarViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddField(bar, "_fleetPanelController", panel, offsetof(ScreenActionsBar, panel));
  auto& action = Define("Digit.PrimeServer.Models", "ActionType", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(ActionType));
  auto& action_behaviour   = Define("Digit.PrimeServer.Models", "ActionBehaviour", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  action_behaviour.is_enum = true;
  AddMethod(panel, "RequestAction", reinterpret_cast<void*>(&ScreenActionsManage), T(bool_class),
            {T(object_class), T(action), T(long_class), T(action_behaviour), T(object_class)});
  auto& ship_view = Define("Digit.Prime.Ships", "ShipManagementViewController", IL2CPP_TYPE_CLASS, &behaviour);
  auto& context   = Define("Digit.Prime.Ships", "ShipManagementViewContext", IL2CPP_TYPE_CLASS, &object_class);
  AddField(ship_view, "m_context", context, offsetof(ScreenActionsController, context));
  AddMethod(context, "get_ListCount", reinterpret_cast<void*>(&ScreenActionsCount), T(int_class), {});
  AddMethod(context, "get_CurrentIndex", reinterpret_cast<void*>(&ScreenActionsIndex), T(int_class), {});
  AddMethod(context, "set_LastMovementWasToIncrement", reinterpret_cast<void*>(&ScreenActionsDirection), T(void_class),
            {T(bool_class)});
  AddMethod(context, "set_CurrentIndex", reinterpret_cast<void*>(&ScreenActionsSetIndex), T(void_class),
            {T(int_class)});
  screen_actions_sections.klass = screen_actions_other_sections.klass = &sections.klass;
  screen_actions_canvas.klass                                         = &canvas.klass;
  Il2CppObject            ship_button{&button.klass}, officer_button{&button.klass}, presets_button{&button.klass};
  ReferenceObject         native_listener{{&listener.klass}, &presets_button};
  ReferenceObject         native_widget{{&generic.klass}, &native_listener.header};
  ScreenActionsController controller{{&management.klass},
                                     reinterpret_cast<unity::Button*>(&ship_button),
                                     reinterpret_cast<unity::Button*>(&officer_button)};
  ScreenActionsController officer{
      {&assignment.klass}, nullptr, nullptr, reinterpret_cast<GenericButtonWidget*>(&native_widget)};
  ScreenActionsContext    native_context{{&context.klass}}, replacement_context{{&context.klass}};
  ScreenActionsController native_view{
      {&ship_view.klass}, nullptr, nullptr, nullptr, reinterpret_cast<ShipManagementViewContext*>(&native_context)};
  ScreenActionsPanel native_panel{{&panel.klass}, reinterpret_cast<FleetPlayerData*>(&identity_fleet)};
  ScreenActionsBar   native_bar{{&bar.klass}, reinterpret_cast<FleetLocalViewController*>(&native_panel)};
  const auto         roots                  = warp_roots.size();
  Config::Get().control.hotkeys_enabled     = true;
  Config::Get().control.use_scopely_hotkeys = false;
  fleet_native_keys.clear();
  fleet_native_capture = false;
  selected             = nullptr;
  input_frame          = 1600;
  unity::Key::Poll();
  const auto expose_management = [&] { fleet_native_widgets[&management.klass] = {&controller.header}; };
  const auto expose_assignment = [&] { fleet_native_widgets[&assignment.klass] = {&officer.header}; };

  if (mode == "screen-actions-buttons") {
    expose_management();
    Require(PressShipSwapButton() && PressOfficerSwapButton() && screen_actions_presses == 2,
            "actual screen helpers invoke the existing visible ship/officer buttons once each");
    controller.active = false;
    Require(!PressShipSwapButton() && screen_actions_presses == 2, "inactive pooled screen cannot receive a shortcut");
    controller.active      = true;
    screen_actions_visible = false;
    Require(!PressOfficerSwapButton(), "active view on a hidden Canvas cannot receive a shortcut");
    screen_actions_visible      = true;
    screen_actions_press_throws = true;
    Require(!PressShipSwapButton() && screen_actions_presses == 2,
            "failed native Press invocation is not reported as a completed screen action");
    screen_actions_press_throws = false;
    warp_root_failure           = true;
    Require(!PressShipSwapButton(), "failed component custody cannot dispatch a borrowed screen button");
    warp_root_failure = false;
    expose_assignment();
    Require(PressOfficerPresetsButton() && screen_actions_presses == 3,
            "presets helper preserves widget-to-semaphore-to-native-button path");

    std::shared_ptr<ButtonWait> replacement;
    const auto                  before_requests = screen_actions_requests;
    screen_actions_press_throws_after           = true;
    screen_actions_press_callback               = [&] { replacement = AwaitButton(ButtonStage::kOfficerSwap); };
    for (auto show : {ShowShipSwap, ShowOfficerSwap, ShowOfficerPresets}) {
      CancelAwaitedButton();
      replacement.reset();
      const auto before_presses = screen_actions_presses;
      show();
      Require(replacement && pending_button == replacement && screen_actions_requests == before_requests
                  && screen_actions_presses == before_presses + 1,
              "immediate native Press that replaces the request then throws cannot resurrect the old fallback");
    }
    screen_actions_press_callback     = {};
    screen_actions_press_throws_after = false;
    CancelAwaitedButton();
    replacement.reset();

    auto alternate                          = controller;
    fleet_native_widgets[&management.klass] = {&controller.header, &alternate.header};
    int visibility_reads                    = 0;
    screen_actions_visibility_callback      = [&] {
      if (++visibility_reads == 1) {
        replacement            = AwaitButton(ButtonStage::kOfficerSwap);
        screen_actions_visible = false;
      } else
        screen_actions_visible = true; // Another pooled screen would be actionable if the old loop continued.
    };
    const auto before_presses = screen_actions_presses;
    Require(!PressShipSwapButton() && replacement && pending_button == replacement && visibility_reads == 1
                && screen_actions_presses == before_presses,
            "immediate visibility replacement stops the old loop before another pooled screen can be pressed");
    CancelAwaitedButton();
    replacement.reset();
    visibility_reads = 0;
    ShowShipSwap();
    Require(replacement && pending_button == replacement && visibility_reads == 1
                && screen_actions_presses == before_presses && screen_actions_requests == before_requests,
            "visibility replacement also prevents the old Show action from publishing a new fallback wait");
    screen_actions_visibility_callback = {};
    screen_actions_visible             = true;
    CancelAwaitedButton();
    replacement.reset();
  } else if (mode == "screen-actions-pending") {
    auto request = AwaitButton(ButtonStage::kShipSwap);
    Require(request && !PressAwaitedButton() && request->frames == 299,
            "missing screen waits with the original bounded poll budget");
    expose_management();
    screen_actions_section = SectionID::ShipManagement_Details;
    Require(PressAwaitedButton() && !pending_button && screen_actions_presses == 1,
            "delayed visible button is pressed once and releases the completed request");
    Require(!PressAwaitedButton() && screen_actions_presses == 1, "completed request cannot replay next frame");
    request = AwaitButton(ButtonStage::kOfficerAssignment);
    Require(!PressAwaitedButton() && pending_button == request && request->stage == ButtonStage::kOfficerPresets
                && screen_actions_presses == 2,
            "presets wait opens officer assignment through the native Officer Swap button exactly once");
    Require(!PressAwaitedButton() && screen_actions_presses == 2,
            "waiting for assignment does not repeatedly press the previous Officer Swap button");
    expose_assignment();
    screen_actions_section = SectionID::Officer_Assignment;
    Require(PressAwaitedButton() && !pending_button && screen_actions_presses == 3,
            "arrival at officer assignment completes the existing nested presets button path once");
    fleet_native_widgets[&assignment.klass].clear();
    screen_actions_section             = SectionID::ShipManagement_Details;
    request                            = AwaitButton(ButtonStage::kShipSwap);
    screen_actions_visibility_callback = [] { CancelAwaitedButton(); };
    Require(!PressAwaitedButton() && !pending_button && screen_actions_presses == 3,
            "synchronous getter cancellation blocks stale native button publication");
    screen_actions_visibility_callback = {};
    request                            = AwaitButton(ButtonStage::kShipSwap);
    screen_actions_press_callback      = [] { CancelAwaitedButton(); };
    Require(PressAwaitedButton() && !pending_button,
            "native Press cancellation cannot dereference an erased wait request");
    screen_actions_press_callback = [] { (void)AwaitButton(ButtonStage::kOfficerSwap); };
    request                       = AwaitButton(ButtonStage::kShipSwap);
    Require(PressAwaitedButton() && pending_button && pending_button != request
                && pending_button->stage == ButtonStage::kOfficerSwap,
            "successful old press cannot clear a synchronously replaced screen request");
    screen_actions_press_callback = {};
    CancelAwaitedButton();
    fleet_native_widgets.clear();
    request.reset();
    for (int trial = 0; trial < 5; ++trial) {
      auto waiting = AwaitButton(ButtonStage::kShipSwap);
      Require(bool(waiting), "cancellation trial acquires a qualified source");
      if (trial == 0)
        fleet_native_capture = true;
      if (trial == 1)
        Config::Get().control.use_scopely_hotkeys = true;
      if (trial == 2)
        Config::Get().control.hotkeys_enabled = false;
      if (trial == 3) {
        selected = &selected_object;
        ++input_frame;
        unity::Key::Poll();
      }
      if (trial == 4) {
        held_keys[std::to_underlying(unity::KeyCode::Escape)] = any_held = true;
        ++input_frame;
        unity::Key::Poll();
      }
      Require(!PressAwaitedButton() && !pending_button, "mode, capture, text focus and Escape cancel deferred actions");
      fleet_native_capture                                  = false;
      Config::Get().control.use_scopely_hotkeys             = false;
      Config::Get().control.hotkeys_enabled                 = true;
      selected                                              = nullptr;
      held_keys[std::to_underlying(unity::KeyCode::Escape)] = any_held = false;
      ++input_frame;
      unity::Key::Poll();
    }
    auto waiting                    = AwaitButton(ButtonStage::kShipSwap);
    screen_actions_current_sections = &screen_actions_other_sections;
    Require(!PressAwaitedButton() && !pending_button, "replacement SectionManager invalidates the owned request");
    screen_actions_current_sections = &screen_actions_sections;
    waiting                         = AwaitButton(ButtonStage::kShipSwap);
    screen_actions_section          = SectionID::GameSettings;
    Require(!PressAwaitedButton() && !pending_button, "unrelated navigation cancels the old source's button wait");
    waiting = AwaitButton(ButtonStage::kShipSwap);
    for (int i = 0; i < kButtonWaitFrames; ++i)
      Require(!PressAwaitedButton(), "missing button does not claim native consumption before timeout");
    Require(!pending_button, "300 unsuccessful polls retire the original request");
    waiting           = AwaitButton(ButtonStage::kShipSwap);
    fleet_native_keys = {Action::kShowResearch};
    Require(!PressAwaitedButton() && !pending_button, "new screen shortcut wins before the old deferred action");
    fleet_native_keys.clear();
    screen_actions_section_callback = [] { CancelAwaitedButton(); };
    Require(!AwaitButton(ButtonStage::kShipSwap) && !pending_button,
            "cancellation during source acquisition cannot resurrect a same-source wait");
    screen_actions_section_callback  = {};
    fleet_native_widgets[&bar.klass] = {&native_bar.header};
    screen_actions_accepted          = false;
    ShowShipSwap();
    Require(!pending_button && screen_actions_requests == 1,
            "rejected native Manage action cannot leave a delayed button armed");
    screen_actions_accepted = true;
    ShowShipSwap();
    Require(pending_button && pending_button->fleet_id == 999,
            "accepted native Manage captures exact selected bar/panel/fleet identity");
    FleetIdentityObject other_fleet = identity_fleet;
    native_panel.fleet              = reinterpret_cast<FleetPlayerData*>(&other_fleet);
    Require(!PressAwaitedButton() && !pending_button,
            "different selected fleet object, even with the same numeric ID, cannot receive the pending action");
    native_panel.fleet = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
    waiting.reset();
  } else if (mode == "screen-actions-routing") {
    const std::pair<Action, SectionID> routes[] = {{Action::kShowQTrials, SectionID::ChallengeSelection},
                                                   {Action::kShowRefinery, SectionID::Shop_Refining_List},
                                                   {Action::kShowFactions, SectionID::Shop_MainFactions},
                                                   {Action::kShowStationExterior, SectionID::Starbase_Exterior},
                                                   {Action::kShowStationInterior, SectionID::Starbase_Interior},
                                                   {Action::kShowArtifacts, SectionID::ArtifactHall_Inventory},
                                                   {Action::kShowInventory, SectionID::InventoryList},
                                                   {Action::kShowMissions, SectionID::Missions_AcceptedList},
                                                   {Action::kShowResearch, SectionID::Research_LandingPage},
                                                   {Action::kShowScrapYard, SectionID::ShipScrapping_List},
                                                   {Action::kShowOfficers, SectionID::OfficerInventory},
                                                   {Action::kShowCommander, SectionID::FleetCommander_Management},
                                                   {Action::kShowAwayTeam, SectionID::Missions_AwayTeamsList},
                                                   {Action::kShowEvents, SectionID::Tournament_Group_Selection},
                                                   {Action::kShowExoComp, SectionID::Consumables},
                                                   {Action::kShowDaily, SectionID::Missions_DailyGoals},
                                                   {Action::kShowGifts, SectionID::Shop_List},
                                                   {Action::kShowAlliance, SectionID::Alliance_Main},
                                                   {Action::kShowAllianceHelp, SectionID::Alliance_Help},
                                                   {Action::kShowAllianceArmada, SectionID::Alliance_Armadas},
                                                   {Action::kShowSettings, SectionID::GameSettings}};
    for (const auto [shortcut, expected] : routes) {
      fleet_native_keys = {shortcut};
      const auto before = screen_actions_navigations;
      Require(ScreenShortcuts() && screen_actions_destination == expected && screen_actions_navigations == before + 1,
              "actual screen shortcut table retains its exact native section destination and one-action dispatch");
    }
    fleet_native_keys = {Action::kShowResearch, Action::kShowInventory};
    Require(ScreenShortcuts() && screen_actions_destination == SectionID::InventoryList,
            "source table precedence is retained when two screen bindings match");
    fleet_native_widgets[&bar.klass] = {&native_bar.header};
    fleet_native_keys                = {Action::kShowShips};
    Require(!ScreenShortcuts() && screen_actions_requests == 0,
            "Manage Ships is not dispatched before the setting-shortcut chain");
    LateScreenShortcut();
    Require(screen_actions_requests == 1,
            "late Manage Ships helper retains the native selected-fleet action without consuming native Update");
  } else if (mode == "screen-actions-dispatch") {
    Require(extra_case != nullptr, "full-dispatch case requires the coordinator's actual hotkey callback");
    fleet_native_widgets[&bar.klass] = {&native_bar.header};
    extra_case(); // Reuse the same live metadata/owned fixture locals; never copy this setup into another host.
  } else {
    Require(mode == "screen-actions-stepping", "unknown screen-actions fixture mode");
    auto* view = reinterpret_cast<ShipManagementViewController*>(&native_view);
    Require(StepShipDetails(view, 1) && native_context.index == 2 && native_context.increment,
            "actual ship-details step retains direction-before-index mutation");
    Require(!StepShipDetails(view, 1) && native_context.index == 2, "last dock does not wrap or claim movement");
    screen_actions_index_throws = true;
    const auto writes           = screen_actions_index_writes;
    Require(!StepShipDetails(view, -1) && screen_actions_index_writes == writes,
            "unavailable current index cannot become index zero and authorize a different dock");
    screen_actions_index_throws = false;
    screen_actions_count_throws = true;
    Require(!StepShipDetails(view, -1), "unavailable list count remains unavailable");
    screen_actions_count_throws       = false;
    screen_actions_direction_callback = [&] {
      native_view.context = reinterpret_cast<ShipManagementViewContext*>(&replacement_context);
    };
    Require(!StepShipDetails(view, -1) && screen_actions_index_writes == writes,
            "context replacement during direction setter prevents publication into the previous dock context");
    screen_actions_direction_callback = {};
    native_view.context               = reinterpret_cast<ShipManagementViewContext*>(&native_context);
    screen_actions_visible            = false;
    Require(!StepShipDetails(view, -1), "hidden management screen does not intercept camera arrow keys");
  }
  CancelAwaitedButton();
  fleet_native_widgets.clear();
  fleet_native_keys.clear();
  Require(warp_roots.size() == roots, "screen fixture releases all native request and operation-local roots");
  std::cout << "Screen-actions production adapter " << mode << " passed; native UI remains separate qualification\n";
}
} // namespace
