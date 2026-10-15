#pragma once

// The inventory, its use and purchase popups, and the shop and gift screens.

#include "server.h"
#include "ui.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>

struct InterstitialViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Interstitial", "InterstitialViewController">;
};

// The context of a use, purchase or donation popup.
struct InventoryForPopup : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryForPopup">;

  int64_t MaxItemsToUse()
  { return Get<"MaxItemsToUse", int64_t>(); }
  void SetMaxItemsToUse(int64_t value)
  { Set<"MaxItemsToUse">(value); }
  bool IsDonationUse()
  { return Get<"IsDonationUse", bool>(); }
  int64_t AmountOwned()
  { return Get<"AmountOwned", int64_t>(); }
  bool IsChestPurchase()
  { return Get<"IsChestPurchase", bool>(); }
};

struct InventoryUseRowWidget : Widget<InventoryForPopup> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryUseRowWidget">;
};

// The use popup, which also exchanges artifact shards.
struct InventoryUsePopupViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryUsePopupViewController">;

  // Only artifact bulk conversion uses this button, not the inventory entry button or single exchanges.
  unity::Component* convertAllButton()
  { return Field<"_convertAllButton", unity::Component*>(); }
};

// Also the base of the officer roster and the other inventory-style lists.
struct InventoryListViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryListViewController">;

  InputFieldWidget* inputField()
  { return Field<"_inputField", InputFieldWidget*>(); }
  bool isActiveAndEnabled()
  { return Get<"isActiveAndEnabled", bool>(); }
};

struct Bundle : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "Bundle">;
};

// A shop offer.
struct BundleDataWidget : Widget<Bundle> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Shop", "BundleDataWidget">;

  // ItemState flags; only the one the mod reads is named.
  static constexpr int32_t kCooldownTimerOn = 16384;

  int32_t CurrentState()
  { return Field<"_currentState", int32_t>(); }
  void AuxViewButtonPressedHandler()
  { Call<"AuxViewButtonPressedHandler">(); }
};

struct DrawerContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext">;

  bool Enabled()
  { return Field<"Enabled", bool>(); }
};

struct DrawerWidget : Widget<DrawerContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget">;

  void OnOpenButtonClicked()
  { Call<"OnOpenButtonClicked">(); }
};

// The gifts screen; its selection drawer holds bulk claim.
struct ShopListScrollerViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Shop", "ShopListScrollerViewController">;

  DrawerWidget* selectionDrawerWidget()
  { return Field<"_selectionDrawerWidget", DrawerWidget*>(); }
};

using ShopSceneManager = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager">;

// The forbidden tech inventory tiles and the large selected-item panel.
struct ForbiddenTechInventoryItemWidget : Widget<InventoryItem> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ForbiddenTechs", "ForbiddenTechInventoryItemWidget">;

  // ForbiddenTechInventoryItemWidget.WidgetType.
  enum class Type : int32_t {
    ScrollerEntry = 0,
    LargeShowcase = 1,
  };

  Type widgetType()
  { return Field<"_widgetType", Type>(); }
  // SmartScroller
  unity::Component* buffListScroller()
  { return Field<"_buffListScroller", unity::Component*>(); }
  TextLocalizer* name()
  { return Field<"_name", TextLocalizer*>(); }
};
