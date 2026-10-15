#pragma once

#include <filesystem>
#include <string>

// Keep the extension intact: community_patch.log.1 is the newest previous session.
inline void RotateLogHistory(const std::filesystem::path& log)
{
  namespace fs = std::filesystem;
  const auto backup = [&](int index) {
    auto path = log;
    path += "." + std::to_string(index);
    return path;
  };
  fs::remove(backup(5));
  for (int index = 4; index >= 1; --index) {
    if (fs::exists(backup(index))) {
      fs::rename(backup(index), backup(index + 1));
    }
  }
  if (fs::exists(log)) {
    fs::rename(log, backup(1));
  }
}
