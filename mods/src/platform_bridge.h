/**
 * @file platform_bridge.h
 * @brief Thin platform adapters for command-line and mod file storage paths.
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace platform_bridge {

struct CommandLineOptions {
  bool debug = false;
  bool trace = false;
  std::optional<std::filesystem::path> config_override;
};

CommandLineOptions ReadCommandLineOptions();
std::filesystem::path ModStoragePath(std::string_view filename, bool create_dir = false, bool old_path = false);
std::string PathToUtf8String(const std::filesystem::path& path);

}
