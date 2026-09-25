#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

inline constexpr size_t kNotificationAudioMaxBytes = 16 * 1024 * 1024;
inline constexpr double kNotificationAudioMaxSeconds = 30.0;

// Validate and prepare local bytes once at configuration load. Empty means
// unsupported, malformed, or over the decoded size/duration limit.
std::vector<uint8_t> notification_audio_platform_prepare(std::span<const uint8_t> bytes, double& duration_seconds);

enum class AudioPlaybackResult { Started, Unchanged, Stopped };
[[nodiscard]] AudioPlaybackResult notification_audio_platform_play(const uint8_t* data, size_t size);
