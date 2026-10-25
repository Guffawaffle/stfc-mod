// Exercise actual File routing with a supplied admitted context. Routing cases do not access files. The macOS
// migration case substitutes a private temporary Library root; it never reads the user's Library or profile stores.
#include "arguments.h"
#include "file.h"
#include "profile_launch.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if __APPLE__
#include <chrono>
#include <fstream>
#include <iterator>
#endif

namespace
{
std::vector<std::string> arguments;
bool                     named = false;
std::filesystem::path    directory;
#if __APPLE__
std::filesystem::path library_directory = "/synthetic-profile-routing-library";
#endif
constexpr std::string_view id = "0123456789abcdef0123456789abcdef";
void                       Require(bool ok, const char* what)
{
  if (!ok)
    throw std::runtime_error(what);
}
std::string Utf8(const std::filesystem::path& path)
{
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}
#if __APPLE__
class TemporaryDirectory
{
public:
  TemporaryDirectory()
      : parent_(std::filesystem::canonical(std::filesystem::temp_directory_path()))
  {
    const auto sequence = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
      root_ = parent_ / ("stfc-profile-routing-fixture-" + std::to_string(sequence) + "-" + std::to_string(attempt));
      if (std::filesystem::create_directory(root_)) {
        created_ = true;
        return;
      }
    }
    throw std::runtime_error("could not create an isolated migration fixture directory");
  }
  ~TemporaryDirectory()
  {
    // Remove only the root this fixture created, under the resolved system temporary directory.
    if (created_ && root_.is_absolute() && root_.parent_path() == parent_
        && root_.filename().string().starts_with("stfc-profile-routing-fixture-")) {
      std::error_code ignored;
      std::filesystem::remove_all(root_, ignored);
    }
  }
  TemporaryDirectory(const TemporaryDirectory&)                     = delete;
  TemporaryDirectory&          operator=(const TemporaryDirectory&) = delete;
  const std::filesystem::path& Root() const noexcept
  { return root_; }

private:
  std::filesystem::path parent_, root_;
  bool                  created_ = false;
};

void Write(const std::filesystem::path& path, std::string_view content)
{
  std::ofstream file;
  file.exceptions(std::ios::badbit | std::ios::failbit);
  file.open(path, std::ios::binary);
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.close();
}
std::string Read(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  Require(file.is_open(), "migrated fixture file could not be read");
  std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  Require(!file.bad(), "migrated fixture file read failed");
  return content;
}

void LegacyMacMigration()
{
  TemporaryDirectory fixture;
  library_directory        = fixture.Root() / "Library";
  const auto old_directory = library_directory / "Preferences" / "com.tashcan.startrekpatch";
  const auto new_directory = library_directory / "Preferences" / "com.stfcmod.startrekpatch";
  std::filesystem::create_directories(old_directory);
  constexpr std::string_view settings     = "# SYNTHETIC legacy migration fixture\n[graphics]\nui_scale = 1.25\n";
  constexpr std::string_view log          = "SYNTHETIC earlier session log\n";
  const auto                 old_settings = old_directory / "community_patch_settings.toml";
  const auto                 new_settings = new_directory / "community_patch_settings.toml";
  Write(old_settings, settings);
  Write(old_directory / "community_patch.log", log);

  File::Init();
  Require(!std::filesystem::exists(new_directory), "fixture created the new preferences directory too early");
  // This is the first native operation StartLogging performs, before Config::Get or MigrateLegacyFiles.
  const auto log_path = File::Path(File::Log(), true);
  Require(log_path == new_directory / "community_patch.log", "ordinary macOS log routing changed");
  Require(Read(new_settings) == settings && Read(log_path) == log,
          "opening the log directory prevented migration or changed existing settings/log bytes");
  Require(std::filesystem::is_symlink(std::filesystem::symlink_status(old_settings))
              && std::filesystem::equivalent(old_settings, new_settings),
          "legacy settings did not retain the migration compatibility symlink");
  Require(std::filesystem::is_regular_file(old_directory / "update-info.txt"), "migration note was not created");
  File::MigrateLegacyFiles();
  Require(File::Path(File::Log(), true) == log_path && Read(new_settings) == settings && Read(log_path) == log,
          "repeated migration or log-directory preparation changed preserved fixture bytes");
}
#endif
} // namespace

const std::vector<std::string>& ProcessArguments()
{ return arguments; }
namespace profile_launch
{
bool Requested() noexcept
{ return named; }
std::string_view Id() noexcept
{ return id; }
const std::filesystem::path& Directory()
{ return directory; }
} // namespace profile_launch
#if __APPLE__
std::filesystem::path MacLibraryDirectory()
{ return library_directory; }
#endif

int main(int count, char** values)
{
  Require(count == 2, "one routing case is required");
  const std::string_view mode     = values[1];
  const auto             absolute = std::filesystem::current_path() / std::filesystem::u8path(u8"profile-routing-Ω");
  arguments                       = {"fixture"};
#if __APPLE__
  if (mode == "mac-legacy-migration") {
    LegacyMacMigration();
    std::cout << "macOS legacy settings migration passed: log-first ordering and preserved synthetic bytes\n";
    return 0;
  }
#endif
  if (mode == "custom-relative")
    arguments = {"fixture", "-ccm", "custom/choice.cfg", "-debug", "-trace"};
  else if (mode == "custom-absolute")
    arguments = {"fixture", "-ccm", Utf8(absolute / "choice.toml")};
  else if (mode == "named" || mode == "unprepared") {
    named     = true;
    directory = mode == "named" ? absolute : std::filesystem::path{};
    arguments = {"fixture", "-ccm", "wrong/context.toml", "-stfc-profile", std::string(id)};
  } else
    Require(mode == "ordinary", "unknown fixture mode");
  if (mode == "unprepared") {
    bool refused = false;
    try {
      File::Init();
    } catch (const std::runtime_error&) {
      refused = true;
    }
    Require(refused && File::Config().empty(), "unavailable named context fell back to ordinary configuration");
  } else {
    File::Init();
    if (mode == "ordinary") {
      Require(File::Config() == "community_patch_settings.toml" && File::Vars() == "community_patch_runtime.vars"
                  && File::Log() == "community_patch.log" && File::Battles() == "patch_battlelogs_sent.json"
                  && File::Profile().empty(),
              "ordinary file namespace changed");
    } else if (mode == "custom-relative") {
      Require(File::Config() == "custom/choice.toml" && File::Vars() == "custom/choice.vars"
                  && File::Log() == "custom/choice.log" && File::Battles() == "custom/choice.json"
                  && File::Profile() == "choice" && File::Debug() && File::Trace(),
              "custom namespace changed");
      Require(File::Path(File::Config()) == std::filesystem::path("custom/choice.toml"),
              "custom path duplicated directory");
    } else if (mode == "custom-absolute") {
      Require(File::Path(File::Config()) == absolute / "choice.toml"
                  && File::Path(File::Log()) == absolute / "choice.log",
              "absolute custom directory was dropped");
    } else {
      Require(File::Path(File::Config()) == directory / "config.toml"
                  && File::Path(File::Vars()) == directory / "runtime.toml"
                  && File::Path(File::Log()) == directory / "logs" / "community-mod.log"
                  && File::Path(File::Battles()) == directory / "logs" / "battles.log" && File::Profile() == id,
              "named namespace, precedence or logs directory changed");
      auto state = File::Path(File::Config());
      state.replace_extension(".state.json");
      Require(state == directory / "config.state.json", "existing derived state namespace changed");
    }
  }
  std::cout << "Profile file routing passed: " << mode << '\n';
}
