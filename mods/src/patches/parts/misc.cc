/**
 * @file misc.cc
 * @brief Miscellaneous quality-of-life patches and crash fixes.
 *
 * A collection of independent patches that don't warrant their own file:
 *   - Donation slider extension (raise the 50-item cap)
 *   - Bundle cooldown bypass (click through to info view while on cooldown)
 *   - Resolution list cleanup (deduplicate and normalize refresh rates)
 *   - Buff extraction null-guard (prevent crash on null list entries)
 *   - Shop reveal sequence skip
 *   - First interstitial popup dismissal
 *   - Action queue logging (diagnostic, currently disabled)
 */
#include "config.h"
#include "errormsg.h"

#include <prime/BundleDataWidget.h>
#include <prime/ClientModifierType.h>
#include <prime/Hub.h>
#include <prime/IList.h>
#include <prime/InventoryForPopup.h>
#include <prime/ShopSummaryDirector.h>

#include <il2cpp/il2cpp_helper.h>

#include <spud/detour.h>

#if _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <prime/ActionQueueManager.h>
#include <prime/InterstitialViewController.h>

// ─── Donation Slider Extension ───────────────────────────────────────────────

/**
 * @brief Hook: InventoryForPopup::set_MaxItemsToUse
 *
 * Intercepts the donation slider cap to allow larger donations.
 * Original method: sets the maximum slider value (hard-coded to 50 for donations).
 * Our modification: when extend_donation_slider is enabled and the caller is a
 *   donation popup with cap == 50, replaces it with the user's configured max
 *   (or returns -1 for unlimited if max <= 0).
 */
int64_t InventoryForPopup_set_MaxItemsToUse(auto original, InventoryForPopup* a1, int64_t a2)
{
  if (a1->IsDonationUse && a2 == 50 && Config::Get().extend_donation_slider) {
    const auto max = Config::Get().extend_donation_max;
    if (max > 0) {
      a2 = max;
    } else {
      return -1;
    }
  }

  int64_t standard = original(a1, a2);
  return standard;
}

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
 * @brief Installs donation slider and bundle cooldown patches.
 *
 * Hooks InventoryForPopup::set_MaxItemsToUse (Windows only) and
 * BundleDataWidget::OnActionButtonPressedCallback.
 */
void InstallMiscPatches()
{
#if _WIN32
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
  {
    return this->m_Height == other.m_Height && this->m_Width == other.m_Width;
  }
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
  // Modify
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

void TriggerOpenSectionChange_Hook(auto original, void* _this, void* data, bool ignoreRevealSequence,
                                   int numChests, bool* isFlyOut)
{
  if (Config::Get().always_skip_reveal_sequence) {
    ignoreRevealSequence = true;
  }

  original(_this, data, ignoreRevealSequence, numChests, isFlyOut);
}

// ─── IL2CPP Property Wrappers ────────────────────────────────────────────────

struct ShopCategory {
public:
  __declspec(property(get = __get__flagValue)) int Value;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Prime.Shop", "ShopCategory");
    return class_helper;
  }

public:
  int __get__flagValue()
  {
    static auto field = get_class_helper().GetProperty("Value");
    return *field.GetUnboxedSelf<int>(this);
  }
};

struct CurrencyType {
public:
  __declspec(property(get = __get__flagValue)) int Value;
  //

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "CurrencyType");
    return class_helper;
  }

public:
  int __get__flagValue()
  {
    static auto field = get_class_helper().GetProperty("Value");
    return *field.GetUnboxedSelf<int>(this);
  }
};

struct BundleGroupConfig {
public:
  __declspec(property(get = __get__category)) int _category;
  __declspec(property(get = __get__currency)) int _currency;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleGroupConfig");
    return class_helper;
  }

public:
  int __get__category()
  {
    static auto field = get_class_helper().GetField("_category");
    return *(int*)((ptrdiff_t)this + field.offset());
  }

  int __get__currency()
  {
    static auto field = get_class_helper().GetField("_currency");
    return *(int*)((ptrdiff_t)this + field.offset());
  }
};

class ShopSectionContext
{
public:
  __declspec(property(get = __get__bundleConfig)) BundleGroupConfig* _bundleConfig;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext");
    return class_helper;
  }

public:
  BundleGroupConfig* __get__bundleConfig()
  {
    static auto field = get_class_helper().GetProperty("BundleGroup");
    return field.GetRaw<BundleGroupConfig>(this);
  }
};

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
  if (Config::Get().disable_first_popup && isFirstInterstitial && _this != nullptr) {
    isFirstInterstitial = false;
    _this->CloseWhenReady();
  } else {
    original(_this);
  }
}

/// Diagnostic hook for action queue logging (currently commented out in install).
void ActionQueueManager_AddActionToQueue(auto original, ActionQueueManager* _this, long fleet_id)
{
  spdlog::warn("ActionQueueManager_AddActionToQueue({})", fleet_id);
  original(_this, fleet_id);
}

//   const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(sectionID);

/**
 * @brief Installs crash-fix and QoL hooks.
 *
 * Hooks:
 *   - BuffService::ExtractBuffsOfType (null-guard crash fix)
 *   - ShopSceneManager::ShouldShowRevealSequence (skip reveal animation)
 *   - ShopSceneManager::TriggerOpenSectionChange (force reveal skip flag)
 *   - InterstitialViewController::AboutToShow (dismiss first popup)
 *   - ActionQueueManager::AddActionToQueue (diagnostic, currently disabled)
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

    auto trigger_open_section_change = shop_scene_manager.GetMethod("TriggerOpenSectionChange");
    if (trigger_open_section_change == nullptr) {
      ErrorMsg::MissingMethod("ShopSceneManager", "TriggerOpenSectionChange");
    } else {
      SPUD_STATIC_DETOUR(trigger_open_section_change, TriggerOpenSectionChange_Hook);
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

  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueMaanger");
  } else {
    auto addtoqueue_method = actionqueue_manager.GetMethod("AddActionToQueue");
    if (addtoqueue_method == nullptr) {
      ErrorMsg::MissingMethod("ActionQueueManager", "AddActionToQueue");
    } else {
      // SPUD_STATIC_DETOUR(addtoqueue_method, ActionQueueManager_AddActionToQueue);
    }
  }
}
