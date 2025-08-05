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
std::queue<Toast>       toast_queue_data;

void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver *_this, Toast *toast)
{
  if (std::find(Config::Get().notify_banner_types.begin(), Config::Get().notify_banner_types.end(), toast->State)
      != Config::Get().notify_banner_types.end()) {
    std::lock_guard lk(toast_queue_mutex);
    toast_queue_data.push(*toast);
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
    toast_queue_data.push(*toast);
  }

  if (std::find(Config::Get().disabled_banner_types.begin(), Config::Get().disabled_banner_types.end(), toast->State)
      != Config::Get().disabled_banner_types.end()) {
    return;
  }
  original(_this, toast, cmpAction);
}

void ProcessNotifications()
{
#if _WIN32
  winrt::init_apartment();

  for (;;) {
    {
      std::unique_lock lk(toast_queue_mutex);
      toast_queue_condition.wait(lk, []() { return !toast_queue_data.empty(); });
    }

    {
      std::lock_guard lk(toast_queue_mutex);

      // Ensure we have some strings for population
      std::stringstream toastTypes;
      std::stringstream toastDescription;

      // Ensure ToastMode is properly scoped or defined
      ToastMode highestMode = ToastMode::Normal;
      bool      hasType     = false;

      while (!toast_queue_data.empty()) {
        auto toast = ([&] {
          std::lock_guard lk(toast_queue_mutex);
          auto            data = toast_queue_data.front();
          toast_queue_data.pop();
          return data;
        })();

        toast_queue_data.pop();
        spdlog::info("Found {} toast", toast.Name);

        if (toast.Mode > highestMode) {
          highestMode = toast.Mode;
        }

        if (hasType) {
          toastTypes << ", ";
        }

        if (toast.Mode != ToastMode::Normal) {
          toastTypes << "[" << toast.ModeName << "] ";
        }
        toastTypes << toast.Name;

        if (hasType) {
          toastDescription << " ";
        }
        toastDescription << toast.Description;
      }

      WinToastTemplate templ =
          WinToastTemplate::WinToastTemplate(WinToastTemplate::Text02);
      templ.setTextField(std::wstring(toastTypes.str().begin(), toastTypes.str().end()),
                         WinToastTemplate::FirstLine);
      templ.setTextField(std::wstring(toastDescription.str().begin(), toastDescription.str().end()),
                         WinToastTemplate::SecondLine);
      //templ.setHeroImagePath(L"C:/example.png");
      templ.setAttributionText(L"Via STFC Community Mod");

      WinToast::WinToastError error = WinToastLib::WinToast::NoError;
      const auto toast_id = WinToast::instance()->showToast(templ, nullptr, &error);
      if (toast_id < 0) {
        // We failed to show the toast, log the error
      }
    }
  }
  winrt::uninit_apartment();
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
  WinToastLib::WinToast::instance()->setAppName(L"WinToastExample");
  const auto aumi = WinToastLib::WinToast::configureAUMI(L"STFC", L"Community", L"Mod", L"20161006");
  WinToastLib::WinToast::instance()->setAppUserModelId(aumi);	

  if (!WinToastLib::WinToast::isCompatible()) {
    ErrorMsg::MissingMethod("WinToast", "isCompatible");
  } else {
  
  }
  #endif
}
