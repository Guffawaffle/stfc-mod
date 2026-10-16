#pragma once

#include "file.h"
#include <filesystem>
#include <string_view>

// Group science output beneath the normal mod data location on each platform.
// Call inside the writer's initialization try/catch so a filesystem error only
// disables that capture. Groups and filenames are compile-time caller literals.
inline std::filesystem::path ScienceTracePath(std::string_view group, std::string_view filename)
{
  auto directory = std::filesystem::path(File::MakePath("science", true)) / group;
  std::filesystem::create_directories(directory);
  return directory / filename;
}
