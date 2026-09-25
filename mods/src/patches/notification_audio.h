#pragma once

#include "audio_coalescing.h"

#include <cstdint>
#include <optional>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

enum class NotificationSound : uint8_t {
  None = 0,
  Default,
  Info,
  Success,
  Warning,
  Alarm,
  Arrival,
  Soft,
  Ping,
  Repair,
  Count,
};

[[nodiscard]] std::string_view                 notification_sound_name(NotificationSound sound);
[[nodiscard]] std::optional<NotificationSound> notification_sound_from_name(std::string_view name);
void                                           notification_audio_play(NotificationSound sound);
AudioCoalescing notification_audio_coalescing();
void notification_audio_set_coalescing(AudioCoalescing mode);

// A prepared cue keeps loading/decoding separate from alert delivery. Other
// sources can be added later without putting network or disk IO in playback.
struct NotificationAudioCue {
  NotificationSound sound = NotificationSound::None;
  std::string source = "none";
  std::shared_ptr<const std::vector<uint8_t>> data;
  double duration_seconds = 0;

  NotificationAudioCue() = default;
  NotificationAudioCue(NotificationSound builtin) : sound(builtin), source(notification_sound_name(builtin)) {}
  bool enabled() const { return sound != NotificationSound::None || (data && !data->empty()); }
};

NotificationAudioCue notification_audio_load(std::string_view value, const std::filesystem::path& directory);
void notification_audio_play(const NotificationAudioCue& cue);
