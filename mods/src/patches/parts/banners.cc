#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <queue>

#include <prime/Toast.h>
#include <prime/ToastObserver.h>
#include <prime/ToastState.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <wintoastlib.h>

using namespace WinToastLib;

std::mutex              toast_queue_mutex;
std::condition_variable toast_queue_condition;
std::queue<Toast *>     toast_queue_data;

void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver *_this, Toast *toast)
{
  if (std::find(Config::Get().notify_banner_types.begin(), Config::Get().notify_banner_types.end(), toast->State)
      != Config::Get().notify_banner_types.end()) {
    std::lock_guard lk(toast_queue_mutex);
    toast_queue_data.push(toast);
    toast_queue_condition.notify_all();
  }

  if (std::find(Config::Get().disabled_banner_types.begin(), Config::Get().disabled_banner_types.end(), toast->State)
      != Config::Get().disabled_banner_types.end()) {
    return;
  }
  original(_this, toast);
}

void ToastObserver_EnqueueOrCombineToast_Hook(auto original, ToastObserver *_this, Toast *toast, uintptr_t cmpAction)
{
  if (std::find(Config::Get().notify_banner_types.begin(), Config::Get().notify_banner_types.end(), toast->State)
      != Config::Get().notify_banner_types.end()) {
    std::lock_guard lk(toast_queue_mutex);
    toast_queue_data.push(toast);
  }

  if (std::find(Config::Get().disabled_banner_types.begin(), Config::Get().disabled_banner_types.end(), toast->State)
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast, cmpAction);
}

void show_toasts()
{
#if _WIN32
  WinToast::instance()->setAppName(L"STFC.Community.Mod");
  const auto aumi = WinToast::configureAUMI(L"STFC", L"Community", L"Mod", L"20161006");
  WinToast::instance()->setAppUserModelId(aumi);

  WinToast::WinToastError error     = WinToast::WinToastError::NoError;
  const auto              succeeded = WinToast::instance()->initialize(&error);

  if (!succeeded) {
    std::stringstream message;
    message << "Failed to initialize WinToast due to " << (int)error;
    spdlog::error(message.str());
  } else if (!WinToast::isCompatible()) {
    ErrorMsg::MissingMethod("WinToast", "isCompatible");
  } else {

    for (;;) {
      {
        std::unique_lock lk(toast_queue_mutex);
        toast_queue_condition.wait(lk, []() { return !toast_queue_data.empty(); });
      }

      // Ensure we have some strings for population
      std::stringstream toastTypes;
      std::stringstream toastDescription;

      // Ensure ToastMode is properly scoped or defined
      ToastMode highestMode = ToastMode::Normal;
      bool      hasType     = false;

      // Lock this section to ensure we have a clear pass on the
      // toast queue
      {
        std::lock_guard lk(toast_queue_mutex);

        while (!toast_queue_data.empty()) {
          auto toast = toast_queue_data.front();
          toast_queue_data.pop();

          spdlog::info("Found {} toast{}", toast->ModeName, toast->Name);

          if (toast->Mode > highestMode) {
            highestMode = toast->Mode;
          }

          if (hasType) {
            toastTypes << ", ";
          }

          if (toast->Mode != ToastMode::Normal) {
            toastTypes << "[" << toast->ModeName << "] ";
          }
          toastTypes << toast->Name;

          if (hasType) {
            toastDescription << " ";
          }
          toastDescription << toast->Description;
        }
      }

      WinToastTemplate templ = WinToastTemplate::WinToastTemplate(WinToastTemplate::Text02);
      templ.setTextField(std::wstring(toastTypes.str().begin(), toastTypes.str().end()), WinToastTemplate::FirstLine);
      templ.setTextField(std::wstring(toastDescription.str().begin(), toastDescription.str().end()),
                         WinToastTemplate::SecondLine);
      // templ.setHeroImagePath(L"C:/example.png");
      templ.setAttributionText(L"Via STFC Community Mod");
      auto handler = new WinToastHandler();

      const auto toast_id = WinToast::instance()->showToast(templ, handler, &error);
      if (toast_id < 0) {
        // We failed to show the toast, log the error
        std::stringstream message;
        message << "Failed to pop toast " << toastTypes.str() << " due to " << (int)error;
        spdlog::error(message.str());
      }
    }
  }
#endif
}

void InstallToastBannerHooks()
{
  auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastObserver");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("HUD", "ToastObserver");
  } else {
    auto ptr = helper.GetMethod("EnqueueToast");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueTosat");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueToast_Hook);
    }

    ptr = helper.GetMethod("EnqueueOrCombineToast");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueOrCombineToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueOrCombineToast_Hook);
    }
  }

#if _WIN32
  std::thread(show_toasts).detach();
#endif
}
