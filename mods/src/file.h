/**
 * @file file.h
 * @brief File-path resolution and naming for mod config, log, and data files.
 *
 * The File class manages path derivation from a base config filename,
 * supports a -ccm command-line override for multiple config profiles on
 * Windows, and handles macOS library-directory layout via MakePath().
 */
#include "config.h"
#include "patches/mapkey.h"
#include "prime/KeyCode.h"
#include "str_utils.h"
#include "version.h"
#include <prime\Toast.h>

#include <EASTL/tuple.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#if !_WIN32
#include "folder_manager.h"
#else
#include <shellapi.h>
#include <windows.h>
#endif

// Original output file names
#define FILE_DEF_CONFIG "community_patch_settings.toml"
#define FILE_DEF_LOG "community_patch.log"
#define FILE_DEF_VARS "community_patch_runtime.vars"
#define FILE_DEF_VARS_OLD "community_path_runtime.vars"
#define FILE_DEF_BL "patch_battlelogs_sent.json"
#define FILE_DEF_PARSED "community_patch_settings_parsed.toml"
#define FILE_DEF_TITLE L"Star Trek Fleet Command"

#define FILE_EXT_TOML ".toml"
#define FILE_EXT_VARS ".vars"
#define FILE_EXT_LOG ".log"
#define FILE_EXT_JSON ".json"

/**
 * @brief Central file-path manager for the mod's config, log, and data files.
 *
 * All paths are lazily initialised on first access (via Init()).
 * On Windows, the -ccm <profile> argument allows multiple config profiles
 * by deriving all filenames from a single base path.
 */
class File
{
public:
  /** @brief Parse command-line args and populate all cached paths. */
  static void         Init();

  /** @brief Window title (prefixed with profile name if -ccm is active). */
  static std::wstring Title();

  /** @brief Default config filename (always community_patch_settings.toml). */
  static const char*  Default();

  /** @brief Active config filename (may differ from Default if -ccm is used). */
  static const char*  Config();

  /** @brief Runtime-vars snapshot filename. */
  static const char*  Vars();

  /** @brief Log filename. */
  static const char*  Log();

  /** @brief Battle-log JSON filename. */
  static const char*  Battles();

  /** @brief True if running with a -ccm profile override. */
  static bool         hasCustomNames();

  /** @brief True if -debug was passed on the command line. */
  static bool         hasDebug();

  /** @brief True if -trace was passed on the command line. */
  static bool         hasTrace();

  /**
   * @brief Build a full path for a mod file.
   * @param filename    The leaf filename.
   * @param create_dir  Create parent directories if they don't exist (macOS only).
   * @param old_path    Use the legacy bundle-id directory (macOS migration).
   * @return Platform-appropriate path string.
   */
#if _WIN32
  static std::string_view MakePath(std::string_view filename, bool create_dir = false, bool old_path = false);
#else
  static std::u8string MakePath(std::string_view filename, bool create_dir = false, bool old_path = false);
#endif

private:
  static std::filesystem::path Path();

  static bool debug;
  static bool trace;
  static bool override;
  static bool initialized;

  static std::wstring cacheNameTitle;
  static std::string  cacheNameBattles;
  static std::string  cacheNameLog;
  static std::string  cacheNameVar;
  static std::string  cacheNameConfig;
  static std::string  cacheNameDefault;

  static std::filesystem::path configPath;
};
