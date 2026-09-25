#include "patches/notification_audio.h"
#include "patches/notification_audio_platform.h"
#include "patches/parts/runtime_config_keys.h"
#include <stdexcept>

namespace {
int calls = 0;
auto result = AudioPlaybackResult::Started;
void Check(bool value) { if (!value) throw std::runtime_error("audio dispatch regression"); }
}
AudioPlaybackResult notification_audio_platform_play(const uint8_t*, size_t)
{
  ++calls;
  return result;
}
int main()
{
  bool registered = false;
  for (const auto& [section, key] : config_edit::persisted_settings)
    registered |= std::string_view(section) == "audio" && std::string_view(key) == "coalescing";
  Check(registered);
  NotificationAudioCue a, b;
  a.data = std::make_shared<const std::vector<uint8_t>>(32, 1);
  b.data = std::make_shared<const std::vector<uint8_t>>(32, 2);
  a.duration_seconds = b.duration_seconds = 30;
  notification_audio_set_coalescing(AudioCoalescing::Same);
  notification_audio_play(a);
  notification_audio_play(a);
  Check(calls == 1);
  // A replacement that stops the old clip must release its suppression window.
  result = AudioPlaybackResult::Stopped;
  notification_audio_play(b);
  result = AudioPlaybackResult::Started;
  notification_audio_play(a);
  Check(calls == 3);
  // Failure before stopping the old clip must preserve that window.
  result = AudioPlaybackResult::Unchanged;
  notification_audio_play(b);
  notification_audio_play(a);
  Check(calls == 4);
  notification_audio_set_coalescing(AudioCoalescing::All);
  notification_audio_play(b);
  Check(calls == 4);
  notification_audio_set_coalescing(AudioCoalescing::None);
  result = AudioPlaybackResult::Started;
  notification_audio_play(a);
  notification_audio_play(a);
  Check(calls == 6);
}
