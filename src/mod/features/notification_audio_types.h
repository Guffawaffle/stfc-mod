#pragma once

// Owned, game-independent notification cue data. Parsing a source does not open or decode a file.

#include "audio_coalescing.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class NotificationSound : uint8_t {
  kNone,
  kDefault,
  kInfo,
  kSuccess,
  kWarning,
  kAlarm,
  kArrival,
  kSoft,
  kPing,
  kRepair,
  kCount,
};

inline constexpr std::array<std::string_view, 10> kNotificationSoundNames{
    "none", "default", "info", "success", "warning", "alarm", "arrival", "soft", "ping", "repair"};
static_assert(kNotificationSoundNames.size() == static_cast<size_t>(NotificationSound::kCount));

inline std::string_view NotificationSoundName(NotificationSound sound)
{
  const auto index = static_cast<size_t>(sound);
  return index < kNotificationSoundNames.size() ? kNotificationSoundNames[index] : "none";
}

inline std::optional<NotificationSound> NotificationSoundFromName(std::string_view value)
{
  while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ')
    value.remove_prefix(1);
  while (!value.empty() && static_cast<unsigned char>(value.back()) <= ' ')
    value.remove_suffix(1);
  std::string normalized(value);
  for (auto& c : normalized)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  for (size_t i = 0; i < kNotificationSoundNames.size(); ++i)
    if (normalized == kNotificationSoundNames[i])
      return static_cast<NotificationSound>(i);
  constexpr std::pair<std::string_view, NotificationSound> aliases[]{
      {"off", NotificationSound::kNone},        {"silent", NotificationSound::kNone},
      {"victory", NotificationSound::kSuccess}, {"warn", NotificationSound::kWarning},
      {"attack", NotificationSound::kAlarm},    {"arrive", NotificationSound::kArrival},
      {"quiet", NotificationSound::kSoft},      {"repaired", NotificationSound::kRepair},
  };
  for (const auto& [name, sound] : aliases)
    if (normalized == name)
      return sound;
  return std::nullopt;
}

struct NotificationAudioCue {
  NotificationSound                           sound  = NotificationSound::kNone;
  std::string                                 source = "none";
  std::shared_ptr<const std::vector<uint8_t>> data;
  double                                      duration_seconds = 0;

  NotificationAudioCue() = default;
  NotificationAudioCue(NotificationSound builtin)
      : sound(builtin)
      , source(NotificationSoundName(builtin))
  {
  }
  bool enabled() const
  { return sound != NotificationSound::kNone || (data && !data->empty()); }
};

inline NotificationAudioCue NotificationAudioCueFromSource(std::string_view source)
{
  if (const auto builtin = NotificationSoundFromName(source))
    return NotificationAudioCue(*builtin);
  NotificationAudioCue cue;
  cue.source = source;
  return cue;
}
