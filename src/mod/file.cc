// Owns the mod's file names and their directory, the command-line flags that change them, and moving files left by
// older releases.

#include "file.h"
#include "arguments.h"
#include "profile_launch.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>

#if __APPLE__
std::filesystem::path MacLibraryDirectory(); // file_macos.mm
#endif

namespace
{
constexpr std::string_view kDefaultConfig  = "community_patch_settings.toml";
constexpr std::string_view kDefaultVars    = "community_patch_runtime.vars";
constexpr std::string_view kDefaultLog     = "community_patch.log";
constexpr std::string_view kDefaultBattles = "patch_battlelogs_sent.json";

// Names used by older releases; "path" is their typo.
constexpr std::string_view kLegacyVars   = "community_path_runtime.vars";
constexpr std::string_view kLegacyParsed = "community_patch_settings_parsed.toml";

std::string           config_name, vars_name, log_name, battles_name, profile;
const std::string     default_config{kDefaultConfig};
std::filesystem::path profile_path;
std::filesystem::path admitted_directory;
#ifdef _MODDBG
bool debug = true;
#else
bool debug = false;
#endif
bool trace = false;

std::string ToUtf8(const std::filesystem::path& path)
{
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}

// Where the mod's files live: beside the -ccm profile, the macOS preferences folder, or the working directory.
std::filesystem::path Directory([[maybe_unused]] bool legacy = false)
{
  if (!admitted_directory.empty())
    return admitted_directory;
  if (!profile_path.empty())
    return profile_path.parent_path();
#if __APPLE__
  return MacLibraryDirectory() / "Preferences" / (legacy ? "com.tashcan.startrekpatch" : "com.stfcmod.startrekpatch");
#else
  return {};
#endif
}

// Settings used to live in com.tashcan.startrekpatch. Move that folder over once and leave a symlink and a note
// behind for anyone who still looks there.
void MigrateMacPreferences()
{
#if __APPLE__
  namespace fs         = std::filesystem;
  const auto file_path = File::Path(config_name);
  const auto new_dir   = file_path.parent_path();
  if (fs::exists(file_path) || fs::exists(new_dir))
    return;

  const auto old_dir  = Directory(true);
  const auto old_path = old_dir / config_name;
  if (!fs::is_regular_file(old_path))
    return;

  spdlog::info("moving settings from {} to {}", ToUtf8(old_dir), ToUtf8(new_dir));
  try {
    fs::rename(old_dir, new_dir);
    fs::create_directories(old_dir);
    fs::create_symlink(file_path, old_path);

    std::ofstream info(old_dir / "update-info.txt");
    info << "Your config has been moved!\n\n";
    info << "You can now find your config at " << file_path << "\n\n";
    info << "A symlink has been placed for your convenience, but it is generally recommended, that you use the new "
            "path and delete this directory going forward.";
  } catch (const std::exception& error) {
    spdlog::warn("could not move settings from {}: {}", ToUtf8(old_dir), error.what());
  }
#endif
}
} // namespace

namespace File
{
void Init()
{
  const auto& args = ProcessArguments();
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-debug")
      debug = true;
    else if (args[i] == "-trace")
      trace = true;
    else if (args[i] == "-ccm" && i + 1 < args.size())
      profile_path = FromUtf8(args[++i]);
  }

#if STFC_PROFILES_HOST
  if (profile_launch::Requested()) {
    admitted_directory = profile_launch::Directory();
    if (admitted_directory.empty() || !admitted_directory.is_absolute())
      throw std::runtime_error("The admitted profile directory is unavailable or not absolute.");
    config_name  = ToUtf8(admitted_directory / "config.toml");
    vars_name    = ToUtf8(admitted_directory / "runtime.toml");
    log_name     = ToUtf8(admitted_directory / "logs" / "community-mod.log");
    battles_name = ToUtf8(admitted_directory / "logs" / "battles.log");
    profile      = profile_launch::Id();
    return;
  }
#endif

  if (profile_path.empty()) {
    config_name  = kDefaultConfig;
    vars_name    = kDefaultVars;
    log_name     = kDefaultLog;
    battles_name = kDefaultBattles;
    return;
  }

  profile             = ToUtf8(profile_path.stem());
  auto with_extension = [](const char* extension) {
    return ToUtf8(std::filesystem::path(profile_path).replace_extension(extension));
  };
  config_name  = with_extension(".toml");
  vars_name    = with_extension(".vars");
  log_name     = with_extension(".log");
  battles_name = with_extension(".json");
}

const std::string& Config()
{ return config_name; }

const std::string& Vars()
{ return vars_name; }

const std::string& Log()
{ return log_name; }

const std::string& Battles()
{ return battles_name; }

const std::string& DefaultConfig()
{ return default_config; }

const std::string& Profile()
{ return profile; }

bool Debug()
{ return debug; }

bool Trace()
{ return trace; }

std::filesystem::path Path(std::string_view name, bool create_dir)
{
  // A log may be the first file opened. Migrate the existing macOS preferences
  // before creating the destination directory, otherwise migration sees that
  // new directory and deliberately leaves the legacy settings behind.
  if (create_dir)
    MigrateMacPreferences();
  const auto input = FromUtf8(name);
  const auto path  = input.is_absolute() ? input : Directory() / input.filename();
  if (create_dir && !path.parent_path().empty()) {
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
  }
  // A -ccm profile path is already complete; joining an absolute path replaces the directory.
  return path;
}

std::filesystem::path FromUtf8(std::string_view text)
{ return std::u8string(text.begin(), text.end()); }

void MigrateLegacyFiles()
{
  MigrateMacPreferences();

  std::error_code ignored;
  std::filesystem::remove(Path(kLegacyVars), ignored);
  if (std::filesystem::remove(Path(kLegacyParsed), ignored))
    spdlog::info("removed {}, which older releases wrote", kLegacyParsed);
}
} // namespace File
