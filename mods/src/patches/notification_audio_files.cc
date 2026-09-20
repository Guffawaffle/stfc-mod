#include "notification_audio.h"
#include "notification_audio_platform.h"

#include <fstream>
#include <spdlog/spdlog.h>

NotificationAudioCue notification_audio_load(std::string_view value, const std::filesystem::path& directory)
{
  NotificationAudioCue cue;
  if (const auto builtin = notification_sound_from_name(value))
    return NotificationAudioCue(*builtin);

  // Preserve user spelling in parsed config even if a file is temporarily missing.
  cue.source = value;
  try {
    if (value.empty() || value.find('\0') != std::string_view::npos || value.find("://") != std::string_view::npos)
      throw std::runtime_error("expected a built-in cue or local WAV/MP3 path; URLs are not supported");
    auto file      = std::filesystem::u8path(value);
    auto extension = file.extension().string();
    for (auto& ch : extension)
      if (ch >= 'A' && ch <= 'Z')
        ch += 'a' - 'A';
    if (extension != ".wav" && extension != ".mp3")
      throw std::runtime_error("expected a built-in cue or a .wav/.mp3 file");
    if (file.is_relative())
      file = directory / file;
    if (!std::filesystem::is_regular_file(file))
      throw std::runtime_error("sound file is missing or not a regular file");
    const auto size = std::filesystem::file_size(file);
    if (!size || size > kNotificationAudioMaxBytes)
      throw std::runtime_error("sound file must be between 1 byte and 16 MiB");
    std::ifstream        input(file, std::ios::binary);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
      throw std::runtime_error("could not read sound file");
    auto prepared = notification_audio_platform_prepare(bytes);
    if (prepared.empty())
      throw std::runtime_error(
          "unsupported/corrupt audio, unavailable decoder, or clip exceeds 30 seconds/16 MiB decoded");
    cue.data = std::make_shared<const std::vector<uint8_t>>(std::move(prepared));
  } catch (const std::exception& error) {
    spdlog::warn("[NotifyAudio] Cannot load '{}': {}; this alert will be silent", cue.source, error.what());
  }
  return cue;
}
