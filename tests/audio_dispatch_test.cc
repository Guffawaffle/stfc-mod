#include "features/notification_audio.h"
#include "features/notification_audio_platform.h"
#include <stdexcept>

namespace
{
int  calls  = 0;
auto result = AudioPlaybackResult::Started;
void Check(bool value)
{
  if (!value)
    throw std::runtime_error("audio dispatch regression");
}
} // namespace
AudioPlaybackResult PlayNotificationAudioBytes(const uint8_t*, size_t)
{
  ++calls;
  return result;
}
int main()
{
  NotificationAudioCue a, b;
  a.data             = std::make_shared<const std::vector<uint8_t>>(32, 1);
  b.data             = std::make_shared<const std::vector<uint8_t>>(32, 2);
  a.duration_seconds = b.duration_seconds = 30;
  SetNotificationAudioCoalescing(AudioCoalescing::Same);
  PlayNotificationSound(a);
  PlayNotificationSound(a);
  Check(calls == 1);
  // A replacement that stops the old clip must release its suppression window.
  result = AudioPlaybackResult::Stopped;
  PlayNotificationSound(b);
  result = AudioPlaybackResult::Started;
  PlayNotificationSound(a);
  Check(calls == 3);
  // Failure before stopping the old clip must preserve that window.
  result = AudioPlaybackResult::Unchanged;
  PlayNotificationSound(b);
  PlayNotificationSound(a);
  Check(calls == 4);
  SetNotificationAudioCoalescing(AudioCoalescing::All);
  PlayNotificationSound(b);
  Check(calls == 4);
  SetNotificationAudioCoalescing(AudioCoalescing::None);
  result = AudioPlaybackResult::Started;
  PlayNotificationSound(a);
  PlayNotificationSound(a);
  Check(calls == 6);
  // The player borrows bytes asynchronously. Config replacement must not release its active buffer.
  std::weak_ptr<const std::vector<uint8_t>> active = a.data;
  a.data.reset();
  Check(!active.expired());
  PlayNotificationSound(NotificationSound::kPing);
  Check(calls == 7 && active.expired());
}
