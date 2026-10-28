#pragma once

// Actual production consumers; only managed methods, Unity component creation
// and the shared test runtime are substituted. No rendered UI or purchase.
#include "../src/mod/features/alliance_store.cc"
#include <functional>

namespace
{
struct StoreFixtureTrigger {
  Il2CppObject                header;
  store_game::OverridableBool show_header{true, true, false}, show_description{true, true, false};
  LocaleTextContext*          title           = nullptr;
  LocaleTextContext*          description     = nullptr;
  bool                        override_locale = true;
  int32_t                     anchor          = 2;
  unity::Vector2              size{210, 120}, offset{4, 8};
  unity::Transform*           target               = nullptr;
  bool                        interactable         = true;
  Il2CppString*               description_override = nullptr;
  StoreTooltipLoader*         loader               = nullptr;
  bool                        enabled              = true;
};
struct StoreFixtureWidget {
  Il2CppObject         header;
  InventoryItem*       item     = nullptr;
  StoreTooltipTrigger* trigger  = nullptr;
  StoreTooltipAction*  action   = nullptr;
  StoreActionLocale*   search   = nullptr;
  StoreActionLocale*   locale   = nullptr;
  StoreActionLocales*  settings = nullptr;
};
struct StoreFixtureSource {
  Il2CppObject        header;
  StoreSmartScroller* smart = nullptr;
};
struct StoreFixtureSmart {
  Il2CppObject                     header;
  il2cpp::List<unity::Transform*>* pool = nullptr;
};
struct StoreFixtureBundle {
  Il2CppObject           header;
  StoreInventoryTooltip* tooltip = nullptr;
};

StoreFixtureTrigger store_test_plain{}, store_test_action{};
StoreFixtureWidget  store_test_widget{}, store_test_donor{};
StoreFixtureSource  store_test_source{};
StoreFixtureSmart   store_test_smart{};
Il2CppObject store_test_item{}, store_test_other_item{}, store_test_spec{}, store_test_manager{}, store_test_loader{},
    store_test_search{}, store_test_locale{}, store_test_settings{}, store_test_object{}, store_test_context{},
    store_test_pool{}, store_test_currencies{}, store_test_soft{};
bool    store_test_added = false, store_test_eligible = true, store_test_eligibility_throws = false,
        store_test_setup_throws = false, store_test_rebind = false, store_test_fetching = false;
int     store_test_adds = 0, store_test_setups = 0, store_test_currency = 0, store_test_original_releases = 0;
int64_t store_test_seen_resource = 0;
std::function<void()> store_test_on_setup, store_test_on_eligibility, store_test_on_enable;
int                   store_test_context_reads = 0, store_test_release_at_context = 0;

InventoryItem* StoreTestContext(StoreFixtureWidget* self, const MethodInfo*)
{ return self->item; }
ResourceSpec* StoreTestSpec(InventoryItem*, const MethodInfo*)
{ return reinterpret_cast<ResourceSpec*>(&store_test_spec); }
int64_t StoreTestResource(ResourceSpec*, const MethodInfo*)
{ return 9007199254740993LL; }
StoreResearchManager* StoreTestResearch(const MethodInfo*)
{ return reinterpret_cast<StoreResearchManager*>(&store_test_manager); }
bool StoreTestEligible(StoreResearchManager*, int64_t id, store_game::ResearchTree tree, const MethodInfo*)
{
  Require(tree == store_game::ResearchTree::Standard, "native inventory eligibility keeps Standard research tree");
  store_test_seen_resource = id;
  if (store_test_eligibility_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (auto callback = std::exchange(store_test_on_eligibility, {}))
    callback();
  return store_test_eligible;
}
StoreTooltipTrigger* StoreTestPlain(StoreInventoryTooltip*, Il2CppReflectionType*, const MethodInfo*)
{ return reinterpret_cast<StoreTooltipTrigger*>(&store_test_plain); }
unity::GameObject* StoreTestGameObject(StoreInventoryTooltip*, const MethodInfo*)
{ return reinterpret_cast<unity::GameObject*>(&store_test_object); }
StoreTooltipAction* StoreTestAction(unity::GameObject*, Il2CppReflectionType*, const MethodInfo*)
{ return store_test_added ? reinterpret_cast<StoreTooltipAction*>(&store_test_action) : nullptr; }
StoreTooltipAction* StoreTestAddAction(unity::GameObject*, Il2CppReflectionType*, const MethodInfo*)
{
  store_test_added = true;
  ++store_test_adds;
  return reinterpret_cast<StoreTooltipAction*>(&store_test_action);
}
bool StoreTestEnabled(StoreFixtureTrigger* self, const MethodInfo*)
{ return self->enabled; }
void StoreTestSetEnabled(StoreFixtureTrigger* self, bool enabled, const MethodInfo*)
{
  self->enabled = enabled;
  if (self == &store_test_action && enabled)
    if (auto callback = std::exchange(store_test_on_enable, {}))
      callback();
}
void StoreTestDescription(StoreInventoryTooltip*, const MethodInfo*) {}
void StoreTestSetup(StoreInventoryTooltip*, const MethodInfo*)
{
  ++store_test_setups;
  if (auto callback = std::exchange(store_test_on_setup, {}))
    callback();
  Require(store_test_action.loader == reinterpret_cast<StoreTooltipLoader*>(&store_test_loader),
          "native setup receives the initialized live plain loader");
  Require(store_test_widget.trigger == reinterpret_cast<StoreTooltipTrigger*>(&store_test_action)
              && store_test_widget.action == reinterpret_cast<StoreTooltipAction*>(&store_test_action),
          "both wrapper references route description and action to the native replacement");
  if (store_test_rebind)
    store_test_widget.item = reinterpret_cast<InventoryItem*>(&store_test_other_item);
  if (store_test_setup_throws)
    throw Il2CppExceptionWrapper(&thrown);
}
AllianceStoreContext* StoreTestStoreContext(AllianceStoreScroller*, const MethodInfo*)
{
  if (++store_test_context_reads == store_test_release_at_context)
    AllianceStoreReleased_Hook([](AllianceStoreScroller*) {},
                               reinterpret_cast<AllianceStoreScroller*>(&store_test_source));
  return reinterpret_cast<AllianceStoreContext*>(&store_test_context);
}
bool StoreTestShopActive(AllianceStoreContext*, const MethodInfo*)
{ return true; }
bool StoreTestFetching(AllianceStoreContext*, const MethodInfo*)
{ return store_test_fetching; }
int32_t StoreTestBundleCount(AllianceStoreContext*, const MethodInfo*)
{ return 120; }
int32_t StoreTestPoolCount(il2cpp::Object* self, const MethodInfo*)
{ return self == reinterpret_cast<il2cpp::Object*>(&store_test_pool) ? 0 : (store_test_currency ? 2 : 1); }
AllianceStoreCurrencies* StoreTestCurrencies(AllianceStoreContext*, const MethodInfo*)
{ return reinterpret_cast<AllianceStoreCurrencies*>(&store_test_currencies); }
il2cpp::List<il2cpp::Object*>* StoreTestSoft(AllianceStoreCurrencies*, const MethodInfo*)
{ return reinterpret_cast<il2cpp::List<il2cpp::Object*>*>(&store_test_soft); }
int32_t StoreTestVisible(StoreSmartScroller*, const MethodInfo*)
{ return 4; }
float StoreTestPosition(StoreSmartScroller*, const MethodInfo*)
{ return 0.4f; }
void StoreTestCurrency(AllianceStoreContext*, const MethodInfo*)
{ ++store_test_currency; }

void AllianceStoreNativeCases(std::string_view mode)
{
  auto& behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  auto& plain     = Define("Digit.Client.Tooltip", "TooltipTrigger", IL2CPP_TYPE_CLASS, &behaviour);
  auto& action    = Define("Digit.Client.Tooltip", "TooltipActionTrigger", IL2CPP_TYPE_CLASS, &plain);
  auto& widget =
      Define("Digit.Prime.Inventories", "InventoryTooltipTriggerWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& loader        = Define("Digit.Client.Tooltip", "TooltipLoadAndShow", IL2CPP_TYPE_CLASS, &object_class);
  auto& locale        = Define("Digit.Prime.Actions", "ActionLocaleSetting", IL2CPP_TYPE_CLASS, &object_class);
  auto& settings      = Define("Digit.Prime.Actions", "ActionLocaleSettingsList", IL2CPP_TYPE_CLASS, &object_class);
  auto& item          = Define("Digit.PrimeServer.Models", "InventoryItem", IL2CPP_TYPE_CLASS, &object_class);
  auto& spec          = Define("Digit.PrimeServer.Models", "ResourceSpec", IL2CPP_TYPE_CLASS, &object_class);
  auto& manager       = Define("Digit.Prime.Research", "ResearchManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& hub           = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto& transform     = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &component_class);
  auto& vector        = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, 8);
  auto& override_bool = Define(mode == "store-fields-mismatch" ? "Foreign" : "Digit.Client.Serialization",
                               "OverridableBool", IL2CPP_TYPE_VALUETYPE, nullptr, 3);
  auto& shop = Define("Digit.Prime.Shop", "ShopListScrollerViewController", IL2CPP_TYPE_CLASS, &component_class);
  auto& source =
      Define("Digit.Prime.Alliances.Games", "AllianceGamesStoreScrollerViewController", IL2CPP_TYPE_CLASS, &shop);
  auto& context =
      Define("Digit.Prime.Alliances.Games", "AllianceGamesSectionContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& currencies = Define("Digit.Prime.HUD", "GenericHudResourcesData", IL2CPP_TYPE_CLASS, &object_class);
  auto& smart      = Define("Digit.Client.UI", "SmartScrollerBase", IL2CPP_TYPE_CLASS, &component_class);
  auto& list       = Define("System.Collections.Generic", "List`1", IL2CPP_TYPE_CLASS, &object_class);
  auto& bundle     = Define("Digit.Prime.Shop", "BundleDataWidget", IL2CPP_TYPE_CLASS, &component_class);

  AddField(widget, "_tooltipTrigger", plain, offsetof(StoreFixtureWidget, trigger));
  AddField(widget, "_tooltipActionTrigger", action, offsetof(StoreFixtureWidget, action));
  AddField(widget, "_searchActionLocaleSetting", locale, offsetof(StoreFixtureWidget, search));
  AddField(widget, "_actionLocaleSetting", locale, offsetof(StoreFixtureWidget, locale));
  AddField(widget, "_settings", settings, offsetof(StoreFixtureWidget, settings));
  AddField(plain, "_showHeader", override_bool, offsetof(StoreFixtureTrigger, show_header));
  AddField(plain, "_showDescription", override_bool, offsetof(StoreFixtureTrigger, show_description));
  AddField(plain, "_header", warp_locale_class, offsetof(StoreFixtureTrigger, title));
  AddField(plain, "_description", warp_locale_class, offsetof(StoreFixtureTrigger, description));
  AddField(plain, "_overrideLocale", bool_class, offsetof(StoreFixtureTrigger, override_locale));
  AddField(plain, "_anchorOverride", int_class, offsetof(StoreFixtureTrigger, anchor));
  AddField(plain, "_sizeOverride", vector, offsetof(StoreFixtureTrigger, size));
  AddField(plain, "_positionOverrideTarget", transform, offsetof(StoreFixtureTrigger, target));
  AddField(plain, "_positionOverrideOffset", vector, offsetof(StoreFixtureTrigger, offset));
  AddField(plain, "_isInteractable", bool_class, offsetof(StoreFixtureTrigger, interactable));
  AddField(plain, "<DescriptionOverrideText>k__BackingField", string_class,
           offsetof(StoreFixtureTrigger, description_override));
  AddField(plain, "_loader", loader, offsetof(StoreFixtureTrigger, loader));
  AddField(shop, "_smartScroller", smart, offsetof(StoreFixtureSource, smart));
  AddField(smart, "_spawnedItems", list, offsetof(StoreFixtureSmart, pool));
  AddField(bundle, "_invTooltip", widget, offsetof(StoreFixtureBundle, tooltip));
  Require(store_game::Members().Resolve() == (mode != "store-fields-mismatch"),
          "actual store schema rejects a same-sized foreign override struct");
  if (mode == "store-fields-mismatch")
    return;

  store_test_plain.header.klass  = &plain.klass;
  store_test_action.header.klass = &action.klass;
  store_test_widget.header.klass = store_test_donor.header.klass = &widget.klass;
  store_test_item.klass = store_test_other_item.klass = &item.klass;
  store_test_spec.klass                               = &spec.klass;
  store_test_manager.klass                            = &manager.klass;
  store_test_loader.klass                             = &loader.klass;
  store_test_search.klass = store_test_locale.klass = &locale.klass;
  store_test_settings.klass                         = &settings.klass;
  store_test_object.klass                           = &game_object_class.klass;
  store_test_source.header.klass                    = &source.klass;
  store_test_context.klass                          = &context.klass;
  store_test_smart.header.klass                     = &smart.klass;
  store_test_pool.klass = store_test_soft.klass = &list.klass;
  store_test_currencies.klass                   = &currencies.klass;
  store_test_source.smart                       = reinterpret_cast<StoreSmartScroller*>(&store_test_smart);
  store_test_smart.pool                         = reinterpret_cast<il2cpp::List<unity::Transform*>*>(&store_test_pool);
  store_test_widget.item                        = reinterpret_cast<InventoryItem*>(&store_test_item);
  store_test_widget.trigger                     = reinterpret_cast<StoreTooltipTrigger*>(&store_test_plain);
  store_test_plain.loader                       = reinterpret_cast<StoreTooltipLoader*>(&store_test_loader);
  store_test_donor.action                       = reinterpret_cast<StoreTooltipAction*>(&store_test_action);
  store_test_donor.search                       = reinterpret_cast<StoreActionLocale*>(&store_test_search);
  store_test_donor.locale                       = reinterpret_cast<StoreActionLocale*>(&store_test_locale);
  store_test_donor.settings                     = reinterpret_cast<StoreActionLocales*>(&store_test_settings);

  AddMethod(widget, "get_Context", reinterpret_cast<void*>(&StoreTestContext), T(item), {});
  AddMethod(widget, "GetComponent", reinterpret_cast<void*>(&StoreTestPlain), T(plain), {T(type_class)});
  AddMethod(widget, "get_gameObject", reinterpret_cast<void*>(&StoreTestGameObject), T(game_object_class), {});
  // Replace this mode's existing fake Type overload, rather than adding an
  // impossible overload distinguished only by its return type.
  for (auto* method : game_object_class.methods)
    if (std::string_view(method->name) == "GetComponent" && method->parameters_count == 1) {
      auto* mutable_method                 = const_cast<MethodInfo*>(method);
      mutable_method->methodPointer        = reinterpret_cast<Il2CppMethodPointer>(&StoreTestAction);
      mutable_method->virtualMethodPointer = mutable_method->methodPointer;
      mutable_method->return_type          = T(action);
    }
  AddMethod(game_object_class, "AddComponent", reinterpret_cast<void*>(&StoreTestAddAction), T(action),
            {T(type_class)});
  AddMethod(plain, "get_enabled", reinterpret_cast<void*>(&StoreTestEnabled), T(bool_class), {});
  AddMethod(behaviour, "set_enabled", reinterpret_cast<void*>(&StoreTestSetEnabled), T(void_class), {T(bool_class)});
  AddMethod(item, "get_ResourceSpec", reinterpret_cast<void*>(&StoreTestSpec), T(spec), {});
  AddMethod(spec, "get_Id", reinterpret_cast<void*>(&StoreTestResource),
            mode == "store-research-bad-id" ? T(int_class) : T(long_class), {});
  AddMethod(hub, "get_ResearchManager", reinterpret_cast<void*>(&StoreTestResearch), T(manager), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(manager, "IsResourceRequiredByVisibleResearches", reinterpret_cast<void*>(&StoreTestEligible),
            T(bool_class), {T(long_class), T(int_class)});
  AddMethod(widget, "UpdateTooltipDescription", reinterpret_cast<void*>(&StoreTestDescription), T(void_class), {});
  AddMethod(widget, "SetupActionTooltipTrigger", reinterpret_cast<void*>(&StoreTestSetup), T(void_class), {});
  AddMethod(source, "get_AllianceGamesContext", reinterpret_cast<void*>(&StoreTestStoreContext), T(context), {});
  AddMethod(context, "get_IsShopViewActive", reinterpret_cast<void*>(&StoreTestShopActive), T(bool_class), {});
  AddMethod(context, "get_IsFetchingBundlesInProgress", reinterpret_cast<void*>(&StoreTestFetching), T(bool_class), {});
  AddMethod(context, "get_BundlesCount", reinterpret_cast<void*>(&StoreTestBundleCount), T(int_class), {});
  AddMethod(context, "UpdateStoreCurrency", reinterpret_cast<void*>(&StoreTestCurrency), T(void_class), {});
  AddMethod(context, "get_StoreCurrency", reinterpret_cast<void*>(&StoreTestCurrencies), T(currencies), {});
  AddMethod(currencies, "get_SoftResources", reinterpret_cast<void*>(&StoreTestSoft), T(list), {});
  AddMethod(smart, "get_VisibleItemsCount", reinterpret_cast<void*>(&StoreTestVisible), T(int_class), {});
  AddMethod(smart, "get_ScrollPosition", reinterpret_cast<void*>(&StoreTestPosition), T(float_class), {});
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&StoreTestPoolCount), T(int_class), {});

  const auto        before  = warp_roots.size();
  auto*             view    = reinterpret_cast<AllianceStoreScroller*>(&store_test_source);
  auto*             tooltip = reinterpret_cast<StoreInventoryTooltip*>(&store_test_widget);
  auto*             donor   = reinterpret_cast<StoreInventoryTooltip*>(&store_test_donor);
  store_game::Roots roots;
  QueueStore(view, true);
  if (mode == "store-research-bad-id") {
    Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && store_test_adds == 0
                && store_test_plain.enabled && !store_test_widget.action,
            "unavailable resource identity is not false eligibility or an authorization to mutate");
    alliance_store_work.source.Reset();
    return;
  }
  warp_root_failure = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && !store_test_adds,
          "failed strong custody prevents all component changes");
  warp_root_failure = false;
  weak_root_refusal = [](Il2CppObject* object) { return object == &store_test_source.header; };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && store_test_plain.enabled
              && !store_test_action.enabled && !store_test_widget.action,
          "failed store custody cannot commit a replacement without a release owner");
  weak_root_refusal = nullptr;
  auto expired      = std::make_shared<OwnedStoreTooltip>();
  alliance_store_tooltips.insert(alliance_store_tooltips.begin(), expired);
  store_test_on_setup = [&] { AllianceStoreBound_Hook([](AllianceStoreScroller*) {}, view); };
  const auto first    = ReconcileStoreTooltip(tooltip, donor, roots);
  if (first != ResearchUpdate::Upgraded) {
    std::cerr << "store first=" << static_cast<int>(first) << " adds=" << store_test_adds
              << " resource=" << store_test_seen_resource << "\n";
    for (auto& error : errors)
      std::cerr << error << "\n";
  }
  Require(first == ResearchUpdate::Upgraded && store_test_adds == 1 && store_test_seen_resource == 9007199254740993LL
              && !store_test_plain.enabled && store_test_action.enabled,
          "actual typed consumer upgrades once with exact Int64 resource identity and native eligibility");
  store_test_eligibility_throws = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && store_test_widget.action,
          "failed eligibility is unavailable, not confirmed ineligibility");
  store_test_eligibility_throws = false;
  store_test_eligible           = false;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Restored && !store_test_widget.action
              && store_test_widget.trigger == reinterpret_cast<StoreTooltipTrigger*>(&store_test_plain)
              && store_test_plain.enabled && !store_test_action.enabled && !store_test_widget.search,
          "confirmed ineligible reuse restores all original wrapper and enabled state");
  store_test_eligible = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Upgraded && store_test_adds == 1,
          "eligible pooled reuse retains a single owned native action component");
  store_test_setup_throws = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !store_test_widget.action
              && store_test_plain.enabled && !store_test_action.enabled && !store_test_widget.search,
          "native setup failure rolls back the complete original plain wrapper");
  store_test_setup_throws = false;
  store_test_rebind       = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !store_test_widget.action
              && store_test_plain.enabled && !store_test_action.enabled,
          "context replacement during native setup cannot publish the former material action");
  store_test_rebind      = false;
  store_test_widget.item = reinterpret_cast<InventoryItem*>(&store_test_item);
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Upgraded,
          "rebinding recovers after rollback");
  store_test_on_eligibility = [&] { AllianceStoreReleased_Hook([](AllianceStoreScroller*) {}, view); };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && !store_test_widget.action
              && store_test_plain.enabled,
          "store release during eligibility cancels the old operation and restores the native tooltip");
  QueueStore(view, true);
  store_test_on_setup = [&] { AllianceStoreReleased_Hook([](AllianceStoreScroller*) {}, view); };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !store_test_widget.action
              && !store_test_action.enabled && store_test_plain.enabled,
          "store release during setup cannot publish stale action state");
  QueueStore(view, true);
  store_test_on_enable = [&] { AllianceStoreReleased_Hook([](AllianceStoreScroller*) {}, view); };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !store_test_widget.action
              && !store_test_action.enabled && store_test_plain.enabled,
          "store release during final enable revokes publication even for the same item");
  QueueStore(view, true);
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Upgraded, "restore after cancelled enable");
  StoreFixtureTrigger foreign{};
  foreign.header.klass      = &action.klass;
  store_test_widget.trigger = reinterpret_cast<StoreTooltipTrigger*>(&foreign);
  store_test_widget.action  = reinterpret_cast<StoreTooltipAction*>(&foreign);
  auto owned                = OwnedTooltip(tooltip);
  Require(owned && RestoreStoreTooltip(*owned)
              && store_test_widget.trigger == reinterpret_cast<StoreTooltipTrigger*>(&foreign)
              && store_test_widget.action == reinterpret_cast<StoreTooltipAction*>(&foreign) && foreign.enabled,
          "cleanup preserves a replacement native action owner");
  store_test_widget.trigger = reinterpret_cast<StoreTooltipTrigger*>(&store_test_plain);
  store_test_widget.action  = nullptr;
  store_test_plain.enabled  = true;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Upgraded,
          "native baseline can be rebound again");
  StoreFixtureBundle card{};
  card.header.klass = &bundle.klass;
  card.tooltip      = tooltip;
  StoreBundleReleased_Hook([](BundleDataWidget*) { ++store_test_original_releases; },
                           reinterpret_cast<BundleDataWidget*>(&card));
  Require(store_test_original_releases == 1 && !store_test_widget.action && store_test_plain.enabled,
          "actual card release restores native behavior before forwarding exactly once");

  alliance_store_work.due = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_currency == 1 && !alliance_store_work.pending,
          "actual frame calls native currency discovery once after bundle readiness");
  AllianceStoreFrame();
  Require(store_test_currency == 1, "idle frame makes no repeated currency or scene calls");
  QueueStore(view, true);
  store_test_release_at_context = store_test_context_reads + 2;
  alliance_store_work.due       = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_currency == 1 && !alliance_store_work.source,
          "release during final context retrieval cannot refresh the retired currency context");
  store_test_release_at_context = 0;
  store_test_fetching           = true;
  QueueStore(view, true);
  alliance_store_work.due = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_currency == 1, "loading bundle data cannot authorize premature currency replacement");
  QueueStore(view, true);
  AllianceStoreReleased_Hook([](AllianceStoreScroller*) {}, view);
  alliance_store_work.due = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_currency == 1 && !alliance_store_work.source,
          "actual source release cancels pending work before native release");
  owned.reset();
  alliance_store_tooltips.clear();
  roots.values.clear();
  Require(warp_roots.size() == before, "component and original-asset ownership is fully released by teardown");
}
} // namespace
