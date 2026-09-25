#include "patches/notification_audio.h"
#include "patches/notification_audio_platform.h"

#include <fstream>
#include <stdexcept>

void Check(bool condition)
{
  if (!condition)
    throw std::runtime_error("audio file regression");
}

void WriteWave(const std::filesystem::path& file, unsigned seconds)
{
  std::ofstream out(file, std::ios::binary);
  auto          u16 = [&](unsigned value) {
    out.put(static_cast<char>(value));
    out.put(static_cast<char>(value >> 8));
  };
  auto u32 = [&](unsigned value) {
    u16(value);
    u16(value >> 16);
  };
  const auto bytes = seconds * 8000 * 2;
  out.write("RIFF", 4);
  u32(36 + bytes);
  out.write("WAVEfmt ", 8);
  u32(16);
  u16(1);
  u16(1);
  u32(8000);
  u32(16000);
  u16(2);
  u16(16);
  out.write("data", 4);
  u32(bytes);
  for (unsigned i = 0; i < bytes / 2; ++i)
    u16(i % 20 < 10 ? 1200 : static_cast<unsigned>(-1200));
}

int main(int argc, char** argv)
{
  Check(argc == 3);
  // Construct UTF-8 here: Windows main(char**) arguments use the active code
  // page, whereas TOML paths and File::Config() are UTF-8.
  const auto directory = std::filesystem::u8path(argv[1]) / std::filesystem::path(u8"r\u00e9pertoire");
  std::filesystem::create_directories(directory);
  const auto name = std::string("arrival \xc3\xa9.WAV");
  const auto wave = directory / std::filesystem::u8path(name);
  WriteWave(wave, 1);
  const auto cue = notification_audio_load(name, directory);
  Check(cue.enabled() && cue.data && cue.source == name);
  Check(cue.duration_seconds > 0.99 && cue.duration_seconds < 1.01);
  const auto absolute = wave.u8string();
  Check(notification_audio_load(std::string(absolute.begin(), absolute.end()), {}).enabled());
  std::filesystem::remove(wave);
  Check(cue.enabled() && !cue.data->empty()); // prepared clip survives file removal
  const auto missing = notification_audio_load(name, directory);
  Check(!missing.enabled() && missing.source == name);
  std::ofstream(directory / "corrupt.wav") << "not audio";
  Check(!notification_audio_load("corrupt.wav", directory).enabled());
  Check(!notification_audio_load("https://example.test/clip.mp3", directory).enabled());
  Check(!notification_audio_load("file:///clip.wav", directory).enabled());
  Check(!notification_audio_load("directory.wav", directory).enabled());
  Check(!notification_audio_load("clip.txt", directory).enabled());
  WriteWave(directory / "long.wav", 31);
  Check(!notification_audio_load("long.wav", directory).enabled());
  {
    std::ofstream out(directory / "large.mp3", std::ios::binary);
    out.seekp(kNotificationAudioMaxBytes);
    out.put('x');
  }
  Check(!notification_audio_load("large.mp3", directory).enabled());
  Check(notification_audio_load("arrival", directory).sound == NotificationSound::Arrival);
  Check(notification_audio_load("victory", directory).sound == NotificationSound::Success);
  Check(!notification_audio_load("none", directory).enabled());
  Check(!notification_audio_load("off", directory).enabled());
  const auto mp3 = notification_audio_load(argv[2], {});
  Check(mp3.enabled() && mp3.data && !mp3.data->empty());
  Check(mp3.duration_seconds > 0 && mp3.duration_seconds <= kNotificationAudioMaxSeconds);
  // Deliberately do not play sound in this deterministic fixture.
}
