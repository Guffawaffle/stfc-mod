/**
 * @file patches.cc
 * @brief Patch system coordinator — bootstraps logging, config, and all hook modules.
 *
 * This is the top-level orchestrator for the community patch. It detours the game's
 * il2cpp_init entry point so that, once the IL2CPP runtime is initialized, we can:
 *   1. Set up file paths and logging (spdlog).
 *   2. Load user configuration (Config::Get()).
 *   3. Iterate a table of patch modules, installing each one whose config flag is enabled.
 *
 * Each patch module is a self-contained Install*Hooks() function defined in parts/.
 */
#include "patches.h"
#include "file.h"
#include "patches/fleet_notifications.h"
#include "patches/notification_service.h"
#include "patches/sidecar_local_ingest.h"
#include "patches/sync_battle_logs.h"
#include "patches/sync_payload_builders.h"
#include "patches/sync_scheduler.h"
#include "patches/sync_transport.h"
#include "version.h"

#include <il2cpp/il2cpp-functions.h>

#include <spud/detour.h>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <cstdlib>
#include <exception>
#include <cstdio>
#include <string>

#if _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#endif

namespace
{
constexpr bool kLiveDebugOnlyHookIsolation = false;
constexpr auto kLegacyLogMaxBytes          = 512 * 1024;
constexpr auto kLegacyLogMaxFiles          = 2;

void LogRootHookInstallFailure(const std::string& message)
{
  spdlog::error("{}", message);
#if _WIN32
  OutputDebugStringA(message.c_str());
  OutputDebugStringA("\n");
#else
  std::fprintf(stderr, "%s\n", message.c_str());
#endif
}
}

// ─── Forward Declarations — per-module install functions ─────────────────────

void InstallUiScaleHooks();
void InstallZoomHooks();
void InstallBuffFixHooks();
#if _WIN32
void InstallFreeResizeHooks();
#endif
void InstallToastBannerHooks();
void InstallPanHooks();
void InstallImproveResponsivenessHooks();
void InstallFrameTickHooks();
void InstallHotkeyHooks();
void InstallLiveDebugHooks();
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
void InstallRefineryDiagnosticsHooks();
#endif
void InstallTestPatches();
void InstallMiscPatches();
void InstallChatPatches();
void InstallResolutionListFix();
void InstallTempCrashFixes();
void InstallSyncPatches();
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
void InstallObjectTrackers();
#endif
void InstallFleetArrivalHooks();
void InstallLoadingScreenBgHooks();

/**
 * @brief Hook: il2cpp_init
 *
 * Intercepts the IL2CPP runtime initialization to inject all community patches.
 * Original method: initializes the IL2CPP virtual machine for the given domain.
 * Our modification: after calling the original, sets up logging and config, then
 *   iterates a table of PatchEntry structs. Each entry pairs an Install*Hooks()
 *   function with a config boolean; only enabled patches are installed. This is
 *   the single entry point for all mod functionality.
 */
__int64 il2cpp_init_hook(auto original, const char* domain_name)
{
  struct PatchEntry {
    const char*                  name;
    std::pair<void (*)(), bool*> fnAndEnabled;
  };

#if _WIN32
#ifndef NDEBUG
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
#endif
#endif

  File::Init();

  auto file_logger = spdlog::rotating_logger_mt("default", File::Log(), kLegacyLogMaxBytes, kLegacyLogMaxFiles);
  auto sink        = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  file_logger->sinks().push_back(sink);
  spdlog::set_default_logger(file_logger);

  const auto log_level =
      File::hasTrace() ? spdlog::level::trace : (File::hasDebug() ? spdlog::level::debug : spdlog::level::info);

  spdlog::set_level(log_level);
  spdlog::flush_on(log_level);

  spdlog::info("Initializing STFC Community Mod ({})", VER_PRODUCT_VERSION_STR);
  spdlog::info("");
  if (File::hasCustomNames()) {
    spdlog::info("Using custom names");
  } else {
    spdlog::info("Using standard names");
  }

  spdlog::info("  Log: {}", File::Log());
  spdlog::warn(
      "  Local troubleshooting log is bounded and legacy-only; prefer explicit JSONL/ingress export for diagnostics.");
  spdlog::info("  Cfg: {}", File::Config());
  spdlog::info("  Var: {}", File::Vars());
  spdlog::info("   BL: {}", File::Battles());
  spdlog::info("");

#if VERSION_PATCH
  spdlog::warn("*** NOTE: Beta versions may have unexpected bugs and issues");
  spdlog::info("");
#endif

  spdlog::info("Please see https://github.com/netniv/stfc-mod for latest configuration help,");
  spdlog::info("examples and future releases, or visit the STFC Community Mod discord server");
  spdlog::info("at https://discord.gg/PrpHgs7Vjs");
  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");
  spdlog::info("Loading Configuration...");
  spdlog::info("");

  static auto& cfg = Config::Get();

  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");

  spdlog::info("Initializing code hooks:");
  auto             install_live_debug_hooks           =
      LiveDebugChannelEnabled() || (cfg.installSyncPatches && (cfg.sync_options.fleet_runtime || sidecar_local_ingest::FleetRuntimeEnabled()))
      || fleet_notifications_runtime_events_enabled();
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  auto             install_refinery_diagnostics_hooks = RefineryDiagnosticsEnabled();
#endif
  auto             install_frame_tick_hooks           =
      cfg.installHotkeyHooks || LiveDebugChannelEnabled();
  const PatchEntry patches[]                          = {
      {"UiScaleHooks", {InstallUiScaleHooks, &cfg.installUiScaleHooks}},
      {"ZoomHooks", {InstallZoomHooks, &cfg.installZoomHooks}},
      {"BuffFixHooks", {InstallBuffFixHooks, &cfg.installBuffFixHooks}},
      {"ToastBannerHooks", {InstallToastBannerHooks, &cfg.installToastBannerHooks}},
      {"PanHooks", {InstallPanHooks, &cfg.installPanHooks}},
      {"ImproveResponsivenessHooks", {InstallImproveResponsivenessHooks, &cfg.installImproveResponsivenessHooks}},
      {"FrameTickHooks", {InstallFrameTickHooks, &install_frame_tick_hooks}},
      {"HotkeyHooks", {InstallHotkeyHooks, &cfg.installHotkeyHooks}},
      {"LiveDebugHooks", {InstallLiveDebugHooks, &install_live_debug_hooks}},
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
      {"RefineryDiagnosticsHooks", {InstallRefineryDiagnosticsHooks, &install_refinery_diagnostics_hooks}},
#endif
#if _WIN32
      {"FreeResizeHooks", {InstallFreeResizeHooks, &cfg.installFreeResizeHooks}},
#endif
      {"TempCrashFixes", {InstallTempCrashFixes, &cfg.installTempCrashFixes}},
      {"TestPatches", {InstallTestPatches, &cfg.installTestPatches}},
      {"MiscPatches", {InstallMiscPatches, &cfg.installMiscPatches}},
      {"ChatPatches", {InstallChatPatches, &cfg.installChatPatches}},
      {"ResolutionListFix", {InstallResolutionListFix, &cfg.installResolutionListFix}},
      {"SyncPatches", {InstallSyncPatches, &cfg.installSyncPatches}},
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
      {"ObjectTracker", {InstallObjectTrackers, &cfg.installObjectTracker}},
#endif
      {"FleetArrival", {InstallFleetArrivalHooks, &cfg.installFleetArrivalHooks}},
      {"LoadingScreenBgHooks", {InstallLoadingScreenBgHooks, &cfg.installLoadingScreenBgHooks}},
  };
  printf("il2cpp_init_hook(%s)\n", domain_name);

  auto r = original(domain_name);

  auto patch_count = 0;
  auto patch_total = sizeof(patches) / sizeof(patches[0]);

  for (const auto& patch : patches) {
    patch_count++;
    const auto [patch_func, patch_enabled] = patch.fnAndEnabled;
    const auto patch_allowed_by_isolation =
        !kLiveDebugOnlyHookIsolation || std::strcmp(patch.name, "LiveDebugHooks") == 0
        || std::strcmp(patch.name, "ObjectTracker") == 0 || std::strcmp(patch.name, "FleetArrival") == 0
        || std::strcmp(patch.name, "FrameTickHooks") == 0 || std::strcmp(patch.name, "HotkeyHooks") == 0;
    const auto patch_install = patch_allowed_by_isolation && (patch_enabled && *patch_enabled);
    const auto patch_mode    = patch_install ? "+ Patch" : "x Skipp";
    spdlog::info(" {}ing {:>2} of {} ({})", patch_mode, patch_count, patch_total, patch.name);

    if (patch_install) {
      patch_func();
    }
  }

  spdlog::info("");

#if VERSION_PATCH
  spdlog::info("Installed beta version {}.{}.{} (Patch {})", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
               VERSION_PATCH);
#else
  spdlog::info("Installed release version {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
#endif

  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");

  return r;
}

// ─── Patch Entry Point ───────────────────────────────────────────────────────

/**
 * @brief Loads GameAssembly and detours il2cpp_init.
 *
 * Platform-specific: on Windows loads GameAssembly.dll via LoadLibrary;
 * on macOS loads GameAssembly.dylib via dlopen relative to the executable.
 * Once loaded, resolves the il2cpp_init export and installs the main hook
 * (il2cpp_init_hook) which in turn bootstraps all patch modules.
 */
void ApplyPatches()
{
  static std::once_flag shutdown_registration;
  std::call_once(shutdown_registration, [] { std::atexit(ShutdownPatches); });

#if _WIN32
  auto assembly = LoadLibraryA("GameAssembly.dll");
#else
  char     buf[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  _NSGetExecutablePath(buf, &bufsize);

  char assembly_path[PATH_MAX];
  snprintf(assembly_path, sizeof(assembly_path), "%s/%s", dirname(buf), "../Frameworks/GameAssembly.dylib");
  printf("Loading %s\n", assembly_path);
  auto assembly = dlopen(assembly_path, RTLD_LAZY | RTLD_GLOBAL);

  init_il2cpp_pointers();
#endif

  if (assembly == nullptr) {
    spdlog::error("Failed to load GameAssembly");
    return;
  } else {
    try {
#if _WIN32
      auto n = GetProcAddress(assembly, "il2cpp_init");
#else
      auto n = dlsym(assembly, "il2cpp_init");
#endif
      printf("Got il2cpp_init %p\n", n);
      if (!n) {
        spdlog::error("Failed to resolve il2cpp_init; community patch hooks were not installed");
        return;
      }

      SPUD_STATIC_DETOUR(n, il2cpp_init_hook);
    } catch (const std::exception& exception) {
      LogRootHookInstallFailure(std::string{"Failed to install il2cpp_init hook: "} + exception.what());
    } catch (...) {
      LogRootHookInstallFailure("Failed to install il2cpp_init hook: unknown exception");
    }
  }
}

void ShutdownPatches()
{
  ShutdownSyncPayloadWorkers();
  ShutdownSyncSchedulerWorker();
  ShutdownCombatLogWorker();
  sidecar_local_ingest::Shutdown();
  notification_shutdown();
  http::shutdown_workers();
}
