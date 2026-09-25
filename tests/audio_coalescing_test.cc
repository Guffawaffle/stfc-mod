#include "patches/audio_coalescing.h"
#include <stdexcept>

void Check(bool condition)
{
  if (!condition) throw std::runtime_error("audio coalescing regression");
}

int main()
{
  using namespace std::chrono_literals;
  const auto start = AudioCoalescingWindow::Clock::time_point{} + 1s;
  AudioCoalescingWindow window;
  Check(!window.Suppress(AudioCoalescing::All, true, start));
  window.Started(start, 10.0);
  for (auto offset : {1s, 5s, 9s}) {
    Check(!window.Suppress(AudioCoalescing::None, true, start + offset));
    Check(window.Suppress(AudioCoalescing::Same, true, start + offset));
    Check(!window.Suppress(AudioCoalescing::Same, false, start + offset));
    Check(window.Suppress(AudioCoalescing::All, false, start + offset));
  }
  // Absorbed events do not prolong playback or cause a trailing repeat.
  Check(!window.Suppress(AudioCoalescing::All, true, start + 10s));
  // A different sound replaces the old one with its own shorter window.
  window.Started(start + 5s, 1.0);
  Check(window.Suppress(AudioCoalescing::All, false, start + 5500ms));
  Check(!window.Suppress(AudioCoalescing::All, true, start + 6s));
  // Changing mode while a clip is active uses the existing window.
  Check(!window.Suppress(AudioCoalescing::None, true, start + 5500ms));
  Check(window.Suppress(AudioCoalescing::Same, true, start + 5500ms));
  for (auto mode : {AudioCoalescing::None, AudioCoalescing::Same, AudioCoalescing::All})
    Check(audio_coalescing_from_name(audio_coalescing_name(mode)) == mode);
  Check(!audio_coalescing_from_name("invalid"));
}
