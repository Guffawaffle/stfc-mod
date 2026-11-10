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
  il2cpp::List<unity::Transform*>* pool         = nullptr;
  bool                             regenerating = false;
  il2cpp::Object*                  spawning     = nullptr;
  il2cpp::Object*                  initializing = nullptr;
};
struct StoreFixtureBundle {
  Il2CppObject                    header;
  StoreInventoryTooltip*          tooltip = nullptr;
  ShopListScrollerViewController* owner   = nullptr;
};
struct StoreFixtureArray {
  Il2CppObject        header;
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t size   = 1;
  Il2CppObject*       items[1]{};
};

StoreFixtureTrigger store_test_plain{}, store_test_action{};
StoreFixtureWidget  store_test_widget{}, store_test_donor{};
StoreFixtureSource  store_test_source{};
StoreFixtureSmart   store_test_smart{};
StoreFixtureArray   store_test_templates{};
Il2CppObject store_test_item{}, store_test_other_item{}, store_test_spec{}, store_test_manager{}, store_test_loader{},
    store_test_search{}, store_test_locale{}, store_test_settings{}, store_test_object{}, store_test_context{},
    store_test_pool{}, store_test_currencies{}, store_test_soft{}, store_test_rect{};
bool    store_test_added = false, store_test_eligible = true, store_test_eligibility_throws = false,
        store_test_setup_throws = false, store_test_rebind = false, store_test_fetching = false,
        store_test_shop_active = true, store_test_donor_available = true;
int     store_test_adds = 0, store_test_setups = 0, store_test_currency = 0, store_test_original_releases = 0;
int     store_test_pool_count = 0, store_test_pool_reads = 0, store_test_eligibility_calls = 0;
int64_t store_test_seen_resource = 0;
std::function<void()> store_test_on_setup, store_test_on_eligibility, store_test_on_enable, store_test_on_description;
int                   store_test_context_reads = 0, store_test_release_at_context = 0;
int                   store_test_notifications           = 0;
bool                  store_test_release_on_notification = false;
const MethodInfo*     store_test_position_method         = nullptr;
const MethodInfo*     store_test_notify_method           = nullptr;
struct StorePositionBox {
  Il2CppObject   header;
  unity::Vector2 value{0.0f, 0.4f};
} store_test_position_box;
Il2CppObject* StoreTestInvoke(const MethodInfo* method, void*, void** args, Il2CppException**, bool& handled)
{
  handled = method == store_test_position_method || method == store_test_notify_method;
  if (!handled)
    return nullptr;
  if (method == store_test_position_method)
    return &store_test_position_box.header;
  Require(args && args[0], "same-position notification receives an owned Vector2 argument");
  const auto position = *static_cast<const unity::Vector2*>(args[0]);
  Require(position.x == 0.0f && position.y == 0.4f, "notification preserves native normalized position exactly");
  ++store_test_notifications;
  if (std::exchange(store_test_release_on_notification, false))
    AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook(
        [](AllianceStoreScroller*) {}, reinterpret_cast<AllianceStoreScroller*>(&store_test_source));
  return nullptr;
}

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
  ++store_test_eligibility_calls;
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
void StoreTestDescription(StoreInventoryTooltip*, const MethodInfo*)
{
  if (auto callback = std::exchange(store_test_on_description, {}))
    callback();
}
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
    AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook(
        [](AllianceStoreScroller*) {}, reinterpret_cast<AllianceStoreScroller*>(&store_test_source));
  return reinterpret_cast<AllianceStoreContext*>(&store_test_context);
}
bool StoreTestShopActive(AllianceStoreContext*, const MethodInfo*)
{ return store_test_shop_active; }
bool StoreTestFetching(AllianceStoreContext*, const MethodInfo*)
{ return store_test_fetching; }
int32_t StoreTestBundleCount(AllianceStoreContext*, const MethodInfo*)
{ return 120; }
int32_t StoreTestPoolCount(il2cpp::Object* self, const MethodInfo*)
{
  return self == reinterpret_cast<il2cpp::Object*>(&store_test_pool) ? store_test_pool_count
                                                                     : (store_test_currency ? 2 : 1);
}
unity::Transform* StoreTestPoolItem(il2cpp::Object* self, int32_t index, const MethodInfo*)
{
  Require(self == reinterpret_cast<il2cpp::Object*>(&store_test_pool) && index == 0 && store_test_pool_count == 1,
          "research reconciliation visits the actual current native pool card");
  ++store_test_pool_reads;
  return reinterpret_cast<unity::Transform*>(&store_test_rect);
}
StoreInventoryTooltip* StoreTestPoolTooltip(unity::Transform* self, Il2CppReflectionType*, bool include_inactive,
                                            const MethodInfo*)
{
  Require(self == reinterpret_cast<unity::Transform*>(&store_test_rect) && include_inactive,
          "pooled card lookup retains the native include-inactive contract");
  return reinterpret_cast<StoreInventoryTooltip*>(&store_test_widget);
}
il2cpp::ArrayObject* StoreTestTemplates(Il2CppReflectionType*, const MethodInfo*)
{
  store_test_templates.size = store_test_donor_available ? 1 : 0;
  return reinterpret_cast<il2cpp::ArrayObject*>(&store_test_templates);
}
AllianceStoreCurrencies* StoreTestCurrencies(AllianceStoreContext*, const MethodInfo*)
{ return reinterpret_cast<AllianceStoreCurrencies*>(&store_test_currencies); }
il2cpp::List<il2cpp::Object*>* StoreTestSoft(AllianceStoreCurrencies*, const MethodInfo*)
{ return reinterpret_cast<il2cpp::List<il2cpp::Object*>*>(&store_test_soft); }
int32_t StoreTestVisible(StoreSmartScroller*, const MethodInfo*)
{ return 4; }
unity::Vector2 StoreTestNormalized(StoreSmartScroller*, const MethodInfo*)
{ return {0.0f, 0.4f}; }
void  StoreTestNotify(StoreSmartScroller*, unity::Vector2, const MethodInfo*) {}
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
  auto& resources  = Define("UnityEngine", "Resources", IL2CPP_TYPE_CLASS, &object_class);
  auto& templates  = Define("UnityEngine", "Object[]", IL2CPP_TYPE_SZARRAY, &array_class);
  templates.klass.byval_arg.data.type = T(object_class);

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
  AddField(bundle, "_scrollerController", shop, offsetof(StoreFixtureBundle, owner));
  AddField(smart, "_regeneratingContent", bool_class, offsetof(StoreFixtureSmart, regenerating));
  AddField(smart, "_spawnerCoroutine", object_class, offsetof(StoreFixtureSmart, spawning));
  AddField(smart, "_dataHeldWhilstInitializing", object_class, offsetof(StoreFixtureSmart, initializing));
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
  store_test_rect.klass                             = &transform.klass;
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
  store_test_templates.header.klass             = &templates.klass;
  store_test_templates.items[0]                 = &store_test_donor.header;

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
  store_test_position_box.header.klass = &vector.klass;
  extra_value_box                      = &store_test_position_box.header;
  extra_value_payload                  = &store_test_position_box.value;
  store_test_position_method =
      &AddMethod(smart, "get_NormalizedPosition", reinterpret_cast<void*>(&StoreTestNormalized), T(vector), {});
  store_test_notify_method = &AddMethod(smart, "OnValueChangedCallback", reinterpret_cast<void*>(&StoreTestNotify),
                                        T(void_class), {T(vector)});
  const_cast<MethodInfo*>(store_test_position_method)->invoker_method = &WarpInvokerSentinel;
  const_cast<MethodInfo*>(store_test_notify_method)->invoker_method   = &WarpInvokerSentinel;
  tow_runtime_invoke_override                                         = &StoreTestInvoke;
  AddMethod(smart, "get_ScrollPosition", reinterpret_cast<void*>(&StoreTestPosition), T(float_class), {});
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&StoreTestPoolCount), T(int_class), {});
  AddMethod(list, "get_Item", reinterpret_cast<void*>(&StoreTestPoolItem), T(transform), {T(int_class)});
  AddMethod(transform, "GetComponentInChildren", reinterpret_cast<void*>(&StoreTestPoolTooltip), T(widget),
            {T(type_class), T(bool_class)});
  AddMethod(resources, "FindObjectsOfTypeAll", reinterpret_cast<void*>(&StoreTestTemplates), T(templates),
            {T(type_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);

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
  store_test_on_setup = [&] { AllianceStoreScroller_OnDidBindCanvasContext_Hook([](AllianceStoreScroller*) {}, view); };
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
  store_test_on_eligibility = [&] {
    AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && !store_test_widget.action
              && store_test_plain.enabled,
          "store release during eligibility cancels the old operation and restores the native tooltip");
  QueueStore(view, true);
  store_test_on_setup = [&] {
    AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !store_test_widget.action
              && !store_test_action.enabled && store_test_plain.enabled,
          "store release during setup cannot publish stale action state");
  QueueStore(view, true);
  store_test_on_enable = [&] {
    AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  };
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
  card.owner        = reinterpret_cast<ShopListScrollerViewController*>(view);
  BundleDataWidget_OnAboutToReleaseContext_Hook([](BundleDataWidget*) { ++store_test_original_releases; },
                                                reinterpret_cast<BundleDataWidget*>(&card));
  Require(store_test_original_releases == 1 && !store_test_widget.action && store_test_plain.enabled,
          "actual card release restores native behavior before forwarding exactly once");

  // Late native card bind after an earlier store pass must schedule a fresh
  // reconciliation, with the owner observed only after original bind returns.
  alliance_store_work.pending = false;
  card.owner                  = nullptr;
  BundleDataWidget_OnDidBindContext_Hook(
      [&](BundleDataWidget*) { card.owner = reinterpret_cast<ShopListScrollerViewController*>(view); },
      reinterpret_cast<BundleDataWidget*>(&card));
  Require(alliance_store_work.pending, "actual post-bind hook schedules late/recycled store cards");
  store_test_position_box.header.klass = &vec3_class.klass;
  Require(!store_game::NormalizedPosition(reinterpret_cast<StoreSmartScroller*>(&store_test_smart)),
          "foreign position box is unavailable, never a guessed scroll coordinate");
  store_test_position_box.header.klass = &vector.klass;
  store_test_smart.spawning            = reinterpret_cast<il2cpp::Object*>(&store_test_context);
  alliance_store_work.due              = Clock::time_point{};
  AllianceStoreFrame();
  Require(!store_test_notifications && !store_test_currency && alliance_store_work.pending,
          "live native spawner postpones all settled layout and currency work");
  store_test_smart.spawning = nullptr;
  alliance_store_work.due   = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_notifications == 1, "actual settled frame delivers exactly one same-position native notification");
  Require(store_test_currency == 1 && !alliance_store_work.pending,
          "actual frame calls native currency discovery once after bundle readiness");
  AllianceStoreFrame();
  Require(store_test_currency == 1, "idle frame makes no repeated currency or scene calls");
  QueueStore(view, true);
  store_test_release_on_notification = true;
  alliance_store_work.due            = Clock::time_point{};
  AllianceStoreFrame();
  Require(!alliance_store_work.source && store_test_currency == 1,
          "source release inside scroll notification prevents subsequent stale binding/currency work");
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
  AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  alliance_store_work.due = Clock::time_point{};
  AllianceStoreFrame();
  Require(store_test_currency == 1 && !alliance_store_work.source,
          "actual source release cancels pending work before native release");

  // A preference edit must reach already-bound cards without a card rebind,
  // donor acquisition, eligibility read, active store, or pending store event.
  auto& research = Config::Get().ui.show_alliance_games_research;
  Require(research && Config::Get().patches.alliancegamesstorehooks,
          "research visibility and its independent compatibility hook both start enabled");
  store_test_fetching = false;
  QueueStore(view, false);
  const auto research_native_search   = store_test_widget.search;
  const auto research_native_locale   = store_test_widget.locale;
  const auto research_native_settings = store_test_widget.settings;
  const auto research_native_enabled  = store_test_plain.enabled;
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Upgraded,
          "live preference fixture starts with the actual owned research action");
  alliance_store_work.pending = alliance_store_work.currency = alliance_store_work.layout = false;
  alliance_store_work.due = Clock::now() + std::chrono::hours(1);
  store_test_shop_active = store_test_donor_available = false;
  store_test_eligibility_throws                       = true;
  const auto eligibility_before_off                   = store_test_eligibility_calls;
  research                                            = false;
  warp_root_failure                                   = true;
  AllianceStoreFrame();
  Require(alliance_store_work.research_pending && owned->bound && store_test_widget.action,
          "unavailable restoration custody retains live-off work for a bounded retry");
  warp_root_failure                = false;
  alliance_store_work.research_due = {};
  alliance_store_tooltips.insert(alliance_store_tooltips.begin(), std::make_shared<OwnedStoreTooltip>());
  store_test_on_description = [&] {
    AllianceStoreScroller_OnDidBindCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  };
  AllianceStoreFrame();
  Require(!alliance_store_work.research_pending && !owned->bound && !store_test_widget.action
              && store_test_widget.trigger == reinterpret_cast<StoreTooltipTrigger*>(&store_test_plain)
              && store_test_widget.search == research_native_search
              && store_test_widget.locale == research_native_locale
              && store_test_widget.settings == research_native_settings
              && store_test_plain.enabled == research_native_enabled && !store_test_action.enabled
              && store_test_eligibility_calls == eligibility_before_off,
          "idle live-off restores complete native tooltip state while inactive and prunes safely through reentry");
  StoreInventoryTooltip* missing_donor   = nullptr;
  const auto             adds_before_off = store_test_adds, setups_before_off = store_test_setups;
  Require(ReconcileStoreTooltip(tooltip, missing_donor, roots) == ResearchUpdate::Unchanged
              && store_test_eligibility_calls == eligibility_before_off && store_test_adds == adds_before_off
              && store_test_setups == setups_before_off,
          "disabled research neither acquires an absent donor nor reads throwing eligibility or creates actions");

  // The current visible pool, not a manually invoked reconciliation, proves
  // enabling the setting can recover without reopening or regenerating cards.
  alliance_store_work.pending = alliance_store_work.currency = alliance_store_work.layout = false;
  store_test_eligibility_throws                                                           = false;
  store_test_donor_available                                                              = true;
  store_test_pool_count                                                                   = 1;
  research                                                                                = true;
  AllianceStoreFrame();
  Require(alliance_store_work.research_pending && !store_test_widget.action,
          "live-on remains pending while Settings obscures the current store");
  store_test_shop_active           = true;
  store_test_smart.pool            = nullptr;
  alliance_store_work.research_due = {};
  AllianceStoreFrame();
  Require(alliance_store_work.research_pending && !store_test_widget.action,
          "a missing native pool cannot consume the live-on preference change");
  store_test_smart.pool            = reinterpret_cast<il2cpp::List<unity::Transform*>*>(&store_test_pool);
  store_test_smart.spawning        = reinterpret_cast<il2cpp::Object*>(&store_test_context);
  alliance_store_work.research_due = {};
  AllianceStoreFrame();
  Require(alliance_store_work.research_pending && !store_test_widget.action,
          "live-on waits through native asynchronous card spawning");
  store_test_smart.spawning        = nullptr;
  alliance_store_work.research_due = {};
  const auto pool_before_on = store_test_pool_reads, currency_before_on = store_test_currency,
             notifications_before_on = store_test_notifications;
  store_test_donor_available         = false;
  AllianceStoreFrame();
  Require(alliance_store_work.research_pending && !store_test_widget.action
              && store_test_pool_reads == pool_before_on + 1,
          "a temporarily unavailable donor does not consume the live-on preference");
  store_test_donor_available       = true;
  alliance_store_work.research_due = {};
  AllianceStoreFrame();
  Require(!alliance_store_work.research_pending && store_test_pool_reads == pool_before_on + 2 && owned->bound
              && store_test_widget.action == reinterpret_cast<StoreTooltipAction*>(&store_test_action)
              && store_test_action.enabled && !store_test_plain.enabled && store_test_adds == 1,
          "live-on reconciles the existing pooled card and reuses its single owned native action");
  Require(store_test_currency == currency_before_on && store_test_notifications == notifications_before_on,
          "research-only live reconciliation does not fabricate currency or layout refreshes");
  const auto setups_after_on = store_test_setups;
  AllianceStoreFrame();
  Require(store_test_pool_reads == pool_before_on + 2 && store_test_setups == setups_after_on,
          "an acknowledged live-on preference performs no repeated idle reconciliation");

  research = false;
  AllianceStoreFrame();
  research                             = true;
  store_test_donor_available           = false;
  const auto pool_before_missing_donor = store_test_pool_reads;
  for (int attempt = 0; attempt < 4; ++attempt) {
    alliance_store_work.research_due = {};
    AllianceStoreFrame();
    Require(alliance_store_work.research_pending == (attempt < 3),
            "unavailable live-on reconciliation has a finite four-pass budget");
  }
  alliance_store_work.research_due = {};
  AllianceStoreFrame();
  Require(store_test_pool_reads == pool_before_missing_donor + 4 && !store_test_widget.action,
          "a permanently missing donor cannot trigger repeated idle pool scans");
  store_test_donor_available = true;
  research                   = false;
  AllianceStoreFrame();
  research = true;
  AllianceStoreFrame();
  Require(!alliance_store_work.research_pending && owned->bound && store_test_widget.action && store_test_adds == 1,
          "a later preference change can recover after the bounded discovery budget is exhausted");

  research                                     = false;
  store_test_donor_available                   = false;
  store_test_eligibility_throws                = true;
  const auto eligibility_before_disabled_store = store_test_eligibility_calls;
  QueueStore(view, true);
  alliance_store_work.due = {};
  AllianceStoreFrame();
  Require(!owned->bound && !store_test_widget.action && store_test_plain.enabled && !store_test_action.enabled
              && store_test_eligibility_calls == eligibility_before_disabled_store
              && store_test_currency == currency_before_on + 1
              && store_test_notifications == notifications_before_on + 1 && !alliance_store_work.pending
              && Config::Get().patches.alliancegamesstorehooks,
          "disabled research preserves native same-position layout and bundle-derived currency recovery");
  store_test_eligibility_throws = false;
  store_test_donor_available    = true;

  research                  = true;
  store_test_on_eligibility = [&] { research = false; };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Unavailable && !owned->bound
              && !store_test_widget.action && store_test_plain.enabled && !store_test_action.enabled,
          "policy-off during eligibility refuses before research publication");
  research            = true;
  store_test_on_setup = [&] { research = false; };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !owned->bound
              && !store_test_widget.action && store_test_plain.enabled && !store_test_action.enabled,
          "policy-off during native setup rolls back the owned action");
  research             = true;
  store_test_on_enable = [&] { research = false; };
  Require(ReconcileStoreTooltip(tooltip, donor, roots) == ResearchUpdate::Failed && !owned->bound
              && !store_test_widget.action && store_test_plain.enabled && !store_test_action.enabled
              && store_test_adds == 1,
          "policy-off during final enable revokes publication without adding another action");
  research = true;
  AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook([](AllianceStoreScroller*) {}, view);
  tow_runtime_invoke_override = nullptr;
  extra_value_box             = nullptr;
  extra_value_payload         = nullptr;
  owned.reset();
  alliance_store_tooltips.clear();
  roots.values.clear();
  Require(warp_roots.size() == before, "component and original-asset ownership is fully released by teardown");
}
} // namespace
