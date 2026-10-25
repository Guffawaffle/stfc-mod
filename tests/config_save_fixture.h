#pragma once

#ifdef NDEBUG
#error Configuration save fixtures require active assertions
#endif

// Test-only, exclusively created temporary directory. No caller-selected/user
// settings paths are accepted.
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

class ConfigSaveFixtureDirectory
{
public:
  ConfigSaveFixtureDirectory()
      : parent_(std::filesystem::canonical(std::filesystem::temp_directory_path()))
  {
    const auto sequence = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
      root_ = parent_ / ("stfc-config-save-fixture-" + std::to_string(sequence) + "-" + std::to_string(attempt));
      if (std::filesystem::create_directory(root_)) {
        created_ = true;
        return;
      }
    }
    throw std::runtime_error("could not create isolated config-save fixture directory");
  }
  ~ConfigSaveFixtureDirectory()
  {
    if (created_ && root_.is_absolute() && root_.parent_path() == parent_
        && root_.filename().string().starts_with("stfc-config-save-fixture-")) {
      std::error_code ignored;
      std::filesystem::remove_all(root_, ignored);
    }
  }
  ConfigSaveFixtureDirectory(const ConfigSaveFixtureDirectory &)             = delete;
  ConfigSaveFixtureDirectory  &operator=(const ConfigSaveFixtureDirectory &) = delete;
  const std::filesystem::path &Root() const noexcept
  { return root_; }

private:
  std::filesystem::path parent_, root_;
  bool                  created_ = false;
};
