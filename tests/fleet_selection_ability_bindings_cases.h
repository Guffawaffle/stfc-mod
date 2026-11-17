#pragma once

// The existing fleet_actions fixture already includes the production source. These cases provide native
// metadata and method bodies, not a second selection or ability-recovery implementation. No game/UI is run.
namespace
{
template <class T, size_t N> struct FleetSaArray {
  Il2CppObject        header{};
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t size   = N;
  std::array<T, N>    items{};
};
struct FleetSaModel {
  Il2CppObject header;
  int64_t      id    = 1001;
  FleetState   state = FleetState::IdleInSpace;
};
struct FleetSaContext {
  Il2CppObject  header;
  Il2CppObject* slots = nullptr;
};
struct FleetSaCanvas {
  Il2CppObject header;
  bool         visible = true;
};
struct FleetSaTimer {
  Il2CppObject    header;
  unity::TimeSpan remaining{-10'000'000};
};
struct FleetSaActionPanel {
  Il2CppObject   header;
  Il2CppObject*  ids     = nullptr;
  Il2CppObject*  buttons = nullptr;
  FleetSaTimer*  timer   = nullptr;
  FleetSaCanvas* canvas  = nullptr;
  bool           effect  = false;
};
struct FleetSaPanel {
  Il2CppObject        header;
  FleetSaModel*       fleet   = nullptr;
  FleetSaActionPanel* actions = nullptr;
};
struct FleetSaBar {
  Il2CppObject    header;
  FleetSaPanel*   panel    = nullptr;
  FleetSaContext* context  = nullptr;
  int32_t         selected = -1;
};
struct FleetSaButtonContext {
  Il2CppObject header;
  bool         interactable = false;
};
struct FleetSaButton {
  Il2CppObject          header;
  FleetSaButtonContext* context = nullptr;
  bool                  active  = true;
};
Il2CppObject          fleet_sa_manager{}, fleet_sa_ability_manager{}, fleet_sa_sections{};
int                   fleet_sa_requests = 0, fleet_sa_views = 0, fleet_sa_hides = 0, fleet_sa_enables = 0;
int                   fleet_sa_remaining_reads = 0, fleet_sa_ready_reads = 0;
bool                  fleet_sa_selected_throws = false, fleet_sa_request_throws = false, fleet_sa_state_throws = false;
bool                  fleet_sa_effect_throws = false, fleet_sa_timer_throws = false, fleet_sa_remaining_throws = false;
bool                  fleet_sa_ready_throws = false, fleet_sa_interactable_throws = false, fleet_sa_ready = true;
int64_t               fleet_sa_seen_ability = 0;
FleetPlayerData*      fleet_sa_seen_fleet   = nullptr;
std::function<void()> fleet_sa_on_hide, fleet_sa_on_ready, fleet_sa_on_selected;

int64_t FleetSaId(FleetSaModel* fleet, const MethodInfo*)
{ return fleet->id; }
FleetState FleetSaState(FleetSaModel* fleet, const MethodInfo*)
{
  if (fleet_sa_state_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return fleet->state;
}
FleetBarContext* FleetSaBarContext(FleetSaBar* bar, const MethodInfo*)
{ return reinterpret_cast<FleetBarContext*>(bar->context); }
Il2CppObject* FleetSaSlots(FleetSaContext* context, const MethodInfo*)
{ return context->slots; }
FleetPlayerData* FleetSaSelectedFleet(FleetSaPanel* panel, const MethodInfo*)
{ return reinterpret_cast<FleetPlayerData*>(panel->fleet); }
bool FleetSaSelected(FleetSaBar* bar, int32_t index, const MethodInfo*)
{
  if (fleet_sa_selected_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (fleet_sa_on_selected)
    std::exchange(fleet_sa_on_selected, {})();
  return bar->selected == index;
}
void FleetSaRequest(FleetSaBar* bar, int32_t index, bool simulated, const MethodInfo*)
{
  Require(!simulated, "selection keeps the original native simulated=false argument");
  if (fleet_sa_request_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++fleet_sa_requests;
  bar->selected = index;
}
Il2CppObject* FleetSaManager(const MethodInfo*)
{ return &fleet_sa_manager; }
Il2CppObject* FleetSaAbilityManager(const MethodInfo*)
{ return &fleet_sa_ability_manager; }
Il2CppObject* FleetSaSections(const MethodInfo*)
{ return &fleet_sa_sections; }
NavigationManager* FleetSaNavigation(Il2CppObject*, const MethodInfo*)
{ return reinterpret_cast<NavigationManager*>(&warp_navigation); }
void FleetSaHide(Il2CppObject*, const MethodInfo*)
{
  ++fleet_sa_hides;
  if (fleet_sa_on_hide)
    std::exchange(fleet_sa_on_hide, {})();
}
void FleetSaView(Il2CppObject*, FleetPlayerData* fleet, bool show_info, const MethodInfo*)
{
  Require(show_info, "repeat/current location retains native showSystemInfo=true");
  ++fleet_sa_views;
  fleet_sa_seen_fleet = fleet;
}
CanvasController* FleetSaCanvasOf(FleetSaActionPanel* panel, Il2CppReflectionType*, const MethodInfo*)
{ return reinterpret_cast<CanvasController*>(panel->canvas); }
bool FleetSaVisible(FleetSaCanvas* canvas, const MethodInfo*)
{ return canvas->visible; }
bool FleetSaEffect(FleetSaActionPanel* panel, const MethodInfo*)
{
  if (fleet_sa_effect_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return panel->effect;
}
GenericButtonContext* FleetSaButtonContextOf(FleetSaButton* button, const MethodInfo*)
{ return reinterpret_cast<GenericButtonContext*>(button->context); }
bool FleetSaButtonActive(FleetSaButton* button, const MethodInfo*)
{ return button->active; }
bool FleetSaInteractable(FleetSaButtonContext* context, const MethodInfo*)
{
  if (fleet_sa_interactable_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return context->interactable;
}
void FleetSaEnable(FleetSaButtonContext* context, bool enabled, const MethodInfo*)
{
  Require(enabled, "ability repair only enables a button; it does not invoke an ability");
  ++fleet_sa_enables;
  context->interactable = enabled;
}
TimerDataContext* FleetSaCooldown(FleetSaActionPanel* panel, int64_t id, const MethodInfo*)
{
  fleet_sa_seen_ability = id;
  if (fleet_sa_timer_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<TimerDataContext*>(panel->timer);
}
unity::TimeSpan FleetSaRemaining(FleetSaTimer* timer, const MethodInfo*)
{
  ++fleet_sa_remaining_reads;
  if (fleet_sa_remaining_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return timer->remaining;
}
bool FleetSaReady(Il2CppObject*, FleetPlayerData* fleet, int64_t id, const MethodInfo*)
{
  ++fleet_sa_ready_reads;
  fleet_sa_seen_fleet   = fleet;
  fleet_sa_seen_ability = id;
  if (fleet_sa_ready_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (fleet_sa_on_ready)
    std::exchange(fleet_sa_on_ready, {})();
  return fleet_sa_ready;
}

void FleetSelectionAbilityCases(std::string_view mode)
{
  auto& bar_class         = Define("Digit.Prime.HUD", "FleetBarViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& bar_context_class = Define("Digit.Prime.HUD", "FleetBarContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& panel_class       = Define("Digit.Prime.Ships", "FleetLocalViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& action_class =
      Define("Digit.Prime.Ships", "ShipActionPanelLocalViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& button_context_class = Define("Digit.Client.UI", "GenericButtonContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& button_class         = Define("Digit.Client.UI", "GenericButtonWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& canvas_class         = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &component_class);
  auto& timer_class          = Define("Digit.PrimeServer.Models", "TimerDataContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& time_class           = Define("System", "TimeSpan", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::TimeSpan));
  auto& wrong_time           = Define("Other", "SameSizeTime", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::TimeSpan));
  auto& fleet_state          = Define("Digit.PrimeServer.Models", "FleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  fleet_state.is_enum        = true;
  auto& fleets_class         = Define("Digit.Prime.FleetManagement", "FleetsManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& abilities_class =
      Define("Digit.Prime.ActivatedAbilities", "ActivatedAbilityManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& sections_class =
      Define("Digit.Prime.Navigation", "NavigationSectionManager", IL2CPP_TYPE_CLASS, &component_class);
  auto& fleets_array_class                      = Define("Game", "FleetSaFleet[]", IL2CPP_TYPE_SZARRAY, &array_class);
  auto& ids_array_class                         = Define("System", "Int64[]", IL2CPP_TYPE_SZARRAY, &array_class);
  auto& wrong_ids_class                         = Define("System", "Int32[]", IL2CPP_TYPE_SZARRAY, &array_class);
  auto& buttons_array_class                     = Define("Game", "FleetSaButton[]", IL2CPP_TYPE_SZARRAY, &array_class);
  fleets_array_class.klass.byval_arg.data.type  = T(fleet_data_class);
  ids_array_class.klass.byval_arg.data.type     = T(long_class);
  wrong_ids_class.klass.byval_arg.data.type     = T(int_class);
  buttons_array_class.klass.byval_arg.data.type = T(button_class);
  bar_class.klass.instance_size                 = sizeof(FleetSaBar);
  panel_class.klass.instance_size               = sizeof(FleetSaPanel);
  action_class.klass.instance_size              = sizeof(FleetSaActionPanel);
  AddField(bar_class, "_fleetPanelController", panel_class, offsetof(FleetSaBar, panel));
  AddField(panel_class, "_shipActionPanelLocalViewController", action_class, offsetof(FleetSaPanel, actions));
  AddField(action_class, "_abilityIDs", mode == "fleet-ability-id-field" ? wrong_ids_class : ids_array_class,
           offsetof(FleetSaActionPanel, ids));
  AddField(action_class, "_buttons", buttons_array_class, offsetof(FleetSaActionPanel, buttons));
  AddMethod(bar_class, "get_CanvasContext", reinterpret_cast<void*>(&FleetSaBarContext), T(bar_context_class), {});
  AddMethod(bar_class, "RequestSelect", reinterpret_cast<void*>(&FleetSaRequest), T(void_class),
            {T(int_class), T(bool_class)});
  AddMethod(bar_class, "IsIndexSelected", reinterpret_cast<void*>(&FleetSaSelected), T(bool_class), {T(int_class)});
  AddMethod(bar_context_class, "get_FleetData", reinterpret_cast<void*>(&FleetSaSlots), T(fleets_array_class), {});
  AddMethod(panel_class, "get_fleet", reinterpret_cast<void*>(&FleetSaSelectedFleet), T(fleet_data_class), {});
  for (const auto* original : fleet_data_class.methods)
    if (std::string_view(original->name) == "get_Id") {
      auto* replace          = const_cast<MethodInfo*>(original);
      replace->methodPointer = replace->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(&FleetSaId);
    }
  AddMethod(fleet_data_class, "get_CurrentState", reinterpret_cast<void*>(&FleetSaState), T(fleet_state), {});
  AddMethod(fleets_class, "get_Instance", reinterpret_cast<void*>(&FleetSaManager), T(fleets_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(fleets_class, "Instance", fleets_class.methods.back());
  AddMethod(fleets_class, "RequestViewFleet", reinterpret_cast<void*>(&FleetSaView), T(void_class),
            {T(fleet_data_class), T(bool_class)});
  AddMethod(sections_class, "get_Instance", reinterpret_cast<void*>(&FleetSaSections), T(sections_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(sections_class, "Instance", sections_class.methods.back());
  AddMethod(sections_class, "get_NavigationManager", reinterpret_cast<void*>(&FleetSaNavigation),
            T(warp_navigation_class), {});
  AddProperty(sections_class, "NavigationManager", sections_class.methods.back());
  AddMethod(warp_navigation_class, "HideInteraction", reinterpret_cast<void*>(&FleetSaHide), T(void_class), {});
  AddMethod(abilities_class, "get_Instance", reinterpret_cast<void*>(&FleetSaAbilityManager), T(abilities_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(abilities_class, "Instance", abilities_class.methods.back());
  AddMethod(abilities_class, "CanUseAbility", reinterpret_cast<void*>(&FleetSaReady), T(bool_class),
            {T(fleet_data_class), T(long_class)});
  AddMethod(action_class, "GetComponentInParent", reinterpret_cast<void*>(&FleetSaCanvasOf), T(canvas_class),
            {T(type_class)});
  AddMethod(canvas_class, "get_Visible", reinterpret_cast<void*>(&FleetSaVisible), T(bool_class), {});
  AddMethod(action_class, "HasShipAnActiveAbilityWithActiveEffect", reinterpret_cast<void*>(&FleetSaEffect),
            T(bool_class), {});
  AddMethod(action_class, "GetRemainingCooldown", reinterpret_cast<void*>(&FleetSaCooldown), T(timer_class),
            {T(long_class)});
  AddMethod(timer_class, "get_RemainingTime", reinterpret_cast<void*>(&FleetSaRemaining),
            T(mode == "fleet-ability-timer-type" ? wrong_time : time_class), {});
  AddMethod(button_class, "get_Context", reinterpret_cast<void*>(&FleetSaButtonContextOf), T(button_context_class), {});
  AddMethod(button_class, "get_isActiveAndEnabled", reinterpret_cast<void*>(&FleetSaButtonActive), T(bool_class), {});
  AddMethod(button_context_class, "get_Interactable", reinterpret_cast<void*>(&FleetSaInteractable), T(bool_class), {});
  AddMethod(button_context_class, "set_Interactable", reinterpret_cast<void*>(&FleetSaEnable), T(void_class),
            {T(bool_class)});

  FleetSaModel                      first{{&fleet_data_class.klass}}, second{{&fleet_data_class.klass}, 1002};
  FleetSaArray<FleetPlayerData*, 8> slots{{&fleets_array_class.klass}};
  slots.items[0] = reinterpret_cast<FleetPlayerData*>(&first);
  slots.items[1] = reinterpret_cast<FleetPlayerData*>(&second);
  FleetSaContext       context{{&bar_context_class.klass}, &slots.header};
  FleetSaCanvas        canvas{{&canvas_class.klass}};
  FleetSaTimer         timer{{&timer_class.klass}};
  FleetSaButtonContext button_context{{&button_context_class.klass}},
      other_button_context{{&button_context_class.klass}};
  FleetSaButton            button{{&button_class.klass}, &button_context};
  FleetSaArray<int64_t, 2> ids{{&ids_array_class.klass}};
  ids.items = {9007199254740993LL, 42};
  FleetSaArray<GenericButtonWidget*, 1> buttons{{&buttons_array_class.klass}};
  buttons.items[0] = reinterpret_cast<GenericButtonWidget*>(&button);
  FleetSaActionPanel actions{{&action_class.klass}, &ids.header, &buttons.header, &timer, &canvas};
  FleetSaPanel       panel{{&panel_class.klass}, &first, &actions};
  FleetSaBar         bar{{&bar_class.klass}, &panel, &context}, other_bar{{&bar_class.klass}, &panel, &context};
  fleet_sa_manager.klass                 = &fleets_class.klass;
  fleet_sa_ability_manager.klass         = &abilities_class.klass;
  fleet_sa_sections.klass                = &sections_class.klass;
  fleet_native_widgets[&bar_class.klass] = {&bar.header};
  auto* owner                            = reinterpret_cast<FleetBarViewController*>(&bar);
  auto& config                           = Config::Get();
  config.control.hotkeys_enabled         = true;
  config.control.use_scopely_hotkeys     = false;
  config.control.select_timer            = 500;
  config.ui.disable_preview_locate       = false;
  selected                               = nullptr;
  has_event_system                       = true;
  fleet_native_capture                   = false;
  fleet_native_keys.clear();
  ++input_frame;
  unity::Key::Poll();
  const auto roots = warp_roots.size();

  if (mode.starts_with("fleet-selection")) {
    const auto at     = FleetSelectionClock::time_point{} + std::chrono::seconds(100);
    fleet_native_keys = {Action::kSelectShip1};
    Require(SelectShipShortcutsAt(at) && fleet_sa_requests == 1 && fleet_sa_views == 0,
            "first number-key request selects without locating");
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(499)) && fleet_sa_requests == 1 && fleet_sa_views == 1
                && fleet_sa_seen_fleet == reinterpret_cast<FleetPlayerData*>(&first),
            "same qualified bar/slot/fleet within timer locates the selected ship");
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(999)) && fleet_sa_requests == 2 && fleet_sa_views == 1,
            "exact timer endpoint is an ordinary select, not a locate");
    fleet_sa_selected_throws = true;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1000)) && fleet_sa_requests == 3
                && fleet_sa_views == 1,
            "unknown native selected state cannot authorize a locate");
    fleet_sa_selected_throws = false;
    slots.items[0]           = reinterpret_cast<FleetPlayerData*>(&second);
    panel.fleet              = &second;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1001)) && fleet_sa_requests == 4
                && fleet_sa_views == 1,
            "same-slot replacement fleet receives a first selection rather than stale repeat location");
    second.id = 1003;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1002)) && fleet_sa_requests == 5
                && fleet_sa_views == 1,
            "same-object fleet identity replacement invalidates repeat location");
    fleet_native_widgets[&bar_class.klass] = {&other_bar.header};
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1003)) && fleet_sa_requests == 6
                && fleet_sa_views == 1,
            "replacement bar cannot consume the old bar's repeated press");
    fleet_native_widgets[&bar_class.klass] = {&bar.header};
    bar.selected                           = 0;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1004)), "returning bar receives ordinary selection");
    fleet_sa_on_hide = [&] { panel.fleet = &first; };
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(1005)) && fleet_sa_views == 1
                && !fleet_selection_repeat,
            "native hide changing the selected fleet consumes locate without dispatching to either fleet");
    panel.fleet       = &second;
    fleet_native_keys = {Action::kSelectCurrent};
    Require(FleetShortcuts() && fleet_sa_views == 2
                && fleet_sa_seen_fleet == reinterpret_cast<FleetPlayerData*>(&second),
            "select-current retains its independent native camera workflow");
    fleet_native_keys = {Action::kSelectShip8};
    Require(SelectShipShortcutsAt(at + std::chrono::seconds(2)) && !fleet_selection_repeat && bar.selected == 7,
            "empty slot still reaches native RequestSelect but cannot create a repeated-fleet identity");
    constexpr std::array ship_keys{Action::kSelectShip1, Action::kSelectShip2, Action::kSelectShip3,
                                   Action::kSelectShip4, Action::kSelectShip5, Action::kSelectShip6,
                                   Action::kSelectShip7, Action::kSelectShip8};
    for (size_t index = 0; index < ship_keys.size(); ++index) {
      fleet_selection_repeat.reset();
      fleet_native_keys = {ship_keys[index]};
      Require(SelectShipShortcutsAt(at) && bar.selected == static_cast<int32_t>(index),
              "all eight configured ship actions preserve their native fleet-bar slot mapping");
    }
    auto& visibility_class = Define("Digit.Client.UI", "VisibilityController", IL2CPP_TYPE_CLASS, &object_class);
    auto& viewer_base =
        Define("Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget", IL2CPP_TYPE_CLASS, &component_class);
    auto& viewer_class = Define("Digit.Prime.Combat", "PreScanTargetWidget", IL2CPP_TYPE_CLASS, &viewer_base);
    AddField(viewer_base, "_visibilityController", visibility_class, offsetof(FleetNativeViewer, visibility));
    AddField(visibility_class, "_state", int_class, offsetof(FleetNativeVisibility, state));
    FleetNativeVisibility visibility{{&visibility_class.klass}};
    FleetNativeViewer     viewer{{&viewer_class.klass}, &visibility};
    fleet_native_widgets[&viewer_class.klass] = {&viewer.header};
    config.ui.disable_preview_locate          = true;
    fleet_native_keys                         = {Action::kSelectShip1};
    panel.fleet                               = &second;
    fleet_selection_repeat.reset();
    const auto before_blocked_views = fleet_sa_views;
    Require(SelectShipShortcutsAt(at) && SelectShipShortcutsAt(at + std::chrono::milliseconds(1))
                && fleet_sa_views == before_blocked_views,
            "open preview blocks repeat location while preserving ordinary number-key selection");
    fleet_native_widgets.erase(&viewer_class.klass);
    config.ui.disable_preview_locate   = false;
    auto replacement_context           = context;
    bar.context                        = &replacement_context;
    const auto before_context_requests = fleet_sa_requests;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(2)) && fleet_sa_requests == before_context_requests + 1
                && fleet_sa_views == before_blocked_views,
            "same slot/fleet through a replaced bar context needs ordinary selection before location");
    auto replacement_slots    = slots;
    replacement_context.slots = &replacement_slots.header;
    Require(SelectShipShortcutsAt(at + std::chrono::milliseconds(3)) && fleet_sa_requests == before_context_requests + 2
                && fleet_sa_views == before_blocked_views,
            "replaced context vector cannot inherit repeat location from its predecessor");
    bar.context       = &context;
    fleet_native_keys = {Action::kSelectShip1};
    selected          = &selected_object;
    ++input_frame;
    unity::Key::Poll();
    const auto before_disabled = fleet_sa_requests;
    Require(!SelectShipShortcutsAt(at) && fleet_sa_requests == before_disabled && !fleet_selection_repeat,
            "focused text excludes number selection and clears repeat identity");
    selected = nullptr;
    ++input_frame;
    unity::Key::Poll();
    fleet_native_capture = true;
    Require(!SelectShipShortcutsAt(at), "shortcut editor owns selection keys");
    fleet_native_capture               = false;
    config.control.use_scopely_hotkeys = true;
    Require(!SelectShipShortcutsAt(at), "native-hotkey mode does not inject mod selection");
    config.control.use_scopely_hotkeys = false;
    config.control.hotkeys_enabled     = false;
    Require(!SelectShipShortcutsAt(at), "disabled mod hotkeys do not inject selection");
    config.control.hotkeys_enabled = true;
    warp_root_failure              = true;
    Require(!SelectShipShortcutsAt(at) && fleet_sa_requests == before_disabled,
            "failed native custody cannot publish a selection or retain weak repeat state");
    warp_root_failure = false;
    fleet_selection_repeat.reset();
    fleet_native_widgets.clear();
    fleet_native_keys.clear();
    Require(warp_roots.size() == roots, "selection requests release operation-local roots and repeat identities");
    std::cout << "Production fleet selection/current-location, timing and replacement fixtures passed\n";
    return;
  }

  if (mode == "fleet-ability-id-field" || mode == "fleet-ability-timer-type") {
    RecoverAbilityButtons();
    Require(fleet_sa_enables == 0 && fleet_sa_remaining_reads == 0,
            "malformed ability ID metadata or unrelated eight-byte timer value cannot authorize repair");
    Require(warp_roots.size() == roots, "unavailable ability contracts release all acquired sources");
    return;
  }
  if (mode == "fleet-ability-gates") {
    Require(kAbilityButtons.group == &Config::Patches::abilitybuttonrecoveryhooks
                && kFleetActionMaintenance.group == &Config::Patches::hotkeyhooks,
            "registered recovery and housekeeping descriptors have independent startup owners");
    const auto registered_frame = [&](const Feature& feature) {
      // Model only the shared host's startup gate; invoke the actual selected consumer callback.
      if (config.patches.*feature.group)
        feature.frame();
    };
    config.patches.hotkeyhooks                = true;
    config.patches.abilitybuttonrecoveryhooks = false;
    registered_frame(kAbilityButtons);
    Require(fleet_sa_enables == 0 && fleet_sa_remaining_reads == 0,
            "a disabled recovery descriptor schedules no cooldown queries or button mutation");
    config.patches.hotkeyhooks                = false;
    config.patches.abilitybuttonrecoveryhooks = true;
    registered_frame(kAbilityButtons);
    Require(button_context.interactable && fleet_sa_enables == 1,
            "actual recovery frame remains usable independently of the hotkey patch group");
    Require(warp_roots.size() == roots, "independent recovery retains no native sources");
    return;
  }
  Require(mode == "fleet-ability-native", "unknown selection/ability fixture mode");
  RecoverAbilityButtons();
  Require(button_context.interactable && fleet_sa_enables == 1 && fleet_sa_seen_ability == ids.items[0]
              && fleet_sa_seen_fleet == reinterpret_cast<FleetPlayerData*>(&first),
          "parallel arrays use exact signed ability ID and selected fleet; extra unmatched ID is not read as a button");
  RecoverAbilityButtons();
  Require(fleet_sa_enables == 1, "already-interactable native context is not repeatedly written");
  const auto no_enable = [&](const char* reason) {
    button_context.interactable = false;
    const auto before           = fleet_sa_enables;
    RecoverAbilityButtons();
    Require(fleet_sa_enables == before && !button_context.interactable, reason);
  };
  timer.remaining.ticks = -9'999'999;
  no_enable("cooldown above the source minus-one-second threshold is not repaired");
  timer.remaining.ticks = -10'000'000;
  actions.effect        = true;
  no_enable("active ability effect preserves the native disabled state");
  actions.effect         = false;
  fleet_sa_effect_throws = true;
  no_enable("unavailable active-effect observation is unknown, not inactive");
  fleet_sa_effect_throws       = false;
  fleet_sa_interactable_throws = true;
  no_enable("unavailable interactability is not treated as a disabled button");
  fleet_sa_interactable_throws = false;
  fleet_sa_remaining_throws    = true;
  no_enable("failed remaining-time invocation does not fabricate an expired cooldown");
  fleet_sa_remaining_throws = false;
  fleet_sa_timer_throws     = true;
  no_enable("failed native cooldown acquisition does not authorize recovery");
  fleet_sa_timer_throws = false;
  fleet_sa_state_throws = true;
  no_enable("unknown fleet state does not satisfy the ability-activation mask");
  fleet_sa_state_throws = false;
  fleet_sa_ready        = false;
  no_enable("native ability rejection stays authoritative");
  fleet_sa_ready        = true;
  fleet_sa_ready_throws = true;
  no_enable("failed native availability is not successful readiness");
  fleet_sa_ready_throws = false;
  first.state           = FleetState::Docked;
  no_enable("fleet outside source CanActivateAbility mask is untouched");
  first.state    = FleetState::IdleInSpace;
  canvas.visible = false;
  no_enable("hidden ability panel is not repaired");
  canvas.visible = true;
  button.active  = false;
  no_enable("inactive native button is not repaired");
  button.active        = true;
  fleet_native_capture = true;
  no_enable("shortcut capture preserves the original pre-recovery exclusion");
  fleet_native_capture           = false;
  config.control.hotkeys_enabled = false;
  RecoverAbilityButtons();
  Require(button_context.interactable, "button repair remains active when mod hotkeys are disabled");
  button_context.interactable        = false;
  config.control.hotkeys_enabled     = true;
  config.control.use_scopely_hotkeys = true;
  RecoverAbilityButtons();
  Require(button_context.interactable, "button repair remains active in native-hotkey mode");
  config.control.use_scopely_hotkeys = false;
  button_context.interactable        = false;
  ids.header.klass                   = &wrong_ids_class.klass;
  no_enable("actual malformed ID vector is rejected even when the declared field is Int64[]");
  ids.header.klass     = &ids_array_class.klass;
  buttons.header.klass = &object_array_class.klass;
  no_enable("actual base-object vector cannot become a typed button array");
  buttons.header.klass = &buttons_array_class.klass;
  fleet_sa_on_ready    = [&] { panel.fleet = &second; };
  no_enable("native readiness rebind to another fleet cannot enable an old fleet button");
  panel.fleet       = &first;
  fleet_sa_on_ready = [&] { ++first.id; };
  no_enable("same native fleet object with a replaced ID cannot consume prior readiness");
  first.id          = 1001;
  fleet_sa_on_ready = [&] { button.context = &other_button_context; };
  no_enable("native readiness replacing the button context cannot enable its stale predecessor");
  Require(!other_button_context.interactable, "a stale repair does not transfer to the new button context");
  button.context    = &button_context;
  fleet_sa_on_ready = [&] { timer.remaining.ticks = 50'000'000; };
  no_enable("cooldown renewal during native availability wins over the earlier expired observation");
  timer.remaining.ticks = -10'000'000;
  fleet_sa_on_ready     = [&] { ids.items[0] = 43; };
  no_enable("replaced ability ID in the same owned array cannot receive stale readiness");
  ids.items[0]      = 9007199254740993LL;
  warp_root_failure = true;
  no_enable("failed custody leaves ability state native");
  warp_root_failure = false;
  fleet_native_widgets.clear();
  Require(warp_roots.size() == roots, "ability recovery retains no native roots after the operation");
  std::cout << "Production expired ability-button recovery, exact arrays/timers and stale-source fixtures passed\n";
}
} // namespace
