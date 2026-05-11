/**
 * @file notification_audio.h
 * @brief Optional in-game audible cues for selected notification events.
 */
#pragma once

#include "patches/notification_policy.h"

#include <string_view>

void notification_audio_init();
void notification_audio_shutdown();
void notification_audio_play(NotificationSound sound, std::string_view event_name);
