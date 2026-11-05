#pragma once

// Actual shop consumers in the existing managed-host fixture. No purchases,
// rewards, native detours or rendered Unity scenes are exercised here.
// Windows already includes shop.cc through the claims fixture above.
#if !_WIN32
#include "../src/mod/features/shop.cc"
#endif

namespace
{
struct LimitsContext {
  Il2CppObject header{};
  int64_t      maximum = 50, owned = 1000;
  bool         chest = false, donation = false;
};
struct LimitsRow {
  Il2CppObject  header{};
  Il2CppObject* context = nullptr;
};
struct LimitsComponent {
  Il2CppObject  header{};
  Il2CppObject* object = nullptr;
};
struct LimitsGameObject {
  Il2CppObject header{};
  bool         active = true;
};
std::function<void()> limits_on_maximum;
bool                  limits_throw_chest = false, limits_throw_donation = false, limits_throw_owned = false;
int                   limits_max_writes = 0, limits_closed = 0;
Il2CppObject*         LimitsContextGet(LimitsRow* row, const MethodInfo*)
{ return row->context; }
bool LimitsChestGet(LimitsContext* context, const MethodInfo*)
{
  if (limits_throw_chest)
    throw Il2CppExceptionWrapper(&thrown);
  return context->chest;
}
bool LimitsDonationGet(LimitsContext* context, const MethodInfo*)
{
  if (limits_throw_donation)
    throw Il2CppExceptionWrapper(&thrown);
  return context->donation;
}
int64_t LimitsMaximumGet(LimitsContext* context, const MethodInfo*)
{
  const auto value = context->maximum;
  if (limits_on_maximum)
    std::exchange(limits_on_maximum, {})();
  return value;
}
void LimitsMaximumSet(LimitsContext* context, int64_t value, const MethodInfo*)
{
  ++limits_max_writes;
  context->maximum = value;
}
int64_t LimitsOwnedGet(LimitsContext* context, const MethodInfo*)
{
  if (limits_throw_owned)
    throw Il2CppExceptionWrapper(&thrown);
  return context->owned;
}
void LimitsClose(Il2CppObject*, const MethodInfo*)
{ ++limits_closed; }
Il2CppObject* LimitsGameObjectGet(LimitsComponent* component, const MethodInfo*)
{ return component->object; }
void LimitsSetActive(LimitsGameObject* object, bool active, const MethodInfo*)
{ object->active = active; }

void ShopLimitsCases()
{
  auto& context_class = Define("Digit.Prime.Inventories", "InventoryForPopup", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(context_class, "get_MaxItemsToUse", reinterpret_cast<void*>(&LimitsMaximumGet), T(long_class), {});
  AddMethod(context_class, "set_MaxItemsToUse", reinterpret_cast<void*>(&LimitsMaximumSet), T(void_class),
            {T(long_class)});
  AddMethod(context_class, "get_AmountOwned", reinterpret_cast<void*>(&LimitsOwnedGet), T(long_class), {});
  AddMethod(context_class, "get_IsChestPurchase", reinterpret_cast<void*>(&LimitsChestGet), T(bool_class), {});
  AddMethod(context_class, "get_IsDonationUse", reinterpret_cast<void*>(&LimitsDonationGet), T(bool_class), {});
  // Property access resolves getter/setter metadata; expose the same native properties.
  AddProperty(context_class, "MaxItemsToUse", context_class.methods[0], context_class.methods[1]);
  AddProperty(context_class, "AmountOwned", context_class.methods[2]);
  AddProperty(context_class, "IsChestPurchase", context_class.methods[3]);
  AddProperty(context_class, "IsDonationUse", context_class.methods[4]);
  auto& row_class = Define("Digit.Prime.Inventories", "InventoryUseRowWidget", IL2CPP_TYPE_CLASS, &object_class);
  auto& context_get =
      AddMethod(row_class, "get_Context", reinterpret_cast<void*>(&LimitsContextGet), T(context_class), {});
  AddProperty(row_class, "Context", &context_get);
  LimitsContext context{{&context_class.klass}}, replacement = context;
  LimitsRow     row{{&row_class.klass}, &context.header};
  auto*         owner        = reinterpret_cast<InventoryUseRowWidget*>(&row);
  auto*         popup        = reinterpret_cast<InventoryForPopup*>(&context);
  int           originals    = 0;
  auto          original_row = [&](auto* received) {
    Require(received == owner, "native row receives original receiver");
    ++originals;
  };

  context.chest = true;
  for (int requested : {0, -1, 20, 80, 160, 10000}) {
    context.maximum                            = 50;
    Config::Get().ui.extend_chest_purchase_max = requested;
    InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
    const int64_t expected = requested > 50 ? std::min(requested, 160) : 50;
    Require(context.maximum == expected, "chest cap extends only upward and never beyond160");
  }
  context.maximum = 200;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 200, "native cap above160 is never lowered");
  context.maximum = 0;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 0, "zero native cap remains zero");
  context.maximum    = 50;
  limits_throw_chest = true;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50, "unavailable chest tag never authorizes an extension");
  limits_throw_chest = false;
  context.chest      = false;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50, "unrelated inventory remains unchanged");
  context.chest                              = true;
  Config::Get().ui.extend_chest_purchase_max = 160;
  limits_on_maximum                          = [&] { row.context = &replacement.header; };
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50 && replacement.maximum == 50,
          "reentrant context replacement cannot mutate an old popup's cap");
  row.context            = &context.header;
  row.context            = nullptr;
  const auto before_null = originals;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(originals == before_null + 1, "missing context still renders through native original once");
  row.context = &context.header;

#if _WIN32
  context.chest                           = false;
  context.donation                        = true;
  Config::Get().ui.extend_donation_slider = true;
  Config::Get().ui.extend_donation_max    = 80;
  int  setter_calls                       = 0;
  auto native_setter                      = [&](auto* received, int64_t value) {
    Require(received == popup, "native donation setter retains receiver");
    ++setter_calls;
    context.maximum = value;
  };
  InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 50);
  Require(context.maximum == 80 && setter_calls == 1, "tagged donation cap50 becomes configured80");
  InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 100);
  Require(context.maximum == 100 && setter_calls == 2, "nonmatching native cap remains native");
  for (int cap : {0, -1}) {
    Config::Get().ui.extend_donation_max = cap;
    context.maximum                      = 9007199254740993LL;
    InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 50);
    Require(context.maximum == 9007199254740993LL && setter_calls == 2,
            "unlimited donation retains exact previously established native cap");
  }
  context.donation = false;
  InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 50);
  Require(context.maximum == 50 && setter_calls == 3, "unrelated popup cap50 stays native");
  context.donation      = true;
  limits_throw_donation = true;
  InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 50);
  Require(setter_calls == 4 && context.maximum == 50, "unavailable donation tag falls back to native setter");
  limits_throw_donation                   = false;
  Config::Get().ui.extend_donation_slider = false;
  InventoryForPopup_set_MaxItemsToUse_Hook(native_setter, popup, 50);
  Require(setter_calls == 5, "disabled donation extension always calls native setter");
#else
  // Same production row consumer on Mac; these cases require a Mac test run.
  context.chest                           = false;
  context.donation                        = true;
  Config::Get().ui.extend_donation_slider = true;
  Config::Get().ui.extend_donation_max    = 80;
  context.maximum                         = 50;
  context.owned                           = 1000;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 80, "scoped Mac donation extends its own context");
  context.owned = 30;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 30, "Mac donation bound does not exceed known owned amount");
  context.maximum = 50;
  context.owned   = 0;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50, "zero owned amount cannot establish a positive donation bound");
  context.owned                        = 90;
  Config::Get().ui.extend_donation_max = 0;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 90, "Mac nonpositive configured cap uses the known owned amount");
  limits_throw_owned = true;
  context.maximum    = 50;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50, "unavailable owned amount leaves native Mac donation state alone");
  limits_throw_owned = false;
  context.donation   = false;
  InventoryUseRowWidget_SetWidgetData_Hook(original_row, owner);
  Require(context.maximum == 50, "Mac unrelated inventory remains native");
#endif

  auto& interstitial_class =
      Define("Digit.Prime.Interstitial", "InterstitialViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(interstitial_class, "CloseWhenReady", reinterpret_cast<void*>(&LimitsClose), T(void_class), {});
  Il2CppObject interstitial{&interstitial_class.klass};
  auto*        interstitial_owner = reinterpret_cast<InterstitialViewController*>(&interstitial);
  auto*        scene_owner        = reinterpret_cast<il2cpp::Object*>(&interstitial);
  int          plc = 0, interstitial_shows = 0, reveal_calls = 0;
  auto         native_plc                = [&](auto*) { ++plc; };
  auto         native_interstitial       = [&](auto*) { ++interstitial_shows; };
  Config::Get().ui.only_show_first_popup = true;
  popup_shown = false; // Simulates a fresh process; production never resets on a scene change.
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  InterstitialViewController_AboutToShow_Hook(native_interstitial, interstitial_owner);
  Require(plc == 1 && interstitial_shows == 1 && limits_closed == 1, "PLC first consumes shared session allowance");
  popup_shown   = false;
  limits_closed = 0;
  InterstitialViewController_AboutToShow_Hook(native_interstitial, interstitial_owner);
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  Require(plc == 1 && limits_closed == 0, "interstitial first consumes the same allowance after native show");
  Config::Get().ui.only_show_first_popup = false;
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  Config::Get().ui.only_show_first_popup = true;
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  Require(plc == 2, "disabling/re-enabling does not reset the process allowance");
  popup_shown                            = false;
  Config::Get().ui.only_show_first_popup = false;
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  Config::Get().ui.only_show_first_popup = true;
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
  Require(plc == 4, "initially disabled popups do not consume the later enabled allowance");
  popup_shown              = false;
  const auto before_nested = plc;
  InterstitialViewController_AboutToShow_Hook(
      [&](auto*) {
        ++interstitial_shows;
        ShopSceneManager_ShowPlcOfferPopup_Hook(native_plc, scene_owner);
      },
      interstitial_owner);
  Require(plc == before_nested + 1 && limits_closed == 1,
          "native-first interstitial reentry preserves the source's post-show allowance order");
  auto native_reveal = [&](auto*, bool ignore) {
    ++reveal_calls;
    return ignore;
  };
  Config::Get().ui.always_skip_reveal_sequence = true;
  Require(!ShopSceneManager_ShouldShowRevealSequence_Hook(native_reveal, scene_owner, true) && reveal_calls == 1,
          "skip reveal still evaluates native original once");
  Config::Get().ui.always_skip_reveal_sequence = false;
  Require(ShopSceneManager_ShouldShowRevealSequence_Hook(native_reveal, scene_owner, true)
              && !ShopSceneManager_ShouldShowRevealSequence_Hook(native_reveal, scene_owner, false)
              && reveal_calls == 3,
          "disabled reveal skip preserves both native results and arguments");

  auto& game_object = game_object_class;
  auto& get_object =
      AddMethod(component_class, "get_gameObject", reinterpret_cast<void*>(&LimitsGameObjectGet), T(game_object), {});
  AddProperty(component_class, "gameObject", &get_object);
  AddMethod(game_object, "SetActive", reinterpret_cast<void*>(&LimitsSetActive), T(void_class), {T(bool_class)});
  auto& use_popup =
      Define("Digit.Prime.Inventories", "InventoryUsePopupViewController", IL2CPP_TYPE_CLASS, &object_class);
  AddField(use_popup, "_convertAllButton", component_class, offsetof(LimitsRow, context));
  LimitsGameObject bulk_object{{&game_object.klass}}, single_object = bulk_object;
  LimitsComponent  bulk_button{{&component_class.klass}, &bulk_object.header};
  LimitsRow        use_view{{&use_popup.klass}};
  auto*            use_owner    = reinterpret_cast<InventoryUsePopupViewController*>(&use_view);
  int              native_binds = 0;
  auto             native_bind  = [&](auto*) {
    ++native_binds;
    use_view.context   = &bulk_button.header;
    bulk_object.active = true;
  };
  Config::Get().ui.disable_exchange_all = true;
  InventoryUsePopupViewController_OnDidBindCanvasContext_Hook(native_bind, use_owner);
  Require(native_binds == 1 && !bulk_object.active && single_object.active,
          "artifact native bind runs first; only the dedicated bulk button is hidden");
  Config::Get().ui.disable_exchange_all = false;
  InventoryUsePopupViewController_OnDidBindCanvasContext_Hook(native_bind, use_owner);
  Require(native_binds == 2 && bulk_object.active, "reopening after disabling retains native bulk button");
  Config::Get().ui.disable_exchange_all = true;
  InventoryUsePopupViewController_OnDidBindCanvasContext_Hook(
      [&](auto*) {
        ++native_binds;
        use_view.context = nullptr;
      },
      use_owner);
  Require(native_binds == 3, "missing bulk button does not prevent native binding");
  bulk_button.object = nullptr;
  InventoryUsePopupViewController_OnDidBindCanvasContext_Hook(native_bind, use_owner);
  Require(native_binds == 4 && single_object.active, "missing button GameObject leaves other exchanges untouched");
  Require(warp_roots.empty(), "shop operations release temporary custody");
  std::cout << "Production chest/donation limits, shared popups/reveal and artifact binding cases passed\n";
}
} // namespace
