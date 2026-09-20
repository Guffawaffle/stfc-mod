#include "config.h"
#include "errormsg.h"
#include "patches/notification_service.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/Toast.h>

#include <spud/detour.h>

namespace { bool s_toast_audio_available = false; }
bool ToastAudioAvailable() { return s_toast_audio_available; }

struct ToastObserver {
};

void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver *_this, Toast *toast)
{
  notification_handle_toast(toast);

  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast);
}

void ToastObserver_EnqueueOrCombineToast_Hook(auto original, ToastObserver *_this, Toast *toast, uintptr_t cmpAction)
{
  notification_handle_toast(toast);

  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast, cmpAction);
}

void InstallToastBannerHooks()
{
  notification_init();
  bool enqueue = false, combine = false;

  if (auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastObserver");
      !helper.isValidHelper()) {
    ErrorMsg::MissingHelper("HUD", "ToastObserver");
  } else {
    if (const auto ptr = helper.GetMethod("EnqueueToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueToast");
    } else {
      enqueue = SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueToast_Hook) != nullptr;
    }

    if (const auto ptr = helper.GetMethod("EnqueueOrCombineToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueOrCombineToast");
    } else {
      combine = SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueOrCombineToast_Hook) != nullptr;
    }
  }
  s_toast_audio_available = enqueue && combine;
}
