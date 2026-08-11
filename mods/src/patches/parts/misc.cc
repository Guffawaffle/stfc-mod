/**
 * @file misc.cc
 * @brief Miscellaneous quality-of-life patches and crash fixes.
 *
 * A collection of independent patches that don't warrant their own file:
 *   - Donation slider extension (raise the 50-item cap)
 *   - Tagged chest-purchase slider extension
 *   - Bundle cooldown bypass (click through to info view while on cooldown)
 *   - Resolution list cleanup (deduplicate and normalize refresh rates)
 *   - Buff extraction null-guard (prevent crash on null list entries)
 *   - Shop reveal sequence skip
 *   - First interstitial popup dismissal
 */
#include "config.h"
#include "errormsg.h"

#if _WIN32
#include "patches/hook_registry.h"
#endif

#include <prime/BundleDataWidget.h>
#include <prime/ClientModifierType.h>
#include <prime/IList.h>
#include <prime/InterstitialViewController.h>
#include <prime/InventoryForPopup.h>
#if _WIN32
#include <prime/InventoryUseRowWidget.h>
#endif

#include <il2cpp/il2cpp_helper.h>

#if _WIN32
#include <spdlog/spdlog.h>
#endif
#include <spud/detour.h>

#if _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <vector>

// ─── Inventory Slider Extensions ─────────────────────────────────────────────

#if _WIN32
namespace
{
constexpr int64_t kMaximumChestPurchaseMax = 160;

constexpr HookDescriptor kChestPurchaseSliderHook{
    "InventoryUseRowWidget.SetWidgetData",
    "Raise the quantity ceiling only for inventory rows tagged by the game as chest purchases.",
    {"Assembly-CSharp", "Digit.Prime.Inventories", "InventoryUseRowWidget", "SetWidgetData"},
    "Tagged chest-purchase sliders will retain the native quantity ceiling.",
    HookSupportTier::Production};
} // namespace
#endif

/**
 * @brief Hook: InventoryForPopup::set_MaxItemsToUse
 *
 * Intercepts the donation slider cap to allow larger donations.
 * Original method: sets the maximum slider value (hard-coded to 50 for donations).
 * Our modification: when extend_donation_slider is enabled and the caller is a
 *   donation popup with cap == 50, replaces it with the user's configured max.
 *   A non-positive configured max preserves the popup's initial unlimited value.
 */
void InventoryForPopup_set_MaxItemsToUse(auto original, InventoryForPopup* a1, int64_t a2)
{
  if (!a1) {
    return;
  }

  if (a1->IsDonationUse && a2 == 50 && Config::Get().extend_donation_slider) {
    const auto max = Config::Get().extend_donation_max;
    if (max > 0) {
      a2 = max;
    } else {
      // Leave the initial unlimited value in place instead of applying the game's donation cap.
      return;
    }
  }

  original(a1, a2);
}

/**
 * @brief Hook: InventoryUseRowWidget::SetWidgetData
 *
 * The chest-purchase context is tagged only after its native MaxItemsToUse value is assigned, so the earlier setter
 * hook cannot reliably distinguish it. This later row-render seam sees both values and raises only tagged chest
 * purchase rows with a positive native ceiling. It never replaces an unknown/non-positive sentinel or lowers a
 * native maximum. A non-positive configured value disables the hook at install.
 */
#if _WIN32
void InventoryUseRowWidget_SetWidgetData(auto original, InventoryUseRowWidget* widget)
{
  if (widget) {
    if (auto* context = widget->Context; context && context->IsChestPurchase) {
      const auto native_max    = context->MaxItemsToUse;
      const auto requested_max = static_cast<int64_t>(Config::Get().extend_chest_purchase_max);
      const auto bounded_max   = std::min(requested_max, kMaximumChestPurchaseMax);
      if (native_max > 0 && bounded_max > native_max) {
        context->MaxItemsToUse = bounded_max;
        spdlog::info("[ChestPurchaseSlider] extended quantity ceiling from {} to {}", native_max, bounded_max);
      } else if (native_max > bounded_max) {
        spdlog::info("[ChestPurchaseSlider] retained native quantity ceiling {} above bounded configured ceiling {} "
                     "(requested {})",
                     native_max, bounded_max, requested_max);
      }
    }
  }

  original(widget);
}
#endif

// ─── Bundle Cooldown Bypass ──────────────────────────────────────────────────

/**
 * @brief Hook: BundleDataWidget::OnActionButtonPressedCallback
 *
 * Intercepts shop bundle button presses to allow interaction during cooldown.
 * Original method: triggers the bundle's primary action (purchase flow).
 * Our modification: if the bundle is on cooldown, redirects to the auxiliary
 *   info view instead of blocking the press entirely.
 */
void BundleDataWidget_OnActionButtonPressedCallback(auto original, BundleDataWidget* _this)
{
  if (_this->CurrentState & BundleDataWidget::ItemState::CooldownTimerOn) {
    _this->AuxViewButtonPressedHandler();
  } else {
    original(_this);
  }
}

/**
 * @brief Installs inventory-slider and bundle-cooldown patches.
 *
 * Hooks the donation setter and tagged chest-purchase row renderer on Windows, plus the bundle action callback.
 */
void InstallMiscPatches()
{
#if _WIN32
  HookModuleHealth chest_slider_hooks("ChestPurchaseSliderHooks");

  auto h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryForPopup");
  if (!h.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Inventories", "InventoryForPopup");
  } else {
    auto ptr = h.GetMethod("set_MaxItemsToUse");
    if (!ptr) {
      ErrorMsg::MissingMethod("InventoryForPopup", "set_MaxItemsToUse");
    } else {
      SPUD_STATIC_DETOUR(ptr, InventoryForPopup_set_MaxItemsToUse);
    }
  }

  const auto configured_chest_max = Config::Get().extend_chest_purchase_max;
  if (configured_chest_max <= 0) {
    chest_slider_hooks.record_skipped(kChestPurchaseSliderHook, "configured maximum is non-positive");
  } else {
    if (configured_chest_max > kMaximumChestPurchaseMax) {
      spdlog::warn("[ChestPurchaseSlider] configured extension ceiling {} exceeds verified transaction ceiling {}; "
                   "clamping the extension (higher native ceilings remain unchanged)",
                   configured_chest_max, kMaximumChestPurchaseMax);
    }

    auto& row_widget = InventoryUseRowWidget::get_class_helper();
    if (!row_widget.isValidHelper()) {
      chest_slider_hooks.record_missing_helper(kChestPurchaseSliderHook);
    } else if (auto set_widget_data = row_widget.GetMethod("SetWidgetData", 0); set_widget_data == nullptr) {
      chest_slider_hooks.record_missing_method(kChestPurchaseSliderHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(chest_slider_hooks, kChestPurchaseSliderHook, set_widget_data,
                                       InventoryUseRowWidget_SetWidgetData);
    }
  }

  chest_slider_hooks.log_summary();
#endif

  auto bundle_data_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleDataWidget");
  if (!bundle_data_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "BundleDataWidget");
  } else {
    auto ptr = bundle_data_widget.GetMethod("OnActionButtonPressedCallback");
    if (!ptr) {
      ErrorMsg::MissingMethod("BundleDataWidget", "OnActionButtonPressedCallback");
    } else
      SPUD_STATIC_DETOUR(ptr, BundleDataWidget_OnActionButtonPressedCallback);
  }
}

// ─── Resolution List Fix ─────────────────────────────────────────────────────

struct Resolution {
  int m_Width;
  int m_Height;
  int m_RefreshRate;

  bool operator==(const Resolution& other) const
  { return this->m_Height == other.m_Height && this->m_Width == other.m_Width; }
};

struct ResolutionArray {
  Il2CppObject obj;
  void*        bounds;
  size_t       maxlength;
  Resolution   data[1];
};

/**
 * @brief Hook: UnityEngine.Screen::get_resolutions
 *
 * Intercepts the resolution list to clean up duplicates and normalize refresh rates.
 * Original method: returns the raw array of supported screen resolutions.
 * Our modification: on Windows, finds the maximum refresh rate for the native
 *   resolution and (if show_all_resolutions is set) normalizes all entries to
 *   that rate, then deduplicates. This prevents the settings menu from showing
 *   the same resolution multiple times at different refresh rates.
 */
ResolutionArray* GetResolutions_Hook(auto original)
{
  auto resolutions = original();

#if _WIN32
  auto screenWidth  = GetSystemMetrics(SM_CXSCREEN);
  auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

  int targetRefreshRate = 0;
  for (int i = 0; i < resolutions->maxlength; ++i) {
    auto ores = resolutions->data[i];
    if (ores.m_Width == screenWidth && ores.m_Height == screenHeight) {
      targetRefreshRate = std::max(ores.m_RefreshRate, targetRefreshRate);
    }
  }

  std::vector<Resolution> res;
  for (int i = 0; i < resolutions->maxlength; ++i) {
    if (Config::Get().show_all_resolutions)
      resolutions->data[i].m_RefreshRate = targetRefreshRate;

    auto ores = resolutions->data[i];
    if (Config::Get().show_all_resolutions || (ores.m_RefreshRate == targetRefreshRate || targetRefreshRate == 0)) {
      res.push_back(ores);
    }
  }

  res.erase(unique(res.begin(), res.end()), res.end());

  int i = 0;
  for (const auto& resultRes : res) {
    resolutions->data[i] = resultRes;
    ++i;
  }
  resolutions->maxlength = res.size();
#endif

  return resolutions;
}

/**
 * @brief Installs the resolution list cleanup hook.
 *
 * Hooks UnityEngine.Screen::get_resolutions via IL2CPP icall resolution.
 */
void InstallResolutionListFix()
{
  auto get_resolutions = il2cpp_resolve_icall_typed<ResolutionArray*()>("UnityEngine.Screen::get_resolutions()");
  if (!get_resolutions) {
    ErrorMsg::MissingMethod("UnityEngine.Screen", "get_resolutions");
  } else {
    SPUD_STATIC_DETOUR(get_resolutions, GetResolutions_Hook);
  }
}

// ─── Crash Fixes & Misc Hooks ────────────────────────────────────────────────

/**
 * @brief Hook: BuffService::ExtractBuffsOfType
 *
 * Null-guard for buff list extraction to prevent crashes.
 * Original method: filters a buff list by modifier type.
 * Our modification: checks each list element for null before passing to
 *   original; returns nullptr early if any entry is null (avoids a crash
 *   deep in the buff processing pipeline).
 */
IList* ExtractBuffsOfType_Hook(auto original, ClientModifierType modifier, IList* list)
{
  if (list) {
    for (int i = 0; i < list->Count; ++i) {
      auto item = list->Get(i);
      if (item == 0) {
        return nullptr;
      }
    }
  }
  return original(modifier, list);
}

/**
 * @brief Hook: ShopSceneManager::ShouldShowRevealSequence
 *
 * Optionally skips the chest-opening reveal animation.
 * Original method: decides whether to play the reveal sequence.
 * Our modification: if always_skip_reveal_sequence is set, returns false
 *   regardless of the original result.
 */
bool ShouldShowRevealHook(auto original, void* _this, bool ignore)
{
  if (Config::Get().always_skip_reveal_sequence) {
    return false;
  }

  return original(_this, ignore);
}

/**
 * @brief Hook: InterstitialViewController::AboutToShow
 *
 * Intercepts the first interstitial popup to auto-dismiss it.
 * Original method: displays the interstitial (typically a promo offer).
 * Our modification: if disable_first_popup is set and this is the first
 *   interstitial since launch, calls CloseWhenReady() instead of showing it.
 */
bool isFirstInterstitial = true;

void InterstitialViewController_AboutToShow(auto original, InterstitialViewController* _this)
{
  if (false /* TEMP: disable_first_popup effect disabled */ && Config::Get().disable_first_popup && isFirstInterstitial
      && _this != nullptr) {
    isFirstInterstitial = false;
    _this->CloseWhenReady();
  } else {
    original(_this);
  }
}

/**
 * @brief Installs crash-fix and QoL hooks.
 *
 * Hooks:
 *   - BuffService::ExtractBuffsOfType (null-guard crash fix)
 *   - ShopSceneManager::ShouldShowRevealSequence (skip reveal animation)
 *   - InterstitialViewController::AboutToShow (dismiss first popup)
 */
void InstallTempCrashFixes()
{
  auto BuffService_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
  if (!BuffService_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    auto ptr_extract_buffs_of_type = BuffService_helper.GetMethod("ExtractBuffsOfType");
    if (ptr_extract_buffs_of_type == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ExtractBuffsOfType");
    } else {
      SPUD_STATIC_DETOUR(ptr_extract_buffs_of_type, ExtractBuffsOfType_Hook);
    }
  }

  auto shop_scene_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager");
  if (!shop_scene_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("Shop", "ShopSceneManager");
  } else {
    auto reveal_show = shop_scene_manager.GetMethod("ShouldShowRevealSequence");
    if (reveal_show == nullptr) {
      ErrorMsg::MissingMethod("ShopSceneManager", "ShouldShowRevealSequence");
    } else {
      SPUD_STATIC_DETOUR(reveal_show, ShouldShowRevealHook);
    }
  }

  static auto interstitial_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Interstitial", "InterstitialViewController");
  if (!interstitial_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Interstitial", "InterstitialViewController");
  } else {
    auto interstitial_show = interstitial_controller.GetMethod("AboutToShow");
    if (interstitial_show == nullptr) {
      ErrorMsg::MissingMethod("InterstitialViewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(interstitial_show, InterstitialViewController_AboutToShow);
    }
  }
}
