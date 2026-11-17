#pragma once

#include <string_view>

// Desktop delivery is independent of native banner visibility and custom audio.
void EmitDesktopNotification(std::string_view title, std::string_view body);
void InitializeDesktopNotifications();

// Suppresses only generic desktop forwarding while the native depletion handler emits its banner.
class ScopedToastNotificationSuppression
{
public:
  ScopedToastNotificationSuppression();
  ~ScopedToastNotificationSuppression();
  ScopedToastNotificationSuppression(const ScopedToastNotificationSuppression&)            = delete;
  ScopedToastNotificationSuppression& operator=(const ScopedToastNotificationSuppression&) = delete;
};

#if __APPLE__
void NotificationDesktopMacInit();
void NotificationDesktopMacEmit(std::string_view title, std::string_view body);
#endif
