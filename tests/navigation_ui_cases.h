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
  Il2CppObject* fleet = nullptr;
};
struct NavigationCardFleet {
  Il2CppObject      header{};
  DeployedFleetType type  = DeployedFleetType::Player;
  bool              local = false, destroyed = false;
};
struct NavigationCardViewer {
  Il2CppObject  header{};
  Il2CppObject *data = nullptr, *context = nullptr, *visibility = nullptr;
  int32_t       occupied = 2; // Mining OtherPlayerOccupied; unused by regular player cards.
};
struct NavigationCardNavigation {
  Il2CppObject  header{};
  Il2CppObject *context = nullptr, *loader = nullptr;
};
struct NavigationCardLoader {
  Il2CppObject  header{};
  Il2CppObject *viewer = nullptr, *poi = nullptr;
};
struct NavigationCardVisibility {
  Il2CppObject    header{};
  VisibilityState state = VisibilityState::Visible;
};
struct NavigationCardLongBox {
  Il2CppObject header{};
  int64_t      value = 0;
} navigation_card_box;
NavigationUiNode*     navigation_card_manager = nullptr;
std::function<void()> navigation_card_on_context, navigation_card_on_classification;
std::string           navigation_card_failure;
bool                  navigation_card_unknown_player = false;
void                  NavigationCardClassification(std::string_view stage)
{
  if (navigation_card_failure == stage)
    throw Il2CppExceptionWrapper(&thrown);
  if (navigation_card_on_classification)
    std::exchange(navigation_card_on_classification, {})();
}
bool NavigationCardBase(NavigationCardData* value, const MethodInfo*)
{
  const auto result = value->base;
  NavigationCardClassification("starbase");
  return result;
}
bool NavigationCardOwn(NavigationCardData* value, const MethodInfo*)
{ return value->own; }
bool NavigationCardValid(NavigationCardData* value, const MethodInfo*)
{
  const auto result = value->valid;
  NavigationCardClassification("valid");
  return result;
}
int64_t NavigationCardPlayer(NavigationCardData* value, const MethodInfo*)
{
  if (navigation_card_unknown_player)
    throw std::runtime_error("controlled player getter unavailable");
  return value->player;
}
int64_t NavigationCardTarget(NavigationCardData* value, const MethodInfo*)
{ return value->target; }
Il2CppString* NavigationCardUser(NavigationCardData* value, const MethodInfo*)
{ return value->user; }
DeployedFleetType NavigationCardFleetType(NavigationCardFleet* value, const MethodInfo*)
{
  const auto result = value->type;
  NavigationCardClassification("fleet-type");
  return result;
}
bool NavigationCardFleetLocal(NavigationCardFleet* value, const MethodInfo*)
{
  const auto result = value->local;
  NavigationCardClassification("fleet-local");
  return result;
}
bool NavigationCardFleetDestroyed(NavigationCardFleet* value, const MethodInfo*)
{
  const auto result = value->destroyed;
  NavigationCardClassification("fleet-destroyed");
  return result;
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
    return static_cast<NavigationCardViewer*>(target)->context;
  }
  if (name == "get_CanvasContext")
    return static_cast<NavigationCardNavigation*>(target)->context;
  if (name == "get_TargetDataCache")
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
  if (name == "get_TargetUserID")
    return reinterpret_cast<Il2CppObject*>(static_cast<NavigationCardData*>(target)->user);
  handled = false;
  return nullptr;
}
void NavigationCardCases()
{
  auto& data        = Define("Digit.PrimeServer.Models", "BattleTargetData", IL2CPP_TYPE_CLASS, &object_class);
  auto& context     = Fake(NavigationInteractionUIContext::Class::Raw());
  auto& poi         = Define("Digit.Prime.Navigation", "POI", IL2CPP_TYPE_CLASS, &object_class);
  auto& viewer_base = Define("Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& preview     = Define("Digit.Prime.Combat", "PreScanTargetWidget", IL2CPP_TYPE_CLASS, &viewer_base);
  auto& mining      = Define("Digit.Prime.ObjectViewer", "MiningObjectViewerWidget", IL2CPP_TYPE_CLASS, &viewer_base);
  auto& nav =
      Define("Digit.Prime.Navigation", "NavigationInteractionUIViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& loader = Define("Digit.Prime.ObjectViewer", "ParentObjectViewerLoadAndShow", IL2CPP_TYPE_CLASS, &object_class);
  auto& visibility = Define("Digit.Client.UI", "VisibilityController", IL2CPP_TYPE_CLASS, &object_class);
  auto& state   = Define("Digit.Client.UI", "VisibilityState", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(VisibilityState));
  state.is_enum = true;
  AddField(state, "value__", int_class, sizeof(Il2CppObject));
  auto& scanning     = Define("Digit.Prime.Navigation.Managers", "ScanningManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& deployed     = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& fleet_type   = Define("Digit.PrimeServer.Models", "DeployedFleetType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  fleet_type.is_enum = true;
  auto& response     = Define("Digit.PrimeServer.Events", "SetCourseResponseEventArgs", IL2CPP_TYPE_VALUETYPE, nullptr,
                              sizeof(SetCourseResponse));
  AddField(response, "<FleetId>k__BackingField", long_class, 0x10);
  AddField(response, "<Success>k__BackingField", bool_class, 0x18);
  AddField(response, "<IsRecall>k__BackingField", bool_class, 0x19);
  AddField(response, "<TargetId>k__BackingField", object_class, 0x20);
  AddMethod(preview, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddMethod(nav, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddField(preview, "_battleTargetData", data, offsetof(NavigationCardViewer, data));
  AddField(mining, "_targetFleet", deployed, offsetof(NavigationCardViewer, data));
  AddField(mining, "_occupiedState", int_class, offsetof(NavigationCardViewer, occupied));
  AddField(context, "Poi", poi, offsetof(NavigationUiNode, first));
  AddField(context, "UserId", string_class, offsetof(NavigationUiNode, second));
  AddField(data, "TargetFleetDeployedData", deployed, offsetof(NavigationCardData, fleet));
  AddField(nav, "_objectViewerLoadAndShow", loader, offsetof(NavigationCardNavigation, loader));
  AddField(loader, "_activeViewer", viewer_base, offsetof(NavigationCardLoader, viewer));
  AddField(loader, "_activePOI", poi, offsetof(NavigationCardLoader, poi));
  AddField(viewer_base, "_visibilityController", visibility, offsetof(NavigationCardViewer, visibility));
  AddField(visibility, "_state", state, offsetof(NavigationCardVisibility, state));
  preview.klass.instance_size = viewer_base.klass.instance_size = sizeof(NavigationCardViewer);
  mining.klass.instance_size                                    = sizeof(NavigationCardViewer);
  nav.klass.instance_size                                       = sizeof(NavigationCardNavigation);
  context.klass.instance_size                                   = sizeof(NavigationUiNode);
  data.klass.instance_size                                      = sizeof(NavigationCardData);
  deployed.klass.instance_size                                  = sizeof(NavigationCardFleet);
  loader.klass.instance_size                                    = sizeof(NavigationCardLoader);
  visibility.klass.instance_size                                = sizeof(NavigationCardVisibility);
  const auto reference_get = [&](FakeClass& owner, const char* name, FakeClass& result,
                                 bool is_static = false) -> MethodInfo& {
    auto& method          = AddMethod(owner, name, reinterpret_cast<void*>(&NavigationCardNoop), T(result), {},
                                      METHOD_ATTRIBUTE_PUBLIC | (is_static ? METHOD_ATTRIBUTE_STATIC : 0));
    method.invoker_method = reinterpret_cast<InvokerMethod>(&NavigationCardNoop);
    return method;
  };
  auto& preview_context = reference_get(viewer_base, "get_Context", context);
  AddProperty(viewer_base, "Context", &preview_context);
  auto& nav_context = reference_get(nav, "get_CanvasContext", context);
  AddProperty(nav, "CanvasContext", &nav_context);
  reference_get(scanning, "get_Instance", scanning, true);
  reference_get(scanning, "get_TargetDataCache", data);
  AddMethod(data, "get_IsStarbase", reinterpret_cast<void*>(&NavigationCardBase), T(bool_class), {});
  AddMethod(data, "get_IsOwnStation", reinterpret_cast<void*>(&NavigationCardOwn), T(bool_class), {});
  AddMethod(data, "get_IsValid", reinterpret_cast<void*>(&NavigationCardValid), T(bool_class), {});
  AddMethod(deployed, "get_FleetType", reinterpret_cast<void*>(&NavigationCardFleetType), T(fleet_type), {});
  AddMethod(deployed, "get_IsLocalPlayer", reinterpret_cast<void*>(&NavigationCardFleetLocal), T(bool_class), {});
  AddMethod(deployed, "get_IsDestroyed", reinterpret_cast<void*>(&NavigationCardFleetDestroyed), T(bool_class), {});
  auto& player = AddMethod(data, "get_PlayerID", reinterpret_cast<void*>(&NavigationCardPlayer), T(long_class), {});
  auto& target = AddMethod(data, "get_TargetID", reinterpret_cast<void*>(&NavigationCardTarget), T(long_class), {});
  auto& user   = AddMethod(data, "get_TargetUserID", reinterpret_cast<void*>(&NavigationCardUser), T(string_class), {});
  player.invoker_method = target.invoker_method = user.invoker_method =
      reinterpret_cast<InvokerMethod>(&NavigationCardNoop);
  NavigationCardData recorded{{&data.klass}}, replacement = recorded;
  recorded.user    = il2cpp::NewString("enemy");
  replacement.user = recorded.user;
  Il2CppObject     native_poi{&poi.klass}, other_poi{&poi.klass};
  NavigationUiNode native_context{{&context.klass}, &native_poi, reinterpret_cast<Il2CppObject*>(recorded.user)};
  NavigationCardVisibility native_visibility{{&visibility.klass}}, other_visibility = native_visibility;
  NavigationCardViewer     native_preview{
      {&preview.klass}, &recorded.header, &native_context.header, &native_visibility.header};
  NavigationCardViewer other_preview = native_preview;
  NavigationCardLoader native_loader{{&loader.klass}, &native_preview.header, &native_poi},
      other_loader = native_loader;
  NavigationCardNavigation native_nav{{&nav.klass}, &native_context.header, &native_loader.header};
  NavigationUiNode         native_scanning{{&scanning.klass}, &recorded.header};
  navigation_card_manager          = &native_scanning;
  navigation_card_box.header.klass = &long_class.klass;
  auto saved_override              = tow_runtime_invoke_override;
  tow_runtime_invoke_override      = &NavigationCardInvoke;
  NavigationCardLongBox target_box{{&long_class.klass}, 77};
  extra_value_box      = &target_box.header;
  extra_value_payload  = &target_box.value;
  second_value_box     = &navigation_card_box.header;
  second_value_payload = &navigation_card_box.value;
  SetCourseResponse event{recorded.player, true, false, reinterpret_cast<il2cpp::Object*>(&target_box)};
  auto*             widget     = reinterpret_cast<PreScanTargetWidget*>(&native_preview);
  auto*             navigation = reinterpret_cast<NavigationInteractionUIViewController*>(&native_nav);
  Require(base_card_binding::Native().ready, "station-card metadata and response ABI qualify");
  Require(base_card_binding::RetainPreview(widget, event), "matching exact fleet/target/enemy/context retains preview");
  Require(base_card_binding::RetainNavigation(navigation, event),
          "navigation retains matching station scanning evidence");
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
  navigation_card_on_context     = [&] { native_preview.data = &replacement.header; };
  Require(!base_card_binding::RetainPreview(widget, event),
          "preview replacement during context acquisition prevents stale retention");

  native_preview.data = &recorded.header;
  event.is_recall     = false;
  event.target_id     = reinterpret_cast<il2cpp::Object*>(&target_box);
  recorded.base       = false;
  NavigationCardFleet native_fleet{{&deployed.klass}}, other_fleet = native_fleet;
  recorded.fleet                 = &native_fleet.header;
  replacement.base               = false;
  replacement.valid              = false;
  NavigationUiNode other_context = native_context;
  const auto       retain        = [&](bool nav_path) {
    return nav_path ? base_card_binding::RetainNavigation(navigation, event, false, true)
                    : base_card_binding::RetainPreview(widget, event, false, true);
  };
  const auto expect = [&](bool expected, const char* message) {
    Require(retain(false) == expected && retain(true) == expected, message);
  };
  Require(base_card_binding::ShipNative().ready,
          "displayed player-card metadata qualifies separately from station evidence");
  expect(true, "the currently displayed other-player ship retains both response paths");
  Require(!base_card_binding::RetainPreview(widget, event)
              && !base_card_binding::RetainPreview(widget, event, false, false),
          "player-card retention remains opt-in");
  for (auto* payload :
       {static_cast<Il2CppObject*>(nullptr), reinterpret_cast<Il2CppObject*>(recorded.user), &target_box.header}) {
    event.target_id  = reinterpret_cast<il2cpp::Object*>(payload);
    target_box.value = 991;
    event.fleet_id   = 3;
    expect(true, "response target representation and actor identity do not gate the displayed player card");
  }
  navigation_card_manager = nullptr;
  expect(true, "an absent global scanning manager cannot invalidate a displayed player card");
  navigation_card_manager = &native_scanning;
  native_scanning.first   = &replacement.header;
  expect(true, "stale invalid global scanning data is ignored for the displayed player card");
  native_scanning.first = &recorded.header;
  for (const auto type : {DeployedFleetType::Marauder, DeployedFleetType::Alliance, DeployedFleetType::Nonexistent}) {
    native_fleet.type = type;
    expect(false, "NPC and non-player cards retain native dismissal");
  }
  native_fleet.type  = DeployedFleetType::Player;
  native_fleet.local = true;
  expect(false, "own ship cards retain native dismissal");
  native_fleet.local     = false;
  native_fleet.destroyed = true;
  expect(false, "destroyed ship data cannot authorize retention");
  native_fleet.destroyed = false;
  recorded.base          = true;
  expect(false, "the ship option cannot retain station cards");
  recorded.base  = false;
  recorded.valid = false;
  expect(false, "invalid displayed battle data retains native dismissal");
  recorded.valid = true;
  for (const auto hidden : {VisibilityState::Unknown, VisibilityState::Hide, VisibilityState::Hidden}) {
    native_visibility.state = hidden;
    expect(false, "inactive or hidden player cards retain native dismissal");
  }
  native_visibility.state = VisibilityState::Show;
  expect(true, "a player card becoming visible may retain its attack response");
  native_visibility.state = VisibilityState::Visible;
  for (auto* dead : {&native_preview.header, &native_poi}) {
    visual_dead_object = dead;
    expect(false, "dead widget or POI observations retain native dismissal");
  }
  visual_dead_object = nullptr;
  visual_throws      = true;
  expect(false, "unknown Unity liveness cannot authorize player-card retention");
  visual_throws = false;
  for (const auto* field : {"starbase", "valid", "fleet-type", "fleet-local", "fleet-destroyed"}) {
    navigation_card_failure = field;
    expect(false, "unknown displayed target classification retains native dismissal");
  }
  navigation_card_failure.clear();
  recorded.fleet = nullptr;
  expect(false, "absent displayed fleet data cannot qualify a player card");
  recorded.fleet      = &native_fleet.header;
  native_preview.data = nullptr;
  expect(false, "absent displayed battle data cannot qualify a player card");
  native_preview.data = &replacement.header;
  expect(false, "valid global scanning data cannot rescue an invalid displayed card");
  native_preview.data    = &recorded.header;
  native_preview.context = nullptr;
  expect(false, "absent displayed context cannot qualify a player card");
  native_preview.context    = &native_context.header;
  native_preview.visibility = nullptr;
  expect(false, "an absent visibility controller cannot prove the player card is open");
  native_preview.visibility = &native_visibility.header;
  native_context.first      = nullptr;
  expect(false, "absent displayed POI cannot qualify a player card");
  native_context.first = &native_poi;
  native_loader.viewer = nullptr;
  Require(!retain(true), "an absent active viewer cannot use global scanning data as a substitute");
  Il2CppObject unrelated_viewer{&viewer_base.klass};
  native_loader.viewer = &unrelated_viewer;
  Require(!retain(true), "another kind of active viewer cannot inherit player-card retention");
  native_loader.viewer = &native_preview.header;
  native_nav.loader    = nullptr;
  Require(!retain(true), "an absent loader cannot use the global cache as a displayed viewer");
  native_nav.loader = &native_loader.header;
  native_loader.poi = &other_poi;
  Require(!retain(true), "the loader and viewer must still display the same POI reference");
  native_loader.poi = &native_poi;
  event.success     = false;
  expect(false, "failed responses preserve native dismissal");
  event.success   = true;
  event.is_recall = true;
  expect(false, "the player option does not retain recall responses");
  event.is_recall = false;

  const auto reset_display = [&] {
    native_preview.data       = &recorded.header;
    native_preview.context    = &native_context.header;
    native_preview.visibility = &native_visibility.header;
    native_context.first      = &native_poi;
    recorded.fleet            = &native_fleet.header;
    native_nav.loader         = &native_loader.header;
    native_loader.viewer      = &native_preview.header;
    native_loader.poi         = &native_poi;
  };
  for (bool nav_path : {false, true}) {
    for (const auto replace : {0, 1, 2, 3, 4}) {
      navigation_card_on_classification = [&, replace] {
        if (replace == 0)
          native_preview.data = &replacement.header;
        if (replace == 1)
          native_preview.context = &other_context.header;
        if (replace == 2)
          native_context.first = &other_poi;
        if (replace == 3)
          recorded.fleet = &other_fleet.header;
        if (replace == 4)
          native_preview.visibility = &other_visibility.header;
      };
      Require(!retain(nav_path),
              "viewer/context/POI/data/fleet replacement during classification preserves native dismissal");
      reset_display();
    }
  }
  for (const auto replace : {0, 1, 2}) {
    navigation_card_on_classification = [&, replace] {
      if (replace == 0)
        native_nav.loader = &other_loader.header;
      if (replace == 1)
        native_loader.viewer = &other_preview.header;
      if (replace == 2)
        native_loader.poi = &other_poi;
    };
    Require(!retain(true), "navigation loader/viewer/POI replacement cannot retain a stale displayed card");
    reset_display();
  }
  warp_root_failure = true;
  expect(false, "failed managed custody retains native dismissal");
  warp_root_failure = false;

  base_card_ready.store(true);
  auto&      options    = Config::Get().ui;
  const auto saved_base = options.keep_base_card_open, saved_ship = options.keep_player_ship_card_open;
  int        preview_calls = 0, navigation_calls = 0;
  const auto original_preview = [&](auto* actual, SetCourseResponse received) {
    Require(actual == widget && received.target_id == event.target_id && received.fleet_id == event.fleet_id,
            "fallback forwards the original preview arguments");
    ++preview_calls;
  };
  const auto original_navigation = [&](auto* actual, SetCourseResponse received) {
    Require(actual == navigation && received.target_id == event.target_id && received.fleet_id == event.fleet_id,
            "fallback forwards the original navigation arguments");
    ++navigation_calls;
  };
  const auto invoke_hooks = [&](bool retained) {
    const auto before_preview = preview_calls, before_navigation = navigation_calls;
    PreScanTargetWidget_OnSetCourseResponse_BaseCardHook(original_preview, widget, event);
    NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook(original_navigation, navigation, event);
    Require(preview_calls == before_preview + !retained && navigation_calls == before_navigation + !retained,
            "each real hook suppresses only retention and otherwise calls its native original exactly once");
  };
  for (bool keep_base : {false, true})
    for (bool keep_ship : {false, true}) {
      options.keep_base_card_open        = keep_base;
      options.keep_player_ship_card_open = keep_ship;
      invoke_hooks(keep_ship);
    }
  options.keep_base_card_open        = false;
  options.keep_player_ship_card_open = true;
  event.success                      = false;
  invoke_hooks(false);
  event.success   = true;
  event.is_recall = true;
  invoke_hooks(false);
  event.is_recall    = false;
  native_fleet.local = true;
  invoke_hooks(false);
  native_fleet.local      = false;
  native_visibility.state = VisibilityState::Hidden;
  invoke_hooks(false);
  native_visibility.state = VisibilityState::Visible;
  navigation_card_failure = "valid";
  invoke_hooks(false);
  navigation_card_failure.clear();
  event.target_id  = reinterpret_cast<il2cpp::Object*>(&target_box);
  event.fleet_id   = recorded.player;
  target_box.value = recorded.target;
  recorded.base    = true;
  for (bool keep_base : {false, true})
    for (bool keep_ship : {false, true}) {
      options.keep_base_card_open        = keep_base;
      options.keep_player_ship_card_open = keep_ship;
      invoke_hooks(keep_base);
    }

  NavigationCardViewer mining_viewer{
      {&mining.klass}, &native_fleet.header, &native_context.header, &native_visibility.header};
  native_loader.viewer     = &mining_viewer.header;
  recorded.base            = false;
  event.target_id          = nullptr;
  event.fleet_id           = 0;
  const auto retain_mining = [&] { return base_card_binding::RetainNavigation(navigation, event, false, true); };
  Require(base_card_binding::ShipNative().mining_ready && retain_mining(),
          "a displayed mining card retains its actual other-player occupant without battle-target data");
  navigation_card_manager = nullptr;
  Require(retain_mining(), "mining-card retention does not need a global scanning manager or response target");
  navigation_card_manager = &native_scanning;
  native_scanning.first   = &replacement.header;
  for (const auto* unused : {"starbase", "valid"}) {
    navigation_card_failure = unused;
    Require(retain_mining(), "mining-card classification does not read unrelated battle-target predicates");
  }
  navigation_card_failure.clear();
  for (const auto occupied : {int32_t{0}, int32_t{1}, int32_t{-1}, int32_t{99}}) {
    mining_viewer.occupied = occupied;
    Require(!retain_mining(), "empty, own or unknown node states cannot reuse a stale bound opponent fleet");
  }
  mining_viewer.occupied = 2;
  mining_viewer.data     = nullptr;
  Require(!retain_mining(), "an empty mining node keeps native card dismissal");
  mining_viewer.data = &recorded.header;
  Require(!retain_mining(), "an incorrectly typed bound occupant cannot qualify a mining card");
  mining_viewer.data = &native_fleet.header;
  native_fleet.type  = DeployedFleetType::Marauder;
  Require(!retain_mining(), "NPC occupants keep native mining-card dismissal");
  native_fleet.type  = DeployedFleetType::Player;
  native_fleet.local = true;
  Require(!retain_mining(), "own occupants keep native mining-card dismissal");
  native_fleet.local     = false;
  native_fleet.destroyed = true;
  Require(!retain_mining(), "destroyed occupants cannot keep mining cards open");
  native_fleet.destroyed  = false;
  native_visibility.state = VisibilityState::Hidden;
  Require(!retain_mining(), "hidden mining cards cannot qualify for retention");
  native_visibility.state = VisibilityState::Visible;
  for (const auto* field : {"fleet-type", "fleet-local", "fleet-destroyed"}) {
    navigation_card_failure = field;
    Require(!retain_mining(), "unknown occupant classification keeps native mining-card dismissal");
  }
  navigation_card_failure.clear();
  for (auto* dead : {&mining_viewer.header, &native_poi}) {
    visual_dead_object = dead;
    Require(!retain_mining(), "dead mining viewer or node cannot qualify for retention");
  }
  visual_dead_object = nullptr;
  visual_throws      = true;
  Require(!retain_mining(), "unknown mining-viewer liveness keeps native dismissal");
  visual_throws     = false;
  warp_root_failure = true;
  Require(!retain_mining(), "failed managed custody keeps native mining-card dismissal");
  warp_root_failure       = false;
  const auto reset_mining = [&] {
    mining_viewer.data       = &native_fleet.header;
    mining_viewer.context    = &native_context.header;
    mining_viewer.visibility = &native_visibility.header;
    mining_viewer.occupied   = 2;
    native_context.first     = &native_poi;
    native_nav.loader        = &native_loader.header;
    native_loader.viewer     = &mining_viewer.header;
    native_loader.poi        = &native_poi;
  };
  for (const auto replace : {0, 1, 2, 3, 4, 5, 6, 7}) {
    navigation_card_on_classification = [&, replace] {
      if (replace == 0)
        mining_viewer.data = &other_fleet.header;
      if (replace == 1)
        mining_viewer.context = &other_context.header;
      if (replace == 2)
        native_context.first = &other_poi;
      if (replace == 3)
        mining_viewer.visibility = &other_visibility.header;
      if (replace == 4)
        native_nav.loader = &other_loader.header;
      if (replace == 5)
        native_loader.viewer = &other_preview.header;
      if (replace == 6)
        native_loader.poi = &other_poi;
      if (replace == 7)
        mining_viewer.occupied = 0;
    };
    Require(!retain_mining(), "mining occupant/state/viewer/context/loader replacement prevents stale retention");
    reset_mining();
  }
  const auto invoke_mining_hook = [&](bool retained) {
    const auto before = navigation_calls;
    NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook(original_navigation, navigation, event);
    Require(navigation_calls == before + !retained,
            "the mining response hook either retains or calls its native original exactly once");
  };
  for (bool keep_base : {false, true})
    for (bool keep_ship : {false, true}) {
      options.keep_base_card_open        = keep_base;
      options.keep_player_ship_card_open = keep_ship;
      invoke_mining_hook(keep_ship);
    }
  options.keep_player_ship_card_open = true;
  event.success                      = false;
  invoke_mining_hook(false);
  event.success   = true;
  event.is_recall = true;
  invoke_mining_hook(false);
  event.is_recall    = false;
  native_fleet.local = true;
  invoke_mining_hook(false);
  native_fleet.local = false;
  native_fleet.type  = DeployedFleetType::Marauder;
  invoke_mining_hook(false);
  native_fleet.type      = DeployedFleetType::Player;
  native_fleet.destroyed = true;
  invoke_mining_hook(false);
  native_fleet.destroyed = false;
  mining_viewer.occupied = 0;
  invoke_mining_hook(false);
  mining_viewer.occupied = 1;
  invoke_mining_hook(false);
  mining_viewer.occupied = 2;
  mining_viewer.data     = nullptr;
  invoke_mining_hook(false);
  mining_viewer.data = &recorded.header;
  invoke_mining_hook(false);
  mining_viewer.data      = &native_fleet.header;
  navigation_card_failure = "fleet-type";
  invoke_mining_hook(false);
  navigation_card_failure.clear();
  native_visibility.state = VisibilityState::Hidden;
  invoke_mining_hook(false);
  native_visibility.state = VisibilityState::Visible;
  base_card_ready.store(false);
  invoke_mining_hook(false);
  reset_display();
  options.keep_base_card_open        = saved_base;
  options.keep_player_ship_card_open = saved_ship;
  base_card_ready.store(false);
  tow_runtime_invoke_override = saved_override;
  extra_value_box = second_value_box = nullptr;
  extra_value_payload = second_value_payload = nullptr;
  Require(warp_roots.empty(), "card observations release every operation-local root");
  std::cout << "Production station matching and displayed player/mining-card classification/replacement cases passed\n";
}
} // namespace
