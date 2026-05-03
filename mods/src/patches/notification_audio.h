/**
 * @file notification_audio.h
 * @brief Optional in-game audible cues for selected notification events.
 */
#pragma once

enum class NotificationAudioEvent {
  FleetArrivedInSystem,
};

void notification_audio_init();
void notification_audio_play(NotificationAudioEvent event);