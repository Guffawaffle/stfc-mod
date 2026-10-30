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
};
struct NavigationCardLongBox {
  Il2CppObject header{};
  int64_t      value = 0;
} navigation_card_box;
NavigationUiNode*     navigation_card_manager = nullptr;
std::function<void()> navigation_card_on_context;
bool                  navigation_card_unknown_player = false;
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
{ return value->user; }
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
    navigation_card_box.value = name == "get_PlayerID" ? static_cast<NavigationCardData*>(target)->player
                                                       : static_cast<NavigationCardData*>(target)->target;
    return &navigation_card_box.header;
  }
  if (name == "get_TargetUserID")
    return reinterpret_cast<Il2CppObject*>(static_cast<NavigationCardData*>(target)->user);
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
  auto& response = Define("Digit.PrimeServer.Events", "SetCourseResponseEventArgs", IL2CPP_TYPE_VALUETYPE, nullptr,
                          sizeof(SetCourseResponse));
  AddField(response, "<FleetId>k__BackingField", long_class, 0x10);
  AddField(response, "<Success>k__BackingField", bool_class, 0x18);
  AddField(response, "<IsRecall>k__BackingField", bool_class, 0x19);
  AddField(response, "<TargetId>k__BackingField", object_class, 0x20);
  AddMethod(preview, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddMethod(nav, "OnSetCourseResponse", reinterpret_cast<void*>(&NavigationCardNoop), T(void_class), {T(response)});
  AddField(preview, "_battleTargetData", data, offsetof(NavigationUiNode, first));
  AddField(context, "Poi", poi, offsetof(NavigationUiNode, first));
  AddField(context, "UserId", string_class, offsetof(NavigationUiNode, second));
  preview.klass.instance_size = context.klass.instance_size = sizeof(NavigationUiNode);
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
  auto& player = AddMethod(data, "get_PlayerID", reinterpret_cast<void*>(&NavigationCardPlayer), T(long_class), {});
  auto& target = AddMethod(data, "get_TargetID", reinterpret_cast<void*>(&NavigationCardTarget), T(long_class), {});
  auto& user   = AddMethod(data, "get_TargetUserID", reinterpret_cast<void*>(&NavigationCardUser), T(string_class), {});
  player.invoker_method = target.invoker_method = user.invoker_method =
      reinterpret_cast<InvokerMethod>(&NavigationCardNoop);
  NavigationCardData recorded{{&data.klass}}, replacement = recorded;
  recorded.user    = il2cpp::NewString("enemy");
  replacement.user = recorded.user;
  Il2CppObject     native_poi{&poi.klass};
  NavigationUiNode native_context{{&context.klass}, &native_poi, reinterpret_cast<Il2CppObject*>(recorded.user)};
  NavigationUiNode native_preview{{&preview.klass}, &recorded.header, &native_context.header};
  NavigationUiNode native_nav{{&nav.klass}, &native_context.header};
  NavigationUiNode native_scanning{{&scanning.klass}, &recorded.header};
  navigation_card_manager          = &native_scanning;
  navigation_card_box.header.klass = &long_class.klass;
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
  tow_runtime_invoke_override = saved_override;
  extra_value_box = second_value_box = nullptr;
  extra_value_payload = second_value_payload = nullptr;
  Require(warp_roots.empty(), "station-card evidence releases every operation-local root");
  std::cout << "Production station-card evidence matching, exact IDs, recall and replacement cases passed\n";
}
} // namespace
