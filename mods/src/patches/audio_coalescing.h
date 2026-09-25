#pragma once

#include <chrono>
#include <optional>
#include <string_view>

enum class AudioCoalescing { None, Same, All };

inline std::string_view audio_coalescing_name(AudioCoalescing mode)
{
  switch (mode) {
  case AudioCoalescing::None: return "none";
  case AudioCoalescing::All: return "all";
  default: return "same";
  }
}

inline std::optional<AudioCoalescing> audio_coalescing_from_name(std::string_view name)
{
  if (name == "none") return AudioCoalescing::None;
  if (name == "same") return AudioCoalescing::Same;
  if (name == "all") return AudioCoalescing::All;
  return std::nullopt;
}

// The native players replace the current clip. Suppressed requests never extend
// its window; only a successful playback starts a new window.
class AudioCoalescingWindow {
public:
  using Clock = std::chrono::steady_clock;
  bool Suppress(AudioCoalescing mode, bool same, Clock::time_point now) const
  {
    return now < until_ && (mode == AudioCoalescing::All || (mode == AudioCoalescing::Same && same));
  }
  void Started(Clock::time_point now, double seconds)
  {
    until_ = now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
  }
  void Stopped() { until_ = {}; }
private:
  Clock::time_point until_{};
};
