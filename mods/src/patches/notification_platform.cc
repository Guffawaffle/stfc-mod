/**
 * @file notification_platform.cc
 * @brief Platform adapter for OS-native notification delivery.
 */
#include "patches/notification_platform.h"

#include "patches/notification_text.h"

#include <mutex>

#include <spdlog/spdlog.h>

#if _WIN32
#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#endif

namespace {
std::mutex s_delivery_override_mutex;
NotificationPlatformDeliveryOverride s_delivery_override = nullptr;
}

void notification_platform_set_delivery_override_for_testing(NotificationPlatformDeliveryOverride override_callback)
{
  std::lock_guard lock(s_delivery_override_mutex);
  s_delivery_override = override_callback;
}

void notification_platform_reset_delivery_override_for_testing()
{
  std::lock_guard lock(s_delivery_override_mutex);
  s_delivery_override = nullptr;
}

void notification_platform_init()
{
#if _WIN32
  try { winrt::init_apartment(); } catch (...) {}
#endif
}

void notification_platform_show(const char* title, const char* body)
{
  {
    std::lock_guard lock(s_delivery_override_mutex);
    if (s_delivery_override) {
      s_delivery_override(title, body);
      return;
    }
  }

#if _WIN32
  try {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    auto normalizedBody = notification_normalize_body(body);
    spdlog::debug("[NotifyQueue] show title='{}' body='{}'",
                  title ? notification_escape_text_for_log(title) : "",
                  notification_escape_text_for_log(normalizedBody));

    auto xml = ToastNotificationManager::GetTemplateContent(normalizedBody.empty() ? ToastTemplateType::ToastText01
                                                                                   : ToastTemplateType::ToastText02);
    auto nodes = xml.GetElementsByTagName(L"text");
    nodes.Item(0).InnerText(winrt::to_hstring(title));
    if (!normalizedBody.empty()) {
      nodes.Item(1).InnerText(winrt::to_hstring(normalizedBody));
    }

    auto notification = ToastNotification(xml);
    auto notifier     = ToastNotificationManager::CreateToastNotifier(L"Star Trek Fleet Command");
    notifier.Show(notification);
    spdlog::debug("[Notify] WinRT notification requested title='{}' body='{}'",
            title ? notification_escape_text_for_log(title) : "",
            notification_escape_text_for_log(normalizedBody));
  } catch (const winrt::hresult_error& e) {
    spdlog::warn("[Notify] WinRT notification failed: {}", winrt::to_string(e.message()));
  } catch (...) {
    spdlog::warn("[Notify] WinRT notification failed (unknown error)");
  }
#else
  (void)title;
  (void)body;
#endif
}
