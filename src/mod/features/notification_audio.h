#pragma once

// Prepared notification cues, built-in tones, and local-file audio. Playback performs no file or game access.

#include "notification_audio_types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

// Mono 16-bit PCM WAV data for `sound`, generated on first use and kept for the process: playback is asynchronous
// and reads the buffer after the call returns. None has no data.
std::span<const uint8_t> NotificationSoundBytes(NotificationSound sound);
void                     PlayNotificationSound(NotificationSound sound);
void                     PlayNotificationSound(const NotificationAudioCue& cue);

// Configuration activation prepares file sources; Parse only retains their descriptors.
NotificationAudioCue LoadNotificationAudio(std::string_view source, const std::filesystem::path& directory);
AudioCoalescing      NotificationAudioCoalescing();
void                 SetNotificationAudioCoalescing(AudioCoalescing mode);
