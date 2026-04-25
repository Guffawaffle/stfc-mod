/**
 * @file disable_banners.cc
 * @brief Filters in-game toast/banner notifications by type.
 *
 * The game displays pop-up "toast" banners for various events (rewards, offers,
 * alliance activity, etc.). This patch intercepts the toast queue and drops any
 * banner whose state matches the user's configured disabled_banner_types list.
 * Toasts are also forwarded to the notification service for optional logging.
 */
#include "config.h"
#include "errormsg.h"
#include "patches/notification_service.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/Toast.h>

#include <spud/detour.h>

namespace {
constexpr bool kEnableToastObserverHooks = true;
}

struct ToastObserver {
};

/**
 * @brief Hook: ToastObserver::EnqueueToast
 *
 * Intercepts single toast enqueue to filter unwanted banner types.
 * Original method: adds a toast to the display queue unconditionally.
 * Our modification: forwards the toast to the notification service, then
 *   drops it silently if its state is in the user's disabled list.
 */
void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver *_this, Toast *toast)
{
  notification_handle_toast(toast);

  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast);
}

/**
 * @brief Hook: ToastObserver::EnqueueOrCombineToast
 *
 * Intercepts the combine-or-enqueue path with the same filtering logic.
 * Original method: merges a toast with an existing one or enqueues it.
 * Our modification: same filter as EnqueueToast — drop if banner type disabled.
 */
void ToastObserver_EnqueueOrCombineToast_Hook(auto original, ToastObserver *_this, Toast *toast, uintptr_t cmpAction)
{
  notification_handle_toast(toast);

  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast, cmpAction);
}

/**
 * @brief Hook: ToastObserver::AddFirstOrCombineToast
 *
 * Some HUD notification flows appear to insert toasts through the add-first
 * path rather than the plain enqueue helpers. Forward them through the same
 * notification/filter pipeline so state-backed desktop notifications stay
 * consistent across both queue paths.
 */
void ToastObserver_AddFirstOrCombineToast_Hook(auto original, ToastObserver *_this, Toast *toast, uintptr_t cmpAction)
{
  notification_handle_toast(toast);

  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast, cmpAction);
}

/**
 * @brief Installs toast/banner filtering hooks.
 *
 * Initializes the notification service and hooks the ToastObserver queue
 * paths so banners can be selectively suppressed regardless of whether the
 * HUD uses enqueue or add-first insertion.
 */
void InstallToastBannerHooks()
{
  notification_init();

  if (!kEnableToastObserverHooks) {
    return;
  }

  if (auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastObserver");
      !helper.isValidHelper()) {
    ErrorMsg::MissingHelper("HUD", "ToastObserver");
  } else {
    if (const auto ptr = helper.GetMethod("EnqueueToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueToast_Hook);
    }

    if (const auto ptr = helper.GetMethod("EnqueueOrCombineToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueOrCombineToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueOrCombineToast_Hook);
    }

    if (const auto ptr = helper.GetMethod("AddFirstOrCombineToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "AddFirstOrCombineToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_AddFirstOrCombineToast_Hook);
    }
  }
}
