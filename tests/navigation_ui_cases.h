#pragma once

// Actual badge and search consumers in the existing controlled runtime. Only
// shortcut values and exported service names are adapted to avoid unrelated sinks.
namespace navigation_fixture_shortcuts
{
inline std::unordered_map<Action, std::string> hints;
inline bool                                    bound = true;
inline std::string                             Hint(Action action)
{ return hints[action]; }
inline bool HasBinding(Action)
{ return bound; }
} // namespace navigation_fixture_shortcuts
#define Shortcut navigation_fixture_shortcuts
#define InstallKeybindBadges NavigationFixtureInstallBadges
#define ToggleKeybindBadges NavigationFixtureToggleBadges
#define HideKeybindBadges NavigationFixtureHideBadges
#include "../src/mod/features/keybind_badges.cc"
#undef HideKeybindBadges
#undef ToggleKeybindBadges
#undef InstallKeybindBadges
#undef Shortcut
#include "game/base_card.h"
namespace navigation_fixture_base_card
{
bool Available();
bool KeepOpenForRecall(std::int64_t fleet);
} // namespace navigation_fixture_base_card
#define base_card navigation_fixture_base_card
#define kBaseCard kNavigationFixtureBaseCard
#include "../src/mod/features/base_card.cc"
#undef kBaseCard
#undef base_card

namespace
{
struct NavigationUiNode {
  Il2CppObject  header{};
  Il2CppObject *first = nullptr, *second = nullptr;
  bool          active = true;
};
struct NavigationUiLabel {
  Il2CppObject header{};
  std::string  text   = "native";
  int          clears = 0, overrides = 0;
};
std::function<void()> navigation_ui_on_active;
int                   navigation_ui_focus = 0, navigation_ui_writes = 0;
bool                  navigation_ui_gate = true, navigation_ui_throw_visibility = false;
NavigationUiNode*     navigation_ui_manager = nullptr;
void                  NavigationUiClear(NavigationUiLabel* label, const MethodInfo*)
{
  ++label->clears;
  label->text = "native";
}
void NavigationUiOverride(NavigationUiLabel* label, Il2CppString* text, const MethodInfo*)
{
  ++label->overrides;
  label->text = il2cpp::ToUtf8(text);
}
Il2CppObject* NavigationUiFirst(NavigationUiNode* node, const MethodInfo*)
{ return node->first; }
bool NavigationUiActive(NavigationUiNode* node, const MethodInfo*)
{
  if (navigation_ui_on_active)
    std::exchange(navigation_ui_on_active, {})();
  return node->active;
}
void NavigationUiFocus(Il2CppObject*, const MethodInfo*)
{ ++navigation_ui_focus; }
Il2CppObject* NavigationUiManager(const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(navigation_ui_manager); }
bool NavigationUiGate(const MethodInfo*)
{ return navigation_ui_gate; }
bool NavigationUiShown(NavigationUiNode* node, const MethodInfo*)
{
  if (navigation_ui_throw_visibility)
    throw Il2CppExceptionWrapper(&thrown);
  return node->active;
}
void NavigationUiShow(NavigationUiNode* node, bool visible, const MethodInfo*)
{
  node->active = visible;
  ++navigation_ui_writes;
}

void NavigationBadgeCases()
{
  auto& label     = Define("Digit.Client.UI", "TextLocalizer", IL2CPP_TYPE_CLASS, &object_class);
  auto& action    = Define("UnityEngine.InputSystem", "InputAction", IL2CPP_TYPE_CLASS, &object_class);
  auto& reference = Define("UnityEngine.InputSystem", "InputActionReference", IL2CPP_TYPE_CLASS, &object_class);
  auto& badge     = Define("Digit.Prime.GameInput", "ShortcutKeybindHint", IL2CPP_TYPE_CLASS, &object_class);
  AddField(badge, "_keyTextLocalizer", label, offsetof(NavigationUiNode, first));
  AddField(badge, "_inputAction", reference, offsetof(NavigationUiNode, second));
  AddMethod(label, "ClearTextOverride", reinterpret_cast<void*>(&NavigationUiClear), T(void_class), {});
  AddMethod(label, "OverrideLocalizedText", reinterpret_cast<void*>(&NavigationUiOverride), T(void_class),
            {T(string_class)});
  auto& get_action = AddMethod(reference, "get_action", reinterpret_cast<void*>(&NavigationUiFirst), T(action), {});
  AddProperty(reference, "action", &get_action);
  auto& get_name = AddMethod(action, "get_name", reinterpret_cast<void*>(&NavigationUiFirst), T(string_class), {});
  AddProperty(action, "name", &get_name);
  NavigationUiLabel old_label{{&label.klass}}, new_label{{&label.klass}};
  NavigationUiNode  native_action{{&action.klass}, reinterpret_cast<Il2CppObject*>(il2cpp::NewString("side_chat"))};
  NavigationUiNode  native_ref{{&reference.klass}, &native_action.header};
  NavigationUiNode  native_badge{{&badge.klass}, &old_label.header, &native_ref.header};
  auto*             owner                                     = reinterpret_cast<ShortcutKeybindHint*>(&native_badge);
  Config::Get().control.hotkeys_enabled                       = true;
  Config::Get().control.use_scopely_hotkeys                   = false;
  navigation_fixture_shortcuts::hints[Action::kShowChatSide2] = "FALLBACK";
  int  originals                                              = 0;
  auto native_text                                            = [&](auto*) { ++originals; };
  ShortcutKeybindHint_UpdateText_Hook(native_text, owner);
  Require(originals == 1 && old_label.text == "FALLBACK", "side chat uses fallback only when primary is empty");
  navigation_fixture_shortcuts::hints[Action::kShowChatSide1] = "PRIMARY";
  ShortcutKeybindHint_UpdateText_Hook(native_text, owner);
  Require(old_label.text == "PRIMARY", "remapped primary hint replaces fallback on refresh");
  Config::Get().control.use_scopely_hotkeys = true;
  ShortcutKeybindHint_UpdateText_Hook(native_text, owner);
  Require(old_label.text == "native", "native mode clears previous mod override before original text update");
  Config::Get().control.use_scopely_hotkeys = false;
  const auto old_overrides                  = old_label.overrides;
  ShortcutKeybindHint_UpdateText_Hook(
      [&](auto*) {
        ++originals;
        native_badge.first = &new_label.header;
      },
      owner);
  Require(old_label.overrides == old_overrides, "native rebind must not write into the previous badge label");
  native_badge.first = &new_label.header;
  ShortcutKeybindHint_UpdateText_Hook(native_text, owner);
  Require(new_label.text == "PRIMARY", "subsequent binding refresh updates current label");
  native_action.first = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("not_a_native_action"));
  ShortcutKeybindHint_UpdateText_Hook(native_text, owner);
  Require(new_label.text == "-", "unknown action retains source dash presentation");

  auto& manager     = Define("Digit.Prime.GameInput", "ShortcutsManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& get_manager = AddMethod(manager, "get_Instance", reinterpret_cast<void*>(&NavigationUiManager), T(manager), {},
                                METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager, "Instance", &get_manager);
  auto& gate = AddMethod(manager, "get_CanUseShortcuts", reinterpret_cast<void*>(&NavigationUiGate), T(bool_class), {},
                         METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager, "CanUseShortcuts", &gate);
  auto& show =
      AddMethod(manager, "get_ShowKeybindings", reinterpret_cast<void*>(&NavigationUiShown), T(bool_class), {});
  auto& set_show = AddMethod(manager, "set_ShowKeybindings", reinterpret_cast<void*>(&NavigationUiShow), T(void_class),
                             {T(bool_class)});
  AddProperty(manager, "ShowKeybindings", &show, &set_show);
  NavigationUiNode native_manager{{&manager.klass}};
  native_manager.active = false;
  navigation_ui_manager = &native_manager;
  badges_installed      = true;
  Require(NavigationFixtureToggleBadges() && native_manager.active && navigation_ui_writes == 1,
          "toggle uses native visibility setter");
  navigation_ui_gate = false;
  Require(!NavigationFixtureToggleBadges() && navigation_ui_writes == 1,
          "native gate refusal does not claim a handled toggle");
  navigation_ui_gate             = true;
  navigation_ui_throw_visibility = true;
  Require(!NavigationFixtureToggleBadges() && navigation_ui_writes == 1,
          "unknown current visibility cannot authorize a guessed toggle");
  navigation_ui_throw_visibility = false;
  NavigationFixtureHideBadges();
  Require(!native_manager.active && navigation_ui_writes == 2, "mode switch hides known visible native hints");
  Require(warp_roots.empty(), "badge operations release temporary managed custody");
  std::cout << "Production shortcut badge mapping, replacement and visibility cases passed\n";
}

void NavigationSearchCases()
{
  ScreenActionsNativeCases("screen-actions-dispatch", [] {
    auto& behaviour  = Fake(OfficerAssignmentViewController::Class::Raw()->parent);
    auto& inventory  = Define("Digit.Prime.Inventories", "InventoryListViewController", IL2CPP_TYPE_CLASS, &behaviour);
    auto& input      = Define("Digit.Client.UI", "InputFieldWidget", IL2CPP_TYPE_CLASS, &behaviour);
    auto& assignment = Fake(OfficerAssignmentViewController::Class::Raw());
    auto& ships      = Define("Digit.Prime.Ships", "AssignShipsWidget", IL2CPP_TYPE_CLASS, &behaviour);
    for (auto* owner : {&inventory, &assignment, &ships})
      AddField(*owner, "_inputField", input, offsetof(ScreenActionsController, ship));
    AddMethod(input, "Focus", reinterpret_cast<void*>(&NavigationUiFocus), T(void_class), {});
    ScreenActionsController field{{&input.klass}}, replacement{{&input.klass}};
    ScreenActionsController row{{&inventory.klass}, reinterpret_cast<unity::Button*>(&field)};
    fleet_native_widgets[&inventory.klass] = {&row.header};
    Config::Get().ui.focus_search          = true;
    Require(FocusSearchBox() && navigation_ui_focus == 1, "visible inventory search receives native focus once");
    Config::Get().ui.focus_search = false;
    Require(!FocusSearchBox() && navigation_ui_focus == 1, "disabled search does not act");
    Config::Get().ui.focus_search = true;
    field.active                  = false;
    Require(!FocusSearchBox(), "inactive search field does not receive focus");
    field.active = true;
    screen_actions_active_callback = [&](ScreenActionsController* observed) {
      if (observed == &field) row.ship = reinterpret_cast<unity::Button*>(&replacement);
    };
    Require(!FocusSearchBox() && navigation_ui_focus == 1, "replaced field cannot receive stale focus");
    screen_actions_active_callback = {};
    Require(FocusSearchBox() && navigation_ui_focus == 2, "subsequent bind focuses the replacement field");
    screen_actions_visible = false;
    Require(!FocusSearchBox(), "hidden pooled screen leaves focus unchanged");
    screen_actions_visible = true;
    fleet_native_widgets[&inventory.klass].clear();
    row.header.klass                        = &assignment.klass;
    fleet_native_widgets[&assignment.klass] = {&row.header};
    Require(FocusSearchBox() && navigation_ui_focus == 3, "officer assignment uses its separate native search field");
    fleet_native_widgets[&assignment.klass].clear();
    row.header.klass                   = &ships.klass;
    fleet_native_widgets[&ships.klass] = {&row.header};
    Require(FocusSearchBox() && navigation_ui_focus == 4, "ship assignment remains the final search fallback");
    fleet_native_widgets[&ships.klass].clear();
  });
  std::cout << "Production inventory/officer/ship search visibility and replacement cases passed\n";
}

struct NavigationCardData {
  Il2CppObject  header{};
  int64_t       player = 9007199254740993LL, target = 77;
  Il2CppString* user = nullptr;
  bool          base = true, own = false, valid = true;
  Il2CppObject* deployed = nullptr;
  Il2CppObject* plan     = nullptr;
};
struct NavigationCardFleet {
  Il2CppObject      header{};
  int64_t           id    = 77;
  DeployedFleetType type  = DeployedFleetType::Player;
  bool              local = false, destroyed = false;
};
struct NavigationCardPoi {
  Il2CppObject header{};
  int64_t      id   = 77;
  int32_t      kind = 5; // Native InteractableType.Fleet.
};
using NavigationCardTargetId = il2cpp::Nullable<int64_t>;
struct NavigationCardPlan {
  Il2CppObject           header{};
  int64_t                fleet  = 9007199254740993LL;
  int32_t                action = 2; // Native TargetAction.Attack.
  NavigationCardTargetId target{true, 77};
};
struct NavigationCardLongBox {
  Il2CppObject header{};
  int64_t      value = 0;
} navigation_card_box;
Il2CppObject          navigation_card_integer_box{};
NavigationUiNode*     navigation_card_manager = nullptr;
std::function<void()> navigation_card_on_context;
std::function<void()> navigation_card_on_identity;
std::string           navigation_card_identity_stage, navigation_card_failure;
bool                  navigation_card_unknown_player = false;
void                  NavigationCardIdentityRead(std::string_view stage)
{
  if (navigation_card_failure == stage)
    throw Il2CppExceptionWrapper(&thrown);
  if (navigation_card_identity_stage == stage && navigation_card_on_identity)
    std::exchange(navigation_card_on_identity, {})();
}
bool                  NavigationCardBase(NavigationCardData* value, const MethodInfo*)
{ return value->base; }
bool NavigationCardOwn(NavigationCardData* value, const MethodInfo*)
{ return value->own; }
bool NavigationCardValid(NavigationCardData* value, const MethodInfo*)
{ return value->valid; }
int64_t NavigationCardPlayer(NavigationCardData* value, const MethodInfo*)
{
  if (navigation_card_unknown_player)
    throw std::runtime_error("controlled player getter unavailable");
  return value->player;
}
int64_t NavigationCardTarget(NavigationCardData* value, const MethodInfo*)
{ return value->target; }
Il2CppString* NavigationCardUser(NavigationCardData* value, const MethodInfo*)
{
  auto* user = value->user;
  NavigationCardIdentityRead("target-user");
  return user;
}
int64_t NavigationCardFleetId(NavigationCardFleet* value, const MethodInfo*)
{
  const auto id = value->id;
  NavigationCardIdentityRead("fleet-id");
  return id;
}
DeployedFleetType NavigationCardFleetType(NavigationCardFleet* value, const MethodInfo*)
{
  const auto type = value->type;
  NavigationCardIdentityRead("fleet-type");
  return type;
}
bool NavigationCardFleetLocal(NavigationCardFleet* value, const MethodInfo*)
{
  const auto local = value->local;
  NavigationCardIdentityRead("fleet-local");
  return local;
}
bool NavigationCardFleetDestroyed(NavigationCardFleet* value, const MethodInfo*)
{
  const auto destroyed = value->destroyed;
  NavigationCardIdentityRead("fleet-destroyed");
  return destroyed;
}
int64_t NavigationCardPoiId(NavigationCardPoi* value, const MethodInfo*)
{
  const auto id = value->id;
  NavigationCardIdentityRead("poi-id");
  return id;
}
int32_t NavigationCardPoiKind(NavigationCardPoi* value, const MethodInfo*)
{
  const auto kind = value->kind;
  NavigationCardIdentityRead("poi-kind");
  return kind;
}
int64_t NavigationCardPlanFleet(NavigationCardPlan* value, const MethodInfo*)
{
  const auto fleet = value->fleet;
  NavigationCardIdentityRead("plan-fleet");
  return fleet;
}
int32_t NavigationCardPlanAction(NavigationCardPlan* value, const MethodInfo*)
{
  const auto action = value->action;
  NavigationCardIdentityRead("plan-action");
  return action;
}
void          NavigationCardNoop() {}
Il2CppObject* NavigationCardInvoke(const MethodInfo* method, void* target, void**, Il2CppException** exception,
                                   bool& handled)
{
  const std::string_view name(method->name);
  handled = true;
  if (name == "get_Context") {
    if (navigation_card_on_context)
      std::exchange(navigation_card_on_context, {})();
    return static_cast<NavigationUiNode*>(target)->second;
  }
  if (name == "get_CanvasContext" || name == "get_TargetDataCache")
    return static_cast<NavigationUiNode*>(target)->first;
  if (name == "get_Instance")
    return reinterpret_cast<Il2CppObject*>(navigation_card_manager);
  if (name == "get_PlayerID" || name == "get_TargetID") {
    try {
      auto* data = static_cast<NavigationCardData*>(target);
      navigation_card_box.value =
          name == "get_PlayerID" ? NavigationCardPlayer(data, method) : NavigationCardTarget(data, method);
      return &navigation_card_box.header;
    } catch (const std::exception&) {
      *exception = &thrown;
      return nullptr;
    }
  }
  if (name == "get_ID") {
    try {
      const std::string_view type(method->klass->name);
      navigation_card_box.value = type == "POI"
                                      ? NavigationCardPoiId(static_cast<NavigationCardPoi*>(target), method)
                                      : NavigationCardFleetId(static_cast<NavigationCardFleet*>(target), method);
      return &navigation_card_box.header;
    } catch (const Il2CppExceptionWrapper&) {
      *exception = &thrown;
      return nullptr;
    }
  }
  if (name == "get_FleetID" || name == "get_TargetAction") {
    try {
      auto* plan = static_cast<NavigationCardPlan*>(target);
      if (name == "get_TargetAction") {
        shared_result = NavigationCardPlanAction(plan, method);
        return &navigation_card_integer_box;
      }
      navigation_card_box.value = NavigationCardPlanFleet(plan, method);
      return &navigation_card_box.header;
    } catch (const Il2CppExceptionWrapper&) {
      *exception = &thrown;
      return nullptr;
    }
  }
  if (name == "get_TargetUserID") {
    try {
      return reinterpret_cast<Il2CppObject*>(NavigationCardUser(static_cast<NavigationCardData*>(target), method));
    } catch (const Il2CppExceptionWrapper&) {
      *exception = &thrown;
      return nullptr;
    }
  }
  handled = false;
  return nullptr;
}
void NavigationCardCases()
{
  auto& data    = Define("Digit.PrimeServer.Models", "BattleTargetData", IL2CPP_TYPE_CLASS, &object_class);
  auto& context = Fake(NavigationInteractionUIContext::Class::Raw());
  auto& poi     = Define("Digit.Prime.Navigation", "POI", IL2CPP_TYPE_CLASS, &object_class);
  auto& preview = Define("Digit.Prime.Combat", "PreScanTargetWidget", IL2CPP_TYPE_CLASS, &object_class);
  auto& nav =
      Define("Digit.Prime.Navigation", "NavigationInteractionUIViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& scanning = Define("Digit.Prime.Navigation.Managers", "ScanningManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& plan     = warp_course_class;
  auto& nullable_long =
      Define("System", "Nullable`1", IL2CPP_TYPE_GENERICINST, nullptr, sizeof(NavigationCardTargetId));
  nullable_long.value_alignment = alignof(NavigationCardTargetId);
  std::array<const Il2CppType*, 1> long_arguments{T(long_class)};
  Il2CppGenericInst                long_inst{};
  long_inst.type_argc = 1;
  long_inst.type_argv = long_arguments.data();
  Il2CppGenericClass long_generic{};
  long_generic.type                                = T(nullable_definition);
  long_generic.context.class_inst                  = &long_inst;
  nullable_long.klass.byval_arg.data.generic_class = &long_generic;
  nullable_long.klass.generic_class                = &long_generic;
  AddField(nullable_long, "hasValue", bool_class, sizeof(Il2CppObject) + offsetof(NavigationCardTargetId, has_value));
  AddField(nullable_long, "value", long_class, sizeof(Il2CppObject) + offsetof(NavigationCardTargetId, value));
  auto& deployed     = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& fleet_type   = Define("Digit.PrimeServer.Models", "DeployedFleetType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& poi_type     = Define("Digit.Prime.Navigation", "InteractableType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  fleet_type.is_enum = poi_type.is_enum = true;
  auto& response = Define("Digit.PrimeServer.Events", "SetCourseResponseEventArgs", IL2CPP_TYPE_VALUETYPE, nullptr,
                          sizeof(SetCourseResponse));
  AddField(response, "<FleetId>k__BackingField", long_class, 0x10);
  AddField(response, "<Success>k__BackingField", bool_class, 0x18);
  AddField(response, "<IsRecall>k__BackingField", bool_class, 0x19);
  AddField(response, "<TargetId>k__BackingField", object_class, 0x20);
  AddMethod(preview, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddMethod(nav, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddField(preview, "_battleTargetData", data, offsetof(NavigationUiNode, first));
  AddField(data, "TargetFleetDeployedData", deployed, offsetof(NavigationCardData, deployed));
  AddField(data, "<PlannedCourseToTarget>k__BackingField", plan, offsetof(NavigationCardData, plan));
  AddField(plan, "<TargetDeployedFleetId>k__BackingField", nullable_long, offsetof(NavigationCardPlan, target));
  // The shared fixture's class_from_type recognizes constructed types by the original descriptor. This field
  // retains that same valid SDK descriptor rather than changing its protected runtime API for a single case.
  for (auto& field : plan.fields)
    if (std::string_view(field.name) == "<TargetDeployedFleetId>k__BackingField")
      field.type = T(nullable_long);
  AddField(context, "Poi", poi, offsetof(NavigationUiNode, first));
  AddField(context, "UserId", string_class, offsetof(NavigationUiNode, second));
  preview.klass.instance_size = context.klass.instance_size = sizeof(NavigationUiNode);
  data.klass.instance_size                                  = sizeof(NavigationCardData);
  poi.klass.instance_size                                   = sizeof(NavigationCardPoi);
  deployed.klass.instance_size                              = sizeof(NavigationCardFleet);
  plan.klass.instance_size                                  = sizeof(NavigationCardPlan);
  const auto reference_get                                  = [&](FakeClass& owner, const char* name, FakeClass& result,
                                                                  bool is_static = false) -> MethodInfo& {
    auto& method = AddMethod(owner, name, reinterpret_cast<void*>(&NavigationCardNoop), T(result), {},
                             METHOD_ATTRIBUTE_PUBLIC | (is_static ? METHOD_ATTRIBUTE_STATIC : 0));
    method.invoker_method =
        reinterpret_cast<InvokerMethod>(&NavigationCardNoop); // Runtime substitute, never directly invoked.
    return method;
  };
  auto& preview_context = reference_get(preview, "get_Context", context);
  AddProperty(preview, "Context", &preview_context);
  auto& nav_context = reference_get(nav, "get_CanvasContext", context);
  AddProperty(nav, "CanvasContext", &nav_context);
  reference_get(scanning, "get_Instance", scanning, true);
  reference_get(scanning, "get_TargetDataCache", data);
  AddMethod(data, "get_IsStarbase", reinterpret_cast<void*>(&NavigationCardBase), T(bool_class), {});
  AddMethod(data, "get_IsOwnStation", reinterpret_cast<void*>(&NavigationCardOwn), T(bool_class), {});
  AddMethod(data, "get_IsValid", reinterpret_cast<void*>(&NavigationCardValid), T(bool_class), {});
  AddMethod(poi, "get_ID", reinterpret_cast<void*>(&NavigationCardPoiId), T(long_class), {});
  AddMethod(poi, "get_InteractableType", reinterpret_cast<void*>(&NavigationCardPoiKind), T(poi_type), {});
  AddMethod(deployed, "get_ID", reinterpret_cast<void*>(&NavigationCardFleetId), T(long_class), {});
  AddMethod(deployed, "get_FleetType", reinterpret_cast<void*>(&NavigationCardFleetType), T(fleet_type), {});
  AddMethod(deployed, "get_IsLocalPlayer", reinterpret_cast<void*>(&NavigationCardFleetLocal), T(bool_class), {});
  AddMethod(deployed, "get_IsDestroyed", reinterpret_cast<void*>(&NavigationCardFleetDestroyed), T(bool_class), {});
  const auto replace_method = [&](FakeClass& owner, const char* name, void* function, FakeClass& result) {
    for (const auto* candidate : owner.methods)
      if (std::string_view(candidate->name) == name) {
        auto* method          = const_cast<MethodInfo*>(candidate);
        method->methodPointer = method->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(function);
        return;
      }
    AddMethod(owner, name, function, T(result), {});
  };
  replace_method(plan, "get_FleetID", reinterpret_cast<void*>(&NavigationCardPlanFleet), long_class);
  replace_method(plan, "get_TargetAction", reinterpret_cast<void*>(&NavigationCardPlanAction), int_class);
  const auto nullable_layout = [&] { return base_card_binding::NullableFleetIdLayout(T(nullable_long)); };
  Require(nullable_layout(), "the plan target qualifies as constructed corlib Nullable<Int64>");
  long_generic.type = T(foreign_nullable_definition);
  Require(!nullable_layout(), "a same-named foreign nullable definition cannot qualify a deployed-fleet target");
  long_generic.type = T(nullable_definition);
  long_arguments[0] = T(unsigned_long_class);
  Require(!nullable_layout(), "Nullable<UInt64> cannot stand in for the signed deployed-fleet identity");
  long_arguments[0] = nullptr;
  Require(!nullable_layout(), "a missing nullable payload type cannot establish signed fleet identity");
  long_arguments[0]   = T(long_class);
  long_inst.type_argc = 0;
  Require(!nullable_layout(), "a nullable without its generic argument does not qualify");
  long_inst.type_argc = 1;
  nullable_long.value_size += 8;
  Require(!nullable_layout(), "wrong nullable target allocation size cannot qualify");
  nullable_long.value_size      = sizeof(NavigationCardTargetId);
  nullable_long.value_alignment = 4;
  Require(!nullable_layout(), "wrong nullable target alignment is unavailable rather than an empty target");
  nullable_long.value_alignment       = alignof(NavigationCardTargetId);
  nullable_long.klass.byval_arg.byref = 1;
  Require(!nullable_layout(), "a byref nullable is not inline deployed-fleet target storage");
  nullable_long.klass.byval_arg.byref = 0;
  auto*      flag                     = &nullable_long.fields[0];
  auto*      value                    = &nullable_long.fields[1];
  const auto original_flag = *flag->type, original_value = *value->type;
  const auto flag_offset = flag->offset, value_offset = value->offset;
  const_cast<Il2CppType*>(flag->type)->type = IL2CPP_TYPE_I1;
  Require(!nullable_layout(), "a same-sized byte cannot stand in for the nullable Boolean flag");
  *const_cast<Il2CppType*>(flag->type)  = original_flag;
  *const_cast<Il2CppType*>(value->type) = *T(unsigned_long_class);
  Require(!nullable_layout(), "an unsigned payload field cannot qualify despite a signed generic argument");
  *const_cast<Il2CppType*>(value->type) = original_value;
  for (auto attrs : {FIELD_ATTRIBUTE_STATIC, FIELD_ATTRIBUTE_LITERAL}) {
    const_cast<Il2CppType*>(value->type)->attrs |= attrs;
    Require(!nullable_layout(), "static or literal payload metadata is not inline nullable storage");
    *const_cast<Il2CppType*>(value->type) = original_value;
  }
  const_cast<Il2CppType*>(value->type)->byref = 1;
  Require(!nullable_layout(), "a byref payload field cannot supply an inline deployed-fleet identity");
  *const_cast<Il2CppType*>(value->type) = original_value;
  ++flag->offset;
  Require(!nullable_layout(), "a malformed nullable target flag layout cannot qualify");
  flag->offset = flag_offset;
  ++value->offset;
  Require(!nullable_layout(), "a malformed nullable target value layout cannot qualify");
  value->offset = value_offset;
  Require(nullable_layout(), "restoring valid signed nullable metadata restores deployed-fleet target qualification");
  auto& player = AddMethod(data, "get_PlayerID", reinterpret_cast<void*>(&NavigationCardPlayer), T(long_class), {});
  auto& target = AddMethod(data, "get_TargetID", reinterpret_cast<void*>(&NavigationCardTarget), T(long_class), {});
  auto& user   = AddMethod(data, "get_TargetUserID", reinterpret_cast<void*>(&NavigationCardUser), T(string_class), {});
  player.invoker_method = target.invoker_method = user.invoker_method =
      reinterpret_cast<InvokerMethod>(&NavigationCardNoop);
  NavigationCardData recorded{{&data.klass}}, replacement = recorded;
  recorded.user    = il2cpp::NewString("enemy");
  replacement.user = recorded.user;
  NavigationCardPoi native_poi{{&poi.klass}};
  NavigationUiNode native_context{{&context.klass}, &native_poi.header, reinterpret_cast<Il2CppObject*>(recorded.user)};
  NavigationUiNode native_preview{{&preview.klass}, &recorded.header, &native_context.header};
  NavigationUiNode native_nav{{&nav.klass}, &native_context.header};
  NavigationUiNode native_scanning{{&scanning.klass}, &recorded.header};
  navigation_card_manager          = &native_scanning;
  navigation_card_box.header.klass = &long_class.klass;
  navigation_card_integer_box.klass = &int_class.klass;
  auto saved_override              = tow_runtime_invoke_override;
  tow_runtime_invoke_override      = &NavigationCardInvoke;
  NavigationCardLongBox target_box{{&long_class.klass}, 77};
  extra_value_box = &target_box.header;
  extra_value_payload = &target_box.value;
  second_value_box = &navigation_card_box.header;
  second_value_payload = &navigation_card_box.value;
  SetCourseResponse     event{recorded.player, true, false, reinterpret_cast<il2cpp::Object*>(&target_box)};
  auto*                 widget = reinterpret_cast<PreScanTargetWidget*>(&native_preview);
  const auto& card_api = base_card_binding::Native();
  if (!card_api.ready) {
    std::cerr << "card setup: layouts=" << base_card_binding::ResponseLayout(card_api.preview) << "/"
              << base_card_binding::ResponseLayout(card_api.navigation)
              << " fields=" << bool(card_api.preview_data) << bool(card_api.poi) << bool(card_api.user)
              << " getters=" << bool(card_api.preview_context) << bool(card_api.navigation_context)
              << " scanning=" << bool(card_api.scanning_instance) << bool(card_api.target_cache) << "\n";
    for (const auto& error : errors) std::cerr << error << "\n";
  }
  Require(card_api.ready,
          "actual station-card metadata/ABI qualification succeeds before evidence checks");
  Require(base_card_binding::RetainPreview(widget, event), "matching exact fleet/target/enemy/context retains preview");
  Require(
      base_card_binding::RetainNavigation(reinterpret_cast<NavigationInteractionUIViewController*>(&native_nav), event),
      "navigation uses the same current scanning evidence and context");
  target_box.value = 78;
  Require(!base_card_binding::RetainPreview(widget, event), "different target cannot retain the station card");
  target_box.value = 77;
  recorded.own     = true;
  Require(!base_card_binding::RetainPreview(widget, event), "own station remains native");
  recorded.own  = false;
  event.success = false;
  Require(!base_card_binding::RetainPreview(widget, event), "failed course response preserves native dismissal");
  event.success   = true;
  event.is_recall = true;
  event.target_id = nullptr;
  Require(base_card_binding::RetainPreview(widget, event), "matching recall may have no target payload");
  navigation_card_unknown_player = true;
  Require(!base_card_binding::RetainPreview(widget, event), "unavailable player identity cannot authorize retention");
  navigation_card_unknown_player = false;
  navigation_card_on_context     = [&] { native_preview.first = &replacement.header; };
  Require(!base_card_binding::RetainPreview(widget, event),
          "preview replacement during context acquisition prevents stale retention");

  native_preview.first = &recorded.header;
  event.is_recall      = false;
  event.target_id      = reinterpret_cast<il2cpp::Object*>(&target_box);
  NavigationCardFleet native_fleet{{&deployed.klass}}, replacement_fleet = native_fleet;
  NavigationCardPoi   replacement_poi     = native_poi;
  NavigationUiNode    replacement_context = native_context;
  NavigationCardPlan  native_plan{{&plan.klass}}, replacement_plan = native_plan;
  recorded.base     = false;
  recorded.deployed = &native_fleet.header;
  recorded.plan     = &native_plan.header;
  recorded.target = native_fleet.id = native_poi.id = target_box.value = 9007199254740997LL;
  native_plan.target.value                                             = recorded.target;
  replacement_plan.target.value                                        = recorded.target + 1;
  replacement_fleet.id = replacement_poi.id = recorded.target + 1;
  replacement_context.first                 = &replacement_poi.header;
  replacement.base                          = false;
  replacement.deployed                      = &replacement_fleet.header;
  replacement.target                        = replacement_fleet.id;
  const auto retain_ship            = [&] { return base_card_binding::RetainPreview(widget, event, false, true); };
  auto*      navigation             = reinterpret_cast<NavigationInteractionUIViewController*>(&native_nav);
  const auto retain_navigation_ship = [&] {
    return base_card_binding::RetainNavigation(navigation, event, false, true);
  };
  Require(retain_ship() && retain_navigation_ship(),
          "exact other-player fleet and displayed POI retain both cards without truncating large IDs");
  Require(!base_card_binding::RetainPreview(widget, event)
              && !base_card_binding::RetainPreview(widget, event, false, false),
          "the existing station default and disabled flags do not opt player ships into retention");

  native_poi.id = recorded.target + 1;
  Require(!retain_ship() && !retain_navigation_ship(),
          "another displayed ship owned by the same user cannot inherit an attack response");
  native_poi.id   = recorded.target;
  native_fleet.id = recorded.target + 1;
  Require(!retain_ship(), "a different deployed fleet cannot inherit the displayed ship's response");
  native_fleet.id  = recorded.target;
  target_box.value = recorded.target + 1;
  Require(!retain_ship(), "a response for another ship cannot retain this player card");
  target_box.value = recorded.target;
  event.target_id  = reinterpret_cast<il2cpp::Object*>(recorded.user);
  Require(!retain_ship(), "a shared owner string is not the exact ship identity required for retention");
  Il2CppObject wrong_box{&int_class.klass};
  event.target_id = reinterpret_cast<il2cpp::Object*>(&wrong_box);
  Require(!retain_ship(), "a non-Int64 response box cannot be interpreted as a deployed fleet ID");
  event.target_id = nullptr;
  Require(!retain_ship(), "a response with no target cannot retain a player ship card");
  event.target_id = reinterpret_cast<il2cpp::Object*>(&target_box);
  ++event.fleet_id;
  Require(!retain_ship(), "a response for another attacking fleet cannot retain the current ship card");
  event.fleet_id             = recorded.player;
  const auto attacking_fleet = recorded.player;
  for (const auto id : {int64_t{0}, int64_t{-1}}) {
    recorded.player = event.fleet_id = native_plan.fleet = id;
    Require(!retain_ship(), "nonpositive attacking-fleet IDs cannot authorize player-ship retention");
  }
  recorded.player = event.fleet_id = native_plan.fleet = attacking_fleet;
  for (const auto type : {DeployedFleetType::Marauder, DeployedFleetType::Alliance, DeployedFleetType::Nonexistent}) {
    native_fleet.type = type;
    Require(!retain_ship(), "hostiles and non-player deployed fleets keep their native dismissal behavior");
  }
  native_fleet.type  = DeployedFleetType::Player;
  native_fleet.local = true;
  Require(!retain_ship(), "own deployed fleets are not retained by the player attack option");
  native_fleet.local     = false;
  native_fleet.destroyed = true;
  Require(!retain_ship(), "a destroyed target cannot be kept through attack-response retention");
  native_fleet.destroyed = false;
  native_poi.kind        = 0;
  Require(!retain_ship(), "a non-fleet POI cannot be retained using a coincidentally equal ID");
  native_poi.kind    = 5;
  visual_dead_object = &native_poi.header;
  Require(!retain_ship(), "a destroyed POI cannot authorize player-ship card retention");
  visual_dead_object = nullptr;
  visual_throws      = true;
  Require(!retain_ship(), "unavailable POI liveness is not proof of a currently displayed target");
  visual_throws  = false;
  recorded.valid = false;
  Require(!retain_ship(), "an invalid battle target keeps native dismissal");
  recorded.valid = true;
  event.success  = false;
  Require(!retain_ship(), "a failed attack response keeps native dismissal");
  event.success   = true;
  event.is_recall = true;
  Require(!retain_ship() && !retain_navigation_ship(), "ship retention never extends to recall responses");
  event.is_recall = false;
  for (const auto id : {int64_t{0}, int64_t{-1}}) {
    native_poi.id = native_fleet.id = recorded.target = target_box.value = native_plan.target.value = id;
    Require(!retain_ship(), "zero and negative IDs cannot establish a player-ship target");
  }
  native_poi.id = native_fleet.id = recorded.target = target_box.value = native_plan.target.value = 9007199254740997LL;
  recorded.deployed                                                                               = nullptr;
  Require(!retain_ship(), "missing deployed-fleet evidence cannot be treated as a player ship");
  recorded.deployed = &native_fleet.header;
  recorded.plan     = nullptr;
  Require(!retain_ship(), "a response alone cannot prove that the player-ship action was Attack");
  recorded.plan = &native_plan.header;
  for (const auto action : {int32_t{0}, int32_t{1}, int32_t{3}}) {
    native_plan.action = action;
    Require(!retain_ship() && !retain_navigation_ship(), "non-Attack plans keep native card dismissal");
  }
  native_plan.action = 2;
  ++native_plan.fleet;
  Require(!retain_ship(), "a plan owned by another attacking fleet cannot authorize retention");
  native_plan.fleet            = recorded.player;
  native_plan.target.has_value = false;
  Require(!retain_ship(), "an Attack plan with no deployed-fleet target cannot retain a ship");
  native_plan.target.has_value = true;
  native_plan.target.value     = 0;
  Require(!retain_ship(), "a present zero deployed-fleet target cannot establish a ship identity");
  native_plan.target.value = recorded.target + 1;
  Require(!retain_ship(), "a stale Attack plan for another deployed fleet does not qualify after retargeting");
  native_plan.target.value = recorded.target;
  for (const char* field : {"poi-id", "poi-kind", "fleet-id", "fleet-type", "fleet-local", "fleet-destroyed",
                            "plan-fleet", "plan-action", "target-user"}) {
    navigation_card_failure = field;
    Require(!retain_ship() && !retain_navigation_ship(),
            "an unavailable POI or deployed-fleet observation cannot authorize either retention path");
  }
  navigation_card_failure.clear();
  navigation_card_unknown_player = true;
  Require(!retain_ship(), "unavailable attacking-fleet identity cannot retain a player ship card");
  navigation_card_unknown_player = false;

  navigation_card_identity_stage = "fleet-id";
  navigation_card_on_identity    = [&] { native_context.first = &replacement_poi.header; };
  Require(!retain_ship(), "POI replacement inside an identity getter cannot retain the old displayed ship");
  native_context.first        = &native_poi.header;
  navigation_card_on_identity = [&] { recorded.deployed = &replacement_fleet.header; };
  Require(!retain_ship(), "deployed-data replacement inside an identity getter cannot retain the previous ship");
  recorded.deployed           = &native_fleet.header;
  navigation_card_on_identity = [&] { native_preview.first = &replacement.header; };
  Require(!retain_ship(), "preview-data replacement after acquisition cannot retain the previous ship");
  native_preview.first        = &recorded.header;
  navigation_card_on_identity = [&] {
    native_context.second = reinterpret_cast<Il2CppObject*>(il2cpp::NewString("other"));
  };
  Require(!retain_ship(), "owner replacement during a managed observation cannot retain another player's card");
  native_context.second       = reinterpret_cast<Il2CppObject*>(recorded.user);
  navigation_card_on_identity = [&] { recorded.plan = &replacement_plan.header; };
  Require(!retain_ship(), "an Attack plan replaced during a managed observation cannot retain its previous card");
  recorded.plan                  = &native_plan.header;
  navigation_card_identity_stage = "target-user";
  navigation_card_on_identity    = [&] { native_plan.target.value = recorded.target + 1; };
  Require(!retain_ship(), "a deployed-fleet target changed during observation cannot retain the old ship identity");
  native_plan.target.value    = recorded.target;
  navigation_card_on_identity = [&] { native_plan.target.has_value = false; };
  Require(!retain_ship(),
          "a nullable deployed target cleared after observation cannot retain a previously matched card");
  native_plan.target.has_value   = true;
  navigation_card_identity_stage = "fleet-id";
  navigation_card_on_identity    = [&] { native_preview.second = &replacement_context.header; };
  Require(!retain_ship(), "preview-context replacement after acquisition cannot retain the old ship");
  native_preview.second       = &native_context.header;
  navigation_card_on_identity = [&] { native_nav.first = &replacement_context.header; };
  Require(!retain_navigation_ship(), "navigation-context replacement after acquisition remains native");
  native_nav.first            = &native_context.header;
  navigation_card_on_identity = [&] { native_scanning.first = &replacement.header; };
  Require(!retain_navigation_ship(), "scanning cache replacement while matching cannot retain stale ship data");
  native_scanning.first = &recorded.header;
  NavigationUiNode replacement_scanning{{&scanning.klass}, &recorded.header};
  navigation_card_on_identity = [&] { navigation_card_manager = &replacement_scanning; };
  Require(!retain_navigation_ship(), "a replaced scanning manager cannot authorize old-card retention");
  navigation_card_manager = &native_scanning;
  navigation_card_identity_stage.clear();
  warp_root_failure = true;
  Require(!retain_ship() && !retain_navigation_ship(), "failed managed custody preserves native card dismissal");
  warp_root_failure = false;

  base_card_ready.store(true);
  auto&      options              = Config::Get().ui;
  const auto saved_base           = options.keep_base_card_open;
  const auto saved_ship           = options.keep_player_ship_card_open;
  int        native_preview_calls = 0, native_navigation_calls = 0;
  const auto original_preview = [&](auto* actual, SetCourseResponse received) {
    Require(actual == widget && received.target_id == event.target_id && received.fleet_id == event.fleet_id,
            "fallback forwards the unmodified native preview callback");
    ++native_preview_calls;
  };
  const auto original_navigation = [&](auto* actual, SetCourseResponse received) {
    Require(actual == navigation && received.target_id == event.target_id && received.fleet_id == event.fleet_id,
            "fallback forwards the unmodified native navigation callback");
    ++native_navigation_calls;
  };
  for (bool keep_base : {false, true})
    for (bool keep_ship : {false, true}) {
      options.keep_base_card_open        = keep_base;
      options.keep_player_ship_card_open = keep_ship;
      const auto before_preview = native_preview_calls, before_navigation = native_navigation_calls;
      PreScanTargetWidget_OnSetCourseResponse_BaseCardHook(original_preview, widget, event);
      NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook(original_navigation, navigation, event);
      Require(native_preview_calls == before_preview + !keep_ship
                  && native_navigation_calls == before_navigation + !keep_ship,
              "real response hooks honor ship retention independently of the station preference");
    }
  options.keep_base_card_open        = false;
  options.keep_player_ship_card_open = true;
  event.success                      = false;
  const auto before_preview_failure = native_preview_calls, before_navigation_failure = native_navigation_calls;
  PreScanTargetWidget_OnSetCourseResponse_BaseCardHook(original_preview, widget, event);
  NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook(original_navigation, navigation, event);
  Require(native_preview_calls == before_preview_failure + 1
              && native_navigation_calls == before_navigation_failure + 1,
          "failed responses reach each original native hook exactly once");
  event.success = true;
  recorded.base = true;
  for (bool keep_base : {false, true})
    for (bool keep_ship : {false, true}) {
      options.keep_base_card_open        = keep_base;
      options.keep_player_ship_card_open = keep_ship;
      const auto before                  = native_preview_calls;
      PreScanTargetWidget_OnSetCourseResponse_BaseCardHook(original_preview, widget, event);
      Require(native_preview_calls == before + !keep_base,
              "the independent ship setting cannot change existing station-card retention");
    }
  options.keep_base_card_open        = saved_base;
  options.keep_player_ship_card_open = saved_ship;
  base_card_ready.store(false);
  tow_runtime_invoke_override = saved_override;
  extra_value_box = second_value_box = nullptr;
  extra_value_payload = second_value_payload = nullptr;
  Require(warp_roots.empty(), "station-card evidence releases every operation-local root");
  std::cout
      << "Production station/player card exact IDs, independent toggles, callbacks and replacement cases passed\n";
}
} // namespace
