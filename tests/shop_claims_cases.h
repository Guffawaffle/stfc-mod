#pragma once

// Actual consumers against the existing controlled IL2CPP host. No purchases,
// rewards, native detours or Unity rendering are performed by these cases.
#include "../src/mod/features/confirmations.cc"
#include "../src/mod/features/missions.cc"
#include "../src/mod/features/shop.cc"

namespace
{
struct BatchObject {
  Il2CppObject  header{};
  Il2CppObject *first = nullptr, *second = nullptr;
  bool          value = false;
  int32_t       flags = 0;
};
struct BatchHud {
  Il2CppObject header{};
  BatchObject* buttons[4]{};
};
std::function<void()>             batch_context_callback, batch_click_callback, batch_toggle_read_callback;
int                               batch_opens = 0, batch_aux = 0, batch_toggle_clicks = 0, batch_answers = 0;
int32_t                           batch_answer     = 0;
bool                              batch_throw_read = false, batch_throw_answer = false;
std::unordered_set<Il2CppObject*> batch_dead;
std::unordered_map<Il2CppObject*, std::vector<Il2CppObject*>> batch_lists;

bool BatchAlive(Il2CppObject* value, const MethodInfo*)
{ return value && !batch_dead.contains(value); }
Il2CppObject* BatchContext(BatchObject* value, const MethodInfo*)
{
  if (batch_context_callback)
    batch_context_callback();
  return value->first;
}
void BatchOpen(BatchObject* value, const MethodInfo*)
{
  Require(std::ranges::any_of(warp_roots, [value](const auto& root) { return root.second == &value->header; }),
          "drawer has operation-local strong custody during native invocation");
  ++batch_opens;
  if (batch_click_callback)
    batch_click_callback();
}
void BatchAux(Il2CppObject*, const MethodInfo*)
{ ++batch_aux; }
bool BatchIsOn(BatchObject* value, const MethodInfo*)
{
  if (batch_toggle_read_callback)
    batch_toggle_read_callback();
  if (batch_throw_read)
    throw Il2CppExceptionWrapper(&thrown);
  return value->value;
}
void BatchPointerCtor(Il2CppObject*, Il2CppObject*, const MethodInfo*) {}
void BatchToggle(BatchObject* value, Il2CppObject*, const MethodInfo*)
{
  ++batch_toggle_clicks;
  value->value = true;
  if (batch_click_callback)
    batch_click_callback();
}
Il2CppObject* BatchNoTransform(Il2CppObject*, const MethodInfo*)
{ return nullptr; }
Il2CppObject* BatchGameObject(BatchObject* value, const MethodInfo*)
{ return value->first; }
bool BatchActive(BatchObject* value, const MethodInfo*)
{ return value->value; }
void BatchSetActive(BatchObject* value, bool visible, const MethodInfo*)
{ value->value = visible; }
bool BatchHudEnabled(Il2CppObject*, const MethodInfo*)
{ return true; }
void BatchRefresh(BatchHud* hud, const MethodInfo*)
{
  for (size_t i = 0; i < 3; ++i)
    reinterpret_cast<BatchObject*>(hud->buttons[i]->first)->value = true;
}
void BatchAnswer(Il2CppObject*, MessageBoxResult result, const MethodInfo*)
{
  if (batch_throw_answer)
    throw Il2CppExceptionWrapper(&thrown);
  ++batch_answers;
  batch_answer = static_cast<int32_t>(result);
}
int32_t BatchCount(Il2CppObject* value, const MethodInfo*)
{ return static_cast<int32_t>(batch_lists.at(value).size()); }
Il2CppObject* BatchItem(Il2CppObject* value, int32_t index, const MethodInfo*)
{ return batch_lists.at(value).at(index); }
void BatchRemove(Il2CppObject* value, int32_t index, const MethodInfo*)
{
  auto& items = batch_lists.at(value);
  items.erase(items.begin() + index);
}

void ShopClaimsCases()
{
  auto& drawer_context =
      Define("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext", IL2CPP_TYPE_CLASS, &object_class);
  AddField(drawer_context, "Enabled", bool_class, offsetof(BatchObject, value));
  auto& drawer = Define("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& context_get = AddMethod(drawer, "get_Context", reinterpret_cast<void*>(&BatchContext), T(drawer_context), {});
  AddProperty(drawer, "Context", &context_get);
  AddMethod(drawer, "OnOpenButtonClicked", reinterpret_cast<void*>(&BatchOpen), T(void_class), {});
  auto& scroller = Define("Digit.Prime.Shop", "ShopListScrollerViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddField(scroller, "_selectionDrawerWidget", drawer, offsetof(BatchObject, first));
  BatchObject context{{&drawer_context.klass}}, native_drawer{{&drawer.klass}, &context.header};
  BatchObject a{{&scroller.klass}, &native_drawer.header}, b = a;
  auto*       first                            = reinterpret_cast<ShopListScrollerViewController*>(&a);
  auto*       second                           = reinterpret_cast<ShopListScrollerViewController*>(&b);
  auto        no_op                            = [](auto*) {};
  Config::Get().ui.auto_open_bulk_claim_flyout = true;
  ShopListScrollerViewController_AboutToShow_Hook(no_op, first);
  Require(batch_opens == 0, "disabled drawer waits for native readiness");
  context.value        = true;
  batch_click_callback = [&] { ShopListScrollerViewController_UpdateDrawer_Hook(no_op, first); };
  ShopListScrollerViewController_UpdateDrawer_Hook(no_op, first);
  Require(batch_opens == 1, "reentrant drawer update still opens once per landing");
  batch_click_callback = {};
  ShopListScrollerViewController_AboutToShow_Hook(
      [&](auto* old) {
        ShopListScrollerViewController_AboutToHide_Hook(no_op, old);
        ShopListScrollerViewController_AboutToShow_Hook(no_op, second);
      },
      first);
  Require(batch_opens == 2 && shown_scroller.Get() == &b.header,
          "reentrant show replacement cannot open the old landing");
  ShopListScrollerViewController_AboutToShow_Hook(
      [&](auto* old) { ShopListScrollerViewController_AboutToHide_Hook(no_op, old); }, first);
  Require(batch_opens == 2 && !shown_scroller, "show cancelled inside original leaves no active drawer");
  Config::Get().ui.auto_open_bulk_claim_flyout = false;
  ShopListScrollerViewController_AboutToShow_Hook(no_op, first);
  Config::Get().ui.auto_open_bulk_claim_flyout = true;
  batch_context_callback                       = [&] { ShopListScrollerViewController_AboutToHide_Hook(no_op, first); };
  ShopListScrollerViewController_UpdateDrawer_Hook(no_op, first);
  Require(batch_opens == 2, "context getter closing the screen prevents a stale open");
  batch_context_callback = {};
  ShopListScrollerViewController_AboutToHide_Hook(no_op, first);

  auto& bundle = Define("Digit.Prime.Shop", "BundleDataWidget", IL2CPP_TYPE_CLASS, &object_class);
  AddField(bundle, "_currentState", int_class, offsetof(BatchObject, flags));
  AddMethod(bundle, "AuxViewButtonPressedHandler", reinterpret_cast<void*>(&BatchAux), T(void_class), {});
  BatchObject offer{{&bundle.klass}};
  int         purchase       = 0;
  auto        original_offer = [&](auto*) { ++purchase; };
  for (int32_t flags : {0, 1, BundleDataWidget::kCooldownTimerOn, BundleDataWidget::kCooldownTimerOn | 4}) {
    offer.flags = flags;
    BundleDataWidget_OnActionButtonPressedCallback_Hook(original_offer, reinterpret_cast<BundleDataWidget*>(&offer));
  }
  Require(purchase == 2 && batch_aux == 2, "cooldown bit combinations route to details; ordinary offer stays native");

  auto& mission = Define("Digit.Prime.Missions", "MissionViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& toggle  = Define("UnityEngine.UI", "Toggle", IL2CPP_TYPE_CLASS, &component_class);
  AddField(mission, "_claimAllToggle", toggle, offsetof(BatchObject, first));
  AddMethod(unity_object_class, "op_Implicit", reinterpret_cast<void*>(&BatchAlive), T(bool_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(toggle, "get_transform", reinterpret_cast<void*>(&BatchNoTransform), T(object_class), {});
  AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&BatchNoTransform), T(object_class), {});
  AddMethod(toggle, "get_isOn", reinterpret_cast<void*>(&BatchIsOn), T(bool_class), {});
  auto& pointer = Define("UnityEngine.EventSystems", "PointerEventData", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(pointer, ".ctor", reinterpret_cast<void*>(&BatchPointerCtor), T(void_class), {T(event_system_class)});
  AddMethod(toggle, "OnPointerClick", reinterpret_cast<void*>(&BatchToggle), T(void_class), {T(pointer)});
  BatchObject native_toggle{{&toggle.klass}}, native_mission{{&mission.klass}, &native_toggle.header};
  auto*       mission_owner                           = reinterpret_cast<MissionViewController*>(&native_mission);
  Config::Get().ui.daily_bulk_claim_toggle_default_on = true;
  MissionViewController_AboutToShow_Hook([&](auto* old) { MissionViewController_AboutToHide_Hook(no_op, old); },
                                         mission_owner);
  DailyClaimFilterFrame();
  Require(!shown_missions_screen && batch_toggle_clicks == 0, "cancelled mission show cannot arm a deferred toggle");
  shown_missions_screen = il2cpp::GcHandle::Weak(&native_mission.header);
  ++claim_view_revision;
  batch_throw_read = true;
  DailyClaimFilterFrame();
  Require(batch_toggle_clicks == 0 && !toggle_done, "unavailable isOn never becomes a known off state");
  batch_throw_read           = false;
  batch_toggle_read_callback = [&] { MissionViewController_AboutToHide_Hook(no_op, mission_owner); };
  DailyClaimFilterFrame();
  Require(batch_toggle_clicks == 0, "toggle read closing the screen prevents a stale click");
  batch_toggle_read_callback = {};
  shown_missions_screen      = il2cpp::GcHandle::Weak(&native_mission.header);
  ++claim_view_revision;
  DailyClaimFilterFrame();
  if (batch_toggle_clicks != 1 || !toggle_done) {
    std::cerr << "toggle clicks=" << batch_toggle_clicks << " done=" << toggle_done << " attempts=" << toggle_attempts
              << '\n';
    for (const auto& error : errors)
      std::cerr << error << '\n';
  }
  Require(batch_toggle_clicks == 1 && toggle_done, "actual pointer handler applies the default once");
  native_toggle.value = false;
  for (int i = 0; i < 1000; ++i)
    DailyClaimFilterFrame();
  Require(batch_toggle_clicks == 1, "session default never overrides a later player choice");
  MissionViewController_AboutToHide_Hook(no_op, mission_owner);

  auto& list_type    = Define("Fixture", "ClaimList", IL2CPP_TYPE_CLASS, &object_class);
  auto& count_method = AddMethod(list_type, "get_Count", reinterpret_cast<void*>(&BatchCount), T(int_class), {});
  AddProperty(list_type, "Count", &count_method);
  AddMethod(list_type, "get_Item", reinterpret_cast<void*>(&BatchItem), T(object_class), {T(int_class)});
  AddMethod(list_type, "RemoveAt", reinterpret_cast<void*>(&BatchRemove), T(void_class), {T(int_class)});
  auto& goal_type = Define("Fixture", "ClaimGoal", IL2CPP_TYPE_CLASS, &object_class);
  AddField(goal_type, "_factionPointRewards", list_type, offsetof(BatchObject, first));
  auto& resource  = Define("Digit.PrimeServer.Models", "ResourceSpec", IL2CPP_TYPE_CLASS, &object_class);
  auto& item_type = Define("Digit.PrimeServer.Models", "InventoryItem", IL2CPP_TYPE_CLASS, &object_class);
  AddField(resource, "resourceId_", string_class, offsetof(BatchObject, first));
  AddField(item_type, "_resourceSpec", resource, offsetof(BatchObject, first));
  struct BatchText {
    Il2CppObject header;
    int32_t      length;
    char16_t     text[60]{};
  };
  BatchText                 labels[2]{{{&string_class.klass}, 0}, {{&string_class.klass}, 0}};
  const std::u16string_view names[] = {u"Resource_FactionPoint_Federation", u"Resource_FactionPoint_Klingon"};
  BatchObject               specs[2]{}, items[2]{}, rewards[2]{}, goals[3]{};
  for (size_t i = 0; i < 2; ++i) {
    labels[i].length = static_cast<int32_t>(names[i].size());
    std::copy(names[i].begin(), names[i].end(), labels[i].text);
    specs[i]                        = {{&resource.klass}, &labels[i].header};
    items[i]                        = {{&item_type.klass}, &specs[i].header};
    rewards[i].header.klass         = &list_type.klass;
    batch_lists[&rewards[i].header] = {&items[i].header};
    goals[i]                        = {{&goal_type.klass}, &rewards[i].header};
  }
  goals[2].header.klass = &goal_type.klass; // No faction evidence; retain conservatively, as in the source.
  BatchObject dailies{{&list_type.klass}};
  batch_lists[&dailies.header]               = {&goals[0].header, &goals[1].header, &goals[2].header};
  auto claimable                             = [&](auto*) { return reinterpret_cast<DailyGoalList*>(&dailies); };
  Config::Get().ui.daily_bulk_claim_factions = {"federation"};
  auto* filtered = MissionViewController_GetClaimableDailiesList_Hook(claimable, mission_owner);
  Require(filtered == reinterpret_cast<DailyGoalList*>(&dailies)
              && batch_lists[&dailies.header] == std::vector<Il2CppObject*>({&goals[0].header, &goals[2].header}),
          "actual daily filter retains selected and unknown factions, removing only excluded main-faction rows");
  Config::Get().ui.daily_bulk_claim_factions.clear();
  batch_lists[&dailies.header] = {&goals[0].header, &goals[1].header, &goals[2].header};
  (void)MissionViewController_GetClaimableDailiesList_Hook(claimable, mission_owner);
  Require(batch_lists[&dailies.header].size() == 3, "empty selection preserves all native claimable dailies");

  auto&       hud_type = Define("Digit.Prime.HUD", "MissionsHudViewController", IL2CPP_TYPE_CLASS, &component_class);
  const char* fields[] = {"_challengesButton", "_achievementsButton", "_outpostsButton", "_missionsButton"};
  for (size_t i = 0; i < 4; ++i)
    AddField(hud_type, fields[i], component_class,
             static_cast<int32_t>(offsetof(BatchHud, buttons) + i * sizeof(void*)));
  AddMethod(hud_type, "get_isActiveAndEnabled", reinterpret_cast<void*>(&BatchHudEnabled), T(bool_class), {});
  AddMethod(hud_type, "SetupAchievementsButton", reinterpret_cast<void*>(&BatchRefresh), T(void_class), {});
  AddMethod(hud_type, "HandleOutpostsAndChallengesHUD", reinterpret_cast<void*>(&BatchRefresh), T(void_class), {});
  AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&BatchGameObject), T(game_object_class), {});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&BatchActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&BatchSetActive), T(void_class), {T(bool_class)});
  BatchObject hud_objects[5]{};
  BatchObject hud_components[5]{};
  BatchHud    hud{{&hud_type.klass}};
  for (size_t i = 0; i < 5; ++i) {
    hud_objects[i].header.klass    = &game_object_class.klass;
    hud_objects[i].value           = true;
    hud_components[i].header.klass = &component_class.klass;
    hud_components[i].first        = &hud_objects[i].header;
    if (i < 4)
      hud.buttons[i] = &hud_components[i];
  }
  HudState().installed                = true;
  Config::Get().ui.hud_q_trials       = MissionHudVisibility::kNever;
  Config::Get().ui.hud_field_training = MissionHudVisibility::kAuto;
  Config::Get().ui.hud_outposts       = MissionHudVisibility::kAlways;
  Config::Get().ui.hud_missions       = MissionHudVisibility::kNever;
  int   native_enable                 = 0;
  auto* hud_owner                     = reinterpret_cast<MissionsHudViewController*>(&hud);
  MissionsHudViewController_OnEnable_Hook([&](auto*) { ++native_enable; }, hud_owner);
  Require(native_enable == 1 && !hud_objects[0].value && hud_objects[1].value && hud_objects[2].value
              && !hud_objects[3].value,
          "actual HUD consumer preserves native-first delivery and independent modes");
  Config::Get().ui.hud_q_trials = MissionHudVisibility::kAuto;
  Config::Get().ui.hud_outposts = MissionHudVisibility::kAuto;
  Config::Get().ui.hud_missions = MissionHudVisibility::kAuto;
  mission_hud::Refresh();
  Require(hud_objects[0].value && hud_objects[2].value && hud_objects[3].value,
          "Auto refresh invokes native setup and restores the missions baseline");
  hud_objects[4].value          = false;
  hud.buttons[3]                = &hud_components[4];
  Config::Get().ui.hud_missions = MissionHudVisibility::kAlways;
  ApplyHudVisibility(hud_owner);
  Require(hud_objects[4].value, "replacement missions button receives its own override");
  Config::Get().ui.hud_missions = MissionHudVisibility::kAuto;
  ApplyHudVisibility(hud_owner);
  Require(!hud_objects[4].value, "replacement missions button restores its own baseline, not the old button's");
  for (int i = 0; i < 1000; ++i)
    MissionHudFrame();
  Require(!hud_objects[4].value, "idle HUD frame work does not overwrite the native baseline");
  HudState().instances.clear();
  HudState().installed = false;

  auto& result_type   = Define("Digit.Client.UI", "ButtonResult", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  result_type.is_enum = true;
  auto& ft            = Define("Digit.Prime.ForbiddenTech", "ForbiddenTechManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& closure       = Define("", "<>c__DisplayClass12_0", IL2CPP_TYPE_CLASS, &object_class);
  closure.klass.declaringType = &ft.klass;
  auto& answer =
      AddMethod(closure, "<RequestAction>b__0", reinterpret_cast<void*>(&BatchAnswer), T(void_class), {T(result_type)});
  Il2CppObject   target{&closure.klass};
  Il2CppDelegate callback{};
  callback.target                          = &target;
  callback.method                          = &answer;
  Config::Get().ui.auto_confirm_ft_upgrade = true;
  Require(ConfirmForbiddenTechUpgrade(&callback) && batch_answers == 1 && batch_answer == 1,
          "exact FT closure retains its native confirm result");
  Config::Get().ui.auto_confirm_ft_upgrade = false;
  Require(!ConfirmForbiddenTechUpgrade(&callback) && batch_answers == 1, "disabled confirmation stays native");
  Config::Get().ui.auto_confirm_ft_upgrade = true;
  batch_throw_answer                       = true;
  Require(!ConfirmForbiddenTechUpgrade(&callback), "failed callback retains native popup fallback");
  batch_throw_answer = false;
  ft.klass.name      = "OtherForbiddenTechManager";
  Require(!ConfirmForbiddenTechUpgrade(&callback), "suffix collision cannot authorize a confirmation");
  ft.klass.name   = "ForbiddenTechManager";
  callback.target = nullptr;
  Require(!ConfirmForbiddenTechUpgrade(&callback), "missing target cannot authorize an action");

  callback.target                                     = &target;
  ft.klass.name                                       = "AllianceContributionViewController";
  Config::Get().ui.auto_confirm_alliance_contribution = true;
  Require(ConfirmAllianceContribution(&callback) && batch_answer == 3,
          "alliance contribution uses its own Yes result rather than FT Confirm");
  Config::Get().ui.auto_confirm_alliance_contribution = false;
  Require(!ConfirmAllianceContribution(&callback), "alliance disabled path retains the native confirmation");

  // No claim/purchase operation is issued. Filters, icons and native HUD rendering
  // retain their own qualification requirements outside the cases above.
  shown_scroller.Reset();
  Require(warp_roots.empty(), "component episodes release strong and weak custody on unbind");
  std::cout << "Production shop/claim lifecycle, cooldown routing and confirmation cases passed\n";
}
} // namespace
