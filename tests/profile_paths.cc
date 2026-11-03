// Exercise actual File routing with a supplied admitted context. Log/cleanup cases use privately owned temporary
// files; the macOS migration case substitutes a temporary Library root. No real profile stores are accessed.
#include "arguments.h"
#include "file.h"
#include "profile_launch.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
  Require(file.is_open(), "fixture file could not be read");
  std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  Require(!file.bad(), "fixture file read failed");
  return content;
}

// Drop the fixture's registered logger even when an assertion throws, before removing its private directory.
struct LogRegistration {
  ~LogRegistration()
  { spdlog::drop("default"); }
};

class CurrentDirectory
{
public:
  explicit CurrentDirectory(const std::filesystem::path& selected)
      : previous_(std::filesystem::current_path())
  { std::filesystem::current_path(selected); }
  ~CurrentDirectory()
  {
    std::error_code ignored;
    std::filesystem::current_path(previous_, ignored);
  }
  CurrentDirectory(const CurrentDirectory&)            = delete;
  CurrentDirectory& operator=(const CurrentDirectory&) = delete;

private:
  std::filesystem::path previous_;
};

void CleanupCases(std::string_view mode)
{
  Require(mode == "cleanup-custom" || mode == "cleanup-named", "unknown cleanup mode");
  TemporaryDirectory fixture;
  const auto         working  = fixture.Root() / "working";
  const auto         selected = fixture.Root() / std::filesystem::path(u8"selected-Ω");
  std::filesystem::create_directories(working);
  std::filesystem::create_directories(selected);
  CurrentDirectory working_directory(working);
  named     = mode == "cleanup-named";
  directory = selected;
  arguments = {"fixture", "-ccm", Utf8(selected / "custom.cfg")};
  File::Init();

  std::vector<std::pair<std::filesystem::path, std::string>> retained;
  const auto keep = [&](const std::filesystem::path& path, std::string bytes) {
    std::filesystem::create_directories(path.parent_path());
    Write(path, bytes);
    retained.emplace_back(path, std::move(bytes));
  };
  for (const auto* name : {"community_patch_settings.toml", "community_patch_runtime.vars", "community_patch.log",
                           "patch_battlelogs_sent.json", "community_patch_state.json", "community_path_runtime.vars",
                           "community_patch_settings_parsed.toml"})
    keep(working / name, "SYNTHETIC unrelated working-directory bytes\n");
  for (const auto* name : {&File::Config(), &File::Vars(), &File::Log(), &File::Battles()})
    keep(File::Path(*name), "SYNTHETIC selected primary/runtime/log/cache bytes\n");
  auto state = File::Path(File::Config());
  state.replace_extension(".state.json");
  keep(state, "SYNTHETIC selected state bytes\n");
  const auto legacy_vars   = selected / "community_path_runtime.vars";
  const auto legacy_parsed = selected / "community_patch_settings_parsed.toml";
  Write(legacy_vars, "SYNTHETIC selected obsolete vars\n");
  Write(legacy_parsed, "SYNTHETIC selected obsolete parsed settings\n");

  File::MigrateLegacyFiles();
  Require(!std::filesystem::exists(legacy_vars) && !std::filesystem::exists(legacy_parsed),
          "cleanup must remove obsolete files in the selected namespace");
  for (const auto& [path, bytes] : retained)
    Require(Read(path) == bytes, "cleanup changed unrelated working-directory or selected live file bytes");
  File::MigrateLegacyFiles();
  for (const auto& [path, bytes] : retained)
    Require(Read(path) == bytes, "repeated cleanup changed retained bytes");
}

void SessionLogCases(std::string_view mode)
{
  TemporaryDirectory fixture;
  LogRegistration    registration;
  const auto         log    = fixture.Root() / std::filesystem::path(u8"session-Ω.log");
  const auto         backup = [&](int index) {
    auto path = log;
    path += "." + std::to_string(index);
    return path;
  };
  const auto blocked = fixture.Root() / "blocked";
  Write(blocked, "SYNTHETIC non-directory\n");
  const auto unavailable = blocked / "session.log";
  if (mode == "log-no-fallback") {
    Write(log, "SYNTHETIC unrelated fallback\n");
    bool refused = false;
    try {
      File::OpenSessionLog(unavailable);
    } catch (const spdlog::spdlog_ex&) {
      refused = true;
    }
    Require(refused && Read(log) == "SYNTHETIC unrelated fallback\n",
            "custom/admitted log failure must not select another location");
    return;
  }
  if (mode == "log-both-fail") {
    auto opened = File::OpenSessionLog(unavailable, blocked / "fallback.log");
    Require(opened.logger && opened.logger->sinks().empty()
                && opened.open_error.find("previous location also failed") != std::string::npos,
            "two failed ordinary locations must retain diagnostics and return a console-ready logger");
    return;
  }
  Write(log, "SYNTHETIC previous session\n");
  if (mode == "log-rotation-error") {
    std::filesystem::create_directory(backup(5));
    Write(backup(5) / "retained", "SYNTHETIC blocked rotation\n");
    auto opened = File::OpenSessionLog(log);
    Require(!opened.rotation_error.empty() && opened.open_error.empty(), "rotation failure must be reported");
    opened.logger->set_formatter(
        std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "\n"));
    opened.logger->info("SYNTHETIC next session");
    opened.logger->flush();
    Require(Read(log) == "SYNTHETIC previous session\nSYNTHETIC next session\n",
            "rotation failure must append without losing the existing session");
    return;
  }
  Require(mode == "log-fallback" || mode == "log-rotation", "unknown session log mode");
  for (int index = 1; index <= 5; ++index)
    Write(backup(index), "SYNTHETIC backup " + std::to_string(index) + "\n");
  auto opened = mode == "log-fallback" ? File::OpenSessionLog(unavailable, log) : File::OpenSessionLog(log);
  Require(opened.logger && (mode == "log-fallback" ? !opened.open_error.empty() : opened.open_error.empty()),
          "ordinary fallback must report the preferred open failure");
  Require(Read(backup(1)) == "SYNTHETIC previous session\n" && Read(backup(5)) == "SYNTHETIC backup 4\n",
          "the actual session log location must retain the previous five sessions");
  opened.logger->set_formatter(
      std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "\n"));
  opened.logger->info("SYNTHETIC next session");
  opened.logger->flush();
  Require(Read(log) == "SYNTHETIC next session\n", "new session must not append after successful rotation");
}

#if __APPLE__
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
try {
  Require(count == 2, "one routing case is required");
  const std::string_view mode     = values[1];
  const auto             absolute = std::filesystem::current_path() / std::filesystem::u8path(u8"profile-routing-Ω");
  arguments                       = {"fixture"};
  if (mode.starts_with("cleanup-")) {
    CleanupCases(mode);
    std::cout << "Scoped legacy cleanup passed: " << mode << '\n';
    return 0;
  }
  if (mode.starts_with("log-")) {
    SessionLogCases(mode);
    std::cout << "Session log production file handling passed: " << mode << '\n';
    return 0;
  }
#if __APPLE__
  if (mode == "mac-legacy-migration") {
    LegacyMacMigration();
    std::cout << "macOS legacy settings migration passed: log-first ordering and preserved synthetic bytes\n";
    return 0;
  }
#endif
  if (mode == "custom-relative")
    arguments = {"fixture", "-ccm", "custom/choice.cfg", "-debug", "-trace"};
  else if (mode == "custom-repeated")
    arguments = {"fixture", "-ccm", "custom/choice.cfg", "-ccm", "later/ignored.cfg", "-debug", "-trace"};
  else if (mode == "custom-empty-first")
    arguments = {"fixture", "-ccm", "", "-ccm", "later/ignored.cfg", "-debug", "-trace"};
  else if (mode == "custom-empty-later")
    arguments = {"fixture", "-ccm", "custom/choice.cfg", "-ccm", "", "-debug", "-trace"};
  else if (mode == "custom-missing")
    arguments = {"fixture", "-ccm"};
  else if (mode == "custom-absolute")
    arguments = {"fixture", "-ccm", Utf8(absolute / "choice.toml")};
  else if (mode == "custom-directory")
    arguments = {"fixture", "-ccm", "custom/directory/"};
  else if (mode == "custom-directory-absolute")
    arguments = {"fixture", "-ccm", Utf8(absolute / "")};
  else if (mode == "named" || mode == "unprepared" || mode == "relative-admitted") {
    named     = true;
    directory = mode == "named"               ? absolute
                : mode == "relative-admitted" ? std::filesystem::path("relative/admitted")
                                              : std::filesystem::path{};
    arguments = {"fixture", "-ccm", "wrong/context.toml", "-stfc-profile", std::string(id)};
  } else
    Require(mode == "ordinary", "unknown fixture mode");
  if (mode == "unprepared" || mode == "relative-admitted") {
    bool refused = false;
    try {
      File::Init();
    } catch (const std::runtime_error&) {
      refused = true;
    }
    Require(refused && File::Config().empty() && File::Vars().empty() && File::Log().empty() && File::Battles().empty()
                && File::Profile().empty(),
            "unavailable named context fell back to ordinary or partial file names");
  } else {
    File::Init();
    if (mode == "ordinary" || mode == "custom-empty-first" || mode == "custom-missing") {
      Require(File::Config() == "community_patch_settings.toml" && File::Vars() == "community_patch_runtime.vars"
                  && File::Log() == "community_patch.log" && File::Battles() == "patch_battlelogs_sent.json"
                  && File::Profile().empty() && !File::HasCustomNames(),
              "ordinary file namespace changed");
      if (mode == "custom-empty-first")
        Require(File::Debug() && File::Trace(), "an empty first operand must retain later debug/trace flags");
    } else if (mode == "custom-relative" || mode == "custom-repeated" || mode == "custom-empty-later") {
      Require(File::Config() == "custom/choice.toml" && File::Vars() == "custom/choice.vars"
                  && File::Log() == "custom/choice.log" && File::Battles() == "custom/choice.json"
                  && File::Profile() == "choice" && File::HasCustomNames() && File::Debug() && File::Trace(),
              "custom namespace changed");
      Require(File::Path(File::Config()) == std::filesystem::path("custom/choice.toml"),
              "custom path duplicated directory");
    } else if (mode == "custom-absolute") {
      Require(File::Path(File::Config()) == absolute / "choice.toml"
                  && File::Path(File::Log()) == absolute / "choice.log" && File::HasCustomNames(),
              "absolute custom directory was dropped");
    } else if (mode == "custom-directory" || mode == "custom-directory-absolute") {
      const auto parent = mode == "custom-directory" ? std::filesystem::path("custom/directory") : absolute;
      Require(File::HasCustomNames() && File::Profile().empty(),
              "an empty display stem discarded explicit custom routing");
      Require(File::Path(File::Config()) == parent / ".toml" && File::Path(File::Log()) == parent / ".log"
                  && File::Path(File::Vars()) == parent / ".vars" && File::Path(File::Battles()) == parent / ".json",
              "directory-only custom routing changed file names");
    } else {
      Require(File::Path(File::Config()) == directory / "config.toml"
                  && File::Path(File::Vars()) == directory / "runtime.toml"
                  && File::Path(File::Log()) == directory / "logs" / "community-mod.log"
                  && File::Path(File::Battles()) == directory / "logs" / "battles.log" && File::Profile() == id
                  && File::HasCustomNames(),
              "named namespace, precedence or logs directory changed");
      auto state = File::Path(File::Config());
      state.replace_extension(".state.json");
      Require(state == directory / "config.state.json", "existing derived state namespace changed");
    }
  }
  std::cout << "Profile file routing passed: " << mode << '\n';
} catch (const std::exception& error) {
  std::cerr << "Profile file fixture failed: " << error.what() << '\n';
  return 1;
}
