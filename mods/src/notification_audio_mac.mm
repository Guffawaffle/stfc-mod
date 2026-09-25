#ifdef __APPLE__

#include "patches/notification_audio_platform.h"

#import <AppKit/AppKit.h>
#include <cmath>

std::vector<uint8_t> notification_audio_platform_prepare(std::span<const uint8_t> bytes, double& duration_seconds)
{
  duration_seconds = 0;
  @autoreleasepool {
    NSData* data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
    NSSound* sound = [[NSSound alloc] initWithData:data];
    const double duration = sound ? [sound duration] : 0;
    const bool valid = std::isfinite(duration) && duration > 0 && duration <= kNotificationAudioMaxSeconds;
    if (valid) duration_seconds = duration;
#if !__has_feature(objc_arc)
    [sound release];
#endif
    return valid ? std::vector<uint8_t>(bytes.begin(), bytes.end()) : std::vector<uint8_t>{};
  }
}

AudioPlaybackResult notification_audio_platform_play(const uint8_t* data, size_t size)
{
  @autoreleasepool {
    static NSSound* current_sound = nil;

    NSData* sound_data = [NSData dataWithBytes:data length:size];
    NSSound* sound      = [[NSSound alloc] initWithData:sound_data];
    if (!sound) return AudioPlaybackResult::Unchanged;

    [current_sound stop];
#if __has_feature(objc_arc)
    current_sound = sound;
#else
    [current_sound release];
    current_sound = sound;
#endif
    return [current_sound play] ? AudioPlaybackResult::Started : AudioPlaybackResult::Stopped;
  }
}

#endif
