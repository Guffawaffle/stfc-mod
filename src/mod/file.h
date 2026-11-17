#pragma once

// Where the mod's settings, runtime vars, log, and battle-log cache live. `-ccm <path>` selects custom file names,
// not account isolation. An admitted immutable Profiles context takes precedence and supplies its existing paths.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace spdlog
{
class logger;
}

namespace File
{
// Parses the command line. Call once, before anything else here.
void Init();

// File names as configured; named Profiles return complete admitted paths. Resolve all through Path().
const std::string& Config();
const std::string& Vars();
const std::string& Log();
const std::string& Battles();
// The settings file name used without -ccm, for messages that point users at it.
const std::string& DefaultConfig();

// The admitted profile ID, otherwise -ccm stem ("alt" for alt.toml), otherwise empty. Not authenticated account ID.
const std::string& Profile();
// Explicit custom routing, independent of the human-readable stem (which can be empty).
bool HasCustomNames();
// -debug and -trace set the starting log level; _MODDBG builds start at debug.
bool Debug();
bool Trace();

// Full path of a mod file. On macOS this is ~/Library/Preferences/com.stfcmod.startrekpatch unless -ccm points
// elsewhere; on Windows it is the game's working directory.
std::filesystem::path Path(std::string_view name, bool create_dir = false);
// A path from UTF-8 text. std::filesystem::path(std::string) reads the ANSI code page on Windows instead.
std::filesystem::path FromUtf8(std::string_view text);

struct SessionLog {
  std::shared_ptr<spdlog::logger> logger;
  std::string                     rotation_error, open_error;
};
// Rotate/open the session log, appending if rotation fails. A supplied fallback is an explicit ordinary-launch
// policy: without one, an open failure propagates; if both attempts fail, return a logger without file sinks.
SessionLog OpenSessionLog(const std::filesystem::path&                preferred,
                          const std::optional<std::filesystem::path>& fallback = std::nullopt);

// Moves settings left behind by older releases. Logs, so call after the logger is up.
void MigrateLegacyFiles();
} // namespace File
