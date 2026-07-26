#include "patches/notification_audio.h"

#include "config.h"
#include "platform_config.h"
#include "patches/async_work_queue.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if STFCMOD_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#endif

namespace
{
bool s_notification_audio_initialized = false;

#if STFCMOD_PLATFORM_WINDOWS
struct ToneSegment {
  double frequency_hz = 0.0;
  int    duration_ms  = 0;
};

constexpr int    kSampleRate = 44100;
constexpr double kTwoPi      = 6.28318530717958647692;
constexpr double kAmplitude  = 0.26;

constexpr std::array<ToneSegment, 4> kDefaultPattern{{{740.0, 70}, {0.0, 22}, {880.0, 85}, {0.0, 18}}};
constexpr std::array<ToneSegment, 3> kInfoPattern{{{659.0, 80}, {0.0, 24}, {880.0, 110}}};
constexpr std::array<ToneSegment, 5> kSuccessPattern{{{587.0, 70}, {0.0, 18}, {740.0, 70}, {0.0, 18}, {988.0, 120}}};
constexpr std::array<ToneSegment, 5> kWarningPattern{{{622.0, 90}, {0.0, 36}, {466.0, 110}, {0.0, 28}, {466.0, 90}}};
constexpr std::array<ToneSegment, 5> kAlarmPattern{{{880.0, 90}, {0.0, 42}, {880.0, 90}, {0.0, 42}, {698.0, 160}}};
constexpr std::array<ToneSegment, 5> kArrivalPattern{{{523.0, 65}, {0.0, 18}, {659.0, 70}, {0.0, 18}, {1046.0, 125}}};
constexpr std::array<ToneSegment, 3> kSoftPattern{{{523.0, 90}, {0.0, 26}, {659.0, 110}}};
constexpr std::array<ToneSegment, 1> kPingPattern{{{1046.0, 95}}};
constexpr std::array<ToneSegment, 5> kRepairPattern{{{440.0, 70}, {0.0, 18}, {554.0, 70}, {0.0, 18}, {740.0, 140}}};

std::once_flag s_sound_buffers_once;
std::array<std::vector<uint8_t>, static_cast<size_t>(NotificationSound::Count)> s_sound_buffers;

struct AudioPlaybackRequest {
  NotificationSound sound = NotificationSound::None;
  std::string       event_name;
};

constexpr size_t                     kAudioQueueMaxDepth = 64;
AsyncWorkQueue<AudioPlaybackRequest> s_audio_queue(kAudioQueueMaxDepth);
std::once_flag                       s_audio_worker_once;
std::thread                          s_audio_worker_thread;
constexpr auto                       kAudioJoinWarnThreshold = std::chrono::seconds(2);

std::span<const ToneSegment> sound_pattern(NotificationSound sound)
{
  switch (sound) {
    case NotificationSound::Info: return kInfoPattern;
    case NotificationSound::Success: return kSuccessPattern;
    case NotificationSound::Warning: return kWarningPattern;
    case NotificationSound::Alarm: return kAlarmPattern;
    case NotificationSound::Arrival: return kArrivalPattern;
    case NotificationSound::Soft: return kSoftPattern;
    case NotificationSound::Ping: return kPingPattern;
    case NotificationSound::Repair: return kRepairPattern;
    case NotificationSound::Default: return kDefaultPattern;
    default: return {};
  }
}

void append_u16(std::vector<uint8_t>& buffer, uint16_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void append_u32(std::vector<uint8_t>& buffer, uint32_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void append_ascii(std::vector<uint8_t>& buffer, std::string_view value)
{ buffer.insert(buffer.end(), value.begin(), value.end()); }

std::vector<uint8_t> build_wav(std::span<const ToneSegment> pattern)
{
  uint32_t sample_count = 0;
  for (const auto& segment : pattern) {
    sample_count += static_cast<uint32_t>((static_cast<int64_t>(segment.duration_ms) * kSampleRate) / 1000);
  }

  constexpr uint16_t channels        = 1;
  constexpr uint16_t bits_per_sample = 16;
  const uint32_t     data_bytes      = sample_count * channels * (bits_per_sample / 8);

  std::vector<uint8_t> buffer;
  buffer.reserve(44 + data_bytes);
  append_ascii(buffer, "RIFF");
  append_u32(buffer, 36 + data_bytes);
  append_ascii(buffer, "WAVE");
  append_ascii(buffer, "fmt ");
  append_u32(buffer, 16);
  append_u16(buffer, 1);
  append_u16(buffer, channels);
  append_u32(buffer, kSampleRate);
  append_u32(buffer, kSampleRate * channels * (bits_per_sample / 8));
  append_u16(buffer, channels * (bits_per_sample / 8));
  append_u16(buffer, bits_per_sample);
  append_ascii(buffer, "data");
  append_u32(buffer, data_bytes);

  double phase = 0.0;
  for (const auto& segment : pattern) {
    const auto segment_samples = static_cast<int>((static_cast<int64_t>(segment.duration_ms) * kSampleRate) / 1000);
    const auto step            = segment.frequency_hz > 0.0 ? kTwoPi * segment.frequency_hz / kSampleRate : 0.0;
    for (int index = 0; index < segment_samples; ++index) {
      const auto fade_in  = std::min(1.0, static_cast<double>(index) / 80.0);
      const auto fade_out = std::min(1.0, static_cast<double>(segment_samples - index) / 120.0);
      const auto sample   = segment.frequency_hz > 0.0 ? std::sin(phase) * kAmplitude * fade_in * fade_out : 0.0;
      const auto pcm      = static_cast<int16_t>(sample * 32767.0);
      append_u16(buffer, static_cast<uint16_t>(pcm));
      phase += step;
      if (phase > kTwoPi) {
        phase -= kTwoPi;
      }
    }
  }

  return buffer;
}

void initialize_sound_buffers()
{
  for (size_t index = 0; index < static_cast<size_t>(NotificationSound::Count); ++index) {
    const auto sound   = static_cast<NotificationSound>(index);
    const auto pattern = sound_pattern(sound);
    if (!pattern.empty()) {
      s_sound_buffers[index] = build_wav(pattern);
    }
  }
}

bool sound_buffer_available(NotificationSound sound)
{
  const auto index = static_cast<size_t>(sound);
  return index < s_sound_buffers.size() && !s_sound_buffers[index].empty();
}

void play_sound_buffer(const AudioPlaybackRequest& request)
{
  const auto  index  = static_cast<size_t>(request.sound);
  const auto& buffer = s_sound_buffers[index];
  if (!PlaySoundW(reinterpret_cast<LPCWSTR>(buffer.data()), nullptr, SND_MEMORY | SND_NODEFAULT)) {
    spdlog::warn("[NotifyAudio] Failed to play notification sound event={} sound={}; falling back to MessageBeep",
                 request.event_name, notification_sound_name(request.sound));
    MessageBeep(MB_ICONINFORMATION);
    return;
  }

  spdlog::debug("[NotifyAudio] Played notification sound event={} sound={}", request.event_name,
                notification_sound_name(request.sound));
}

void notification_audio_worker_main()
{
  s_audio_queue.set_worker_active(true);
  spdlog::debug("[NotifyAudio] worker started");

  AudioPlaybackRequest request;
  while (s_audio_queue.wait_pop(request)) {
    try {
      if (sound_buffer_available(request.sound)) {
        play_sound_buffer(request);
      }
    } catch (const std::exception& exception) {
      s_audio_queue.record_worker_error();
      spdlog::error("[NotifyAudio] worker error: {}", exception.what());
    } catch (...) {
      s_audio_queue.record_worker_error();
      spdlog::error("[NotifyAudio] worker error: unknown exception");
    }
  }

  s_audio_queue.set_worker_active(false);
  const auto diagnostics = s_audio_queue.diagnostics();
  spdlog::debug("[NotifyAudio] worker stopped dequeued={} errors={}", diagnostics.dequeued,
                diagnostics.worker_errors);
}
#endif
} // namespace

void notification_audio_init()
{
  if (s_notification_audio_initialized) {
    return;
  }

#if _WIN32
  if (!notification_policy_any_audio_enabled()) {
    spdlog::debug("[NotifyAudio] Windows notification audio disabled");
    s_notification_audio_initialized = true;
    return;
  }

  std::call_once(s_sound_buffers_once, initialize_sound_buffers);
  std::call_once(s_audio_worker_once, []() {
    s_audio_worker_thread = std::thread(notification_audio_worker_main);
  });
  spdlog::debug("[NotifyAudio] Windows notification audio initialized with generated cue catalog");
#elif STFCMOD_PLATFORM_MACOS
  if (notification_policy_any_audio_enabled()) {
    spdlog::warn("[NotifyAudio] macOS does not support notification audio yet; configured audio deliveries are "
                 "unavailable");
  } else {
    spdlog::debug("[NotifyAudio] Notification audio: macOS does not support this feature yet (no-op)");
  }
#else
  spdlog::debug("[NotifyAudio] Notification audio: platform not supported (no-op)");
#endif
  s_notification_audio_initialized = true;
}

void notification_audio_shutdown()
{
#if STFCMOD_PLATFORM_WINDOWS
  s_audio_queue.request_shutdown();

  if (!s_audio_worker_thread.joinable()) {
    return;
  }

  const auto join_started_at = std::chrono::steady_clock::now();
  s_audio_worker_thread.join();
  const auto join_elapsed = std::chrono::steady_clock::now() - join_started_at;
  if (join_elapsed > kAudioJoinWarnThreshold) {
    spdlog::warn("[NotifyAudio] worker join waited {} ms during shutdown",
                 std::chrono::duration_cast<std::chrono::milliseconds>(join_elapsed).count());
  }
#endif
}

void notification_audio_play(NotificationSound sound, std::string_view event_name)
{
  if (sound == NotificationSound::None) {
    return;
  }

#if STFCMOD_PLATFORM_WINDOWS
  notification_audio_init();
  if (!sound_buffer_available(sound)) {
    spdlog::warn("[NotifyAudio] Missing notification sound buffer event={} sound={}", event_name,
                 notification_sound_name(sound));
    return;
  }

  AudioPlaybackRequest request;
  request.sound      = sound;
  request.event_name = std::string(event_name);
  if (!s_audio_queue.enqueue(std::move(request))) {
    const auto diagnostics = s_audio_queue.diagnostics();
    spdlog::warn("[NotifyAudio] Dropped notification sound event={} sound={} reason={} queue_size={} dropped={}",
                 event_name, notification_sound_name(sound),
                 diagnostics.shutdown_requested ? "shutdown" : "full",
                 diagnostics.depth, diagnostics.dropped);
    return;
  }
#else
  spdlog::debug("[NotifyAudio] Suppressed notification sound event={} sound={} reason=unsupported-platform",
                event_name, notification_sound_name(sound));
#endif
}
