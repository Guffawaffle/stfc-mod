#include "patches/notification_audio.h"

#include "config.h"
#include "platform_config.h"

#include <spdlog/spdlog.h>

#if STFCMOD_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace
{
bool s_notification_audio_initialized = false;

const char* notification_audio_event_name(NotificationAudioEvent event)
{
  switch (event) {
    case NotificationAudioEvent::FleetArrivedInSystem:
      return "fleet_arrived_in_system";
    default:
      return "unknown";
  }
}

bool notification_audio_enabled_for_event(NotificationAudioEvent event)
{
  const auto& notifications = Config::Get().notifications;
  if (!notifications.audio_enabled) {
    return false;
  }

  switch (event) {
    case NotificationAudioEvent::FleetArrivedInSystem:
      return notifications.audio_fleet_arrived_in_system;
    default:
      return false;
  }
}
} // namespace

void notification_audio_init()
{
  if (s_notification_audio_initialized) {
    return;
  }

#if _WIN32
  spdlog::debug("[NotifyAudio] Windows notification audio initialized");
#elif STFCMOD_PLATFORM_MACOS
  if (Config::Get().notifications.audio_enabled) {
    spdlog::warn(
        "[NotifyAudio] macOS does not support notification audio yet; [notifications].notifications_audio_enabled will be ignored");
  } else {
    spdlog::debug("[NotifyAudio] Notification audio: macOS does not support this feature yet (no-op)");
  }
#else
  spdlog::debug("[NotifyAudio] Notification audio: platform not supported (no-op)");
#endif
  s_notification_audio_initialized = true;
}

void notification_audio_play(NotificationAudioEvent event)
{
  if (!notification_audio_enabled_for_event(event)) {
    return;
  }

#if STFCMOD_PLATFORM_WINDOWS
  if (!MessageBeep(MB_ICONINFORMATION)) {
    spdlog::warn("[NotifyAudio] Failed to play notification sound event={}", notification_audio_event_name(event));
    return;
  }

  spdlog::debug("[NotifyAudio] Played notification sound event={}", notification_audio_event_name(event));
#else
  spdlog::debug("[NotifyAudio] Suppressed notification sound event={} reason=unsupported-platform",
                notification_audio_event_name(event));
#endif
}