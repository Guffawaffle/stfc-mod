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
#include "patches/action_queue_guard_policy.h"
#include "patches/deployment_runtime_observers.h"
#include "patches/fleet_notifications.h"
#include "patches/fleet_runtime_sync.h"
#include "patches/notification_service.h"
#include "patches/patch_install_policy.h"
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
constexpr auto kLegacyLogMaxBytes          = 4 * 1024 * 1024;
constexpr auto kLegacyLogMaxFiles          = 2;

struct LegacyLogResetResult {
  int         removed_count = 0;
  int         failure_count = 0;
  std::string first_failure;
};

std::filesystem::path LegacyRotatedLogPath(const std::filesystem::path& log_path, int index)
{
  if (index <= 0) {
    return log_path;
  }

  const auto stem = log_path.stem().string();
  const auto ext  = log_path.extension().string();
  if (stem.empty()) {
    return std::filesystem::path(log_path.string() + "." + std::to_string(index));
  }

  return log_path.parent_path() / std::filesystem::path(stem + "." + std::to_string(index) + ext);
}

LegacyLogResetResult ResetLegacyLogFiles()
{
  LegacyLogResetResult result;
  const auto           remove_if_present = [&](const std::filesystem::path& path) {
    std::error_code error;
    const auto      removed = std::filesystem::remove(path, error);
    if (removed) {
      ++result.removed_count;
      return;
    }
    if (error) {
      ++result.failure_count;
      if (result.first_failure.empty()) {
        result.first_failure = path.string() + " (" + error.message() + ")";
      }
    }
  };

  const auto log_path = std::filesystem::path(File::Log());
  remove_if_present(log_path);
  for (int index = 1; index <= kLegacyLogMaxFiles; ++index) {
    remove_if_present(LegacyRotatedLogPath(log_path, index));
  }

  return result;
}

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
} // namespace

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
void InstallOpenBulkClaimGiftsHooks();
void InstallMissionHudTweaksHooks();
void InstallSectionChangeRouterHooks();
void InstallActionQueueGuardHooks();
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
void InstallLiveDebugHooks();
#endif
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
void InstallRefineryDiagnosticsHooks();
void InstallClientShipStateProbeHooks();
#endif
void InstallTestPatches();
void InstallMiscPatches();
void InstallChatPatches();
void InstallResolutionListFix();
void InstallTempCrashFixes();
void InstallSyncPatches();
void InstallObjectTrackers();
void InstallFleetArrivalHooks();
void InstallLoadingScreenHooks();
void InstallTransitionScreenHooks();
void InstallLoadingTipHooks();

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
    const char* name;
    const char* config_key;
    const char* enabled_by;
    const char* registry_module;
    void (*install)();
    bool requested;
    bool dependency_enabled;
    bool platform_available;
  };

#if _WIN32
#ifndef NDEBUG
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
#endif
#endif

  File::Init();
  const auto legacy_log_reset = ResetLegacyLogFiles();

  auto file_logger = spdlog::rotating_logger_mt("default", File::Log(), kLegacyLogMaxBytes, kLegacyLogMaxFiles);
  auto sink        = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  file_logger->sinks().push_back(sink);
  spdlog::set_default_logger(file_logger);

  const auto log_level =
      File::hasTrace() ? spdlog::level::trace : (File::hasDebug() ? spdlog::level::debug : spdlog::level::info);

  spdlog::set_level(log_level);
  spdlog::flush_on(log_level);

  spdlog::info("Initializing STFC Community Mod ({})", VER_RUNTIME_VERSION_STR);
  spdlog::info("");
  if (File::hasCustomNames()) {
    spdlog::info("Using custom names");
  } else {
    spdlog::info("Using standard names");
  }

  spdlog::info("  Legacy log reset removed {} file(s)", legacy_log_reset.removed_count);
  if (legacy_log_reset.failure_count > 0) {
    spdlog::warn("  Legacy log reset had {} failure(s); first={}", legacy_log_reset.failure_count,
                 legacy_log_reset.first_failure);
  }

  spdlog::info("  Log: {}", File::Log());
  spdlog::warn("  Local troubleshooting log is legacy-only and bounded to one active plus {} rotated file(s), "
               "{} MiB each (about {} MiB total).",
               kLegacyLogMaxFiles, kLegacyLogMaxBytes / (1024 * 1024),
               (kLegacyLogMaxFiles + 1) * kLegacyLogMaxBytes / (1024 * 1024));
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
  auto install_deployment_runtime_observers = false;
  if (sidecar_local_ingest::FleetRuntimeEnabled()) {
    spdlog::info("[FleetRuntimeSync] sidecar fleet_runtime uses fleet-bar transition requests; deployment event "
                 "observers disabled");
  }
  const auto install_action_queue_guard = action_queue_guard::ShouldInstall(
      AdvancedQueueSettings().thin_queue_protection, AdvancedDiagnosticsSettings().action_queue_guard_logging);
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  auto             install_live_debug_hooks              = LiveDebugChannelEnabled();
  auto             install_refinery_diagnostics_hooks    = RefineryDiagnosticsEnabled();
  auto             install_client_ship_state_probe_hooks = RepairActionStatusProbeEnabled();
#endif
  auto install_open_bulk_claim_gifts_hooks = AutoOpenBulkClaimGiftsEnabled();
  auto install_mission_hud_tweaks_hooks    = MissionHudTweaksEnabled();
  auto install_section_change_router_hooks = install_open_bulk_claim_gifts_hooks;
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  install_section_change_router_hooks = install_section_change_router_hooks || install_refinery_diagnostics_hooks;
#endif
  auto install_frame_tick_hooks = cfg.installHotkeyHooks;
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  install_frame_tick_hooks = install_frame_tick_hooks || LiveDebugChannelEnabled();
#endif
  install_frame_tick_hooks   = install_frame_tick_hooks || fleet_runtime_sync_frame_subscriber_enabled();
  install_frame_tick_hooks   = install_frame_tick_hooks || fleet_notifications_runtime_events_enabled();
  const PatchEntry patches[] = {
      {"UiScaleHooks", "patches.uiscalehooks", "", "", InstallUiScaleHooks, cfg.installUiScaleHooks, false, true},
      {"ZoomHooks", "patches.zoomhooks", "", "ZoomPlanetViewHooks", InstallZoomHooks, cfg.installZoomHooks, false,
       true},
      {"BuffFixHooks", "patches.bufffixhooks", "", "", InstallBuffFixHooks, cfg.installBuffFixHooks, false, true},
      {"ToastBannerHooks", "patches.toastbannerhooks", "", "", InstallToastBannerHooks, cfg.installToastBannerHooks,
       false, true},
      {"PanHooks", "patches.panhooks", "", "", InstallPanHooks, cfg.installPanHooks, false, true},
      {"ImproveResponsivenessHooks", "patches.improveresponsivenesshooks", "", "", InstallImproveResponsivenessHooks,
       cfg.installImproveResponsivenessHooks, false, true},
      {"FrameTickHooks", "", "hotkeys|live-debug|fleet-notifications|fleet-runtime-sync", "FrameTickHooks",
       InstallFrameTickHooks, false, install_frame_tick_hooks, true},
      {"HotkeyHooks", "patches.hotkeyhooks", "", "HotkeyHooks", InstallHotkeyHooks, cfg.installHotkeyHooks, false,
       true},
      {"OpenBulkClaimGiftsHooks", "ui.auto_open_bulk_claim_flyout", "", "OpenBulkClaimGiftsHooks",
       InstallOpenBulkClaimGiftsHooks, install_open_bulk_claim_gifts_hooks, false, true},
      {"MissionHudTweaks", "ui.mission_hud_tweaks", "", "MissionHudTweaks", InstallMissionHudTweaksHooks,
       install_mission_hud_tweaks_hooks, false, true},
      {"DeploymentRuntimeObservers", "", "fleet-runtime-observers", "", InstallDeploymentRuntimeObserverHooks, false,
       install_deployment_runtime_observers, true},
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
      {"LiveDebugHooks", "advanced.diagnostics.live_query", "", "", InstallLiveDebugHooks, install_live_debug_hooks,
       false, true},
#endif
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
      {"RefineryDiagnosticsHooks", "advanced.diagnostics.refinery_diagnostics", "", "RefineryDiagnosticsHooks",
       InstallRefineryDiagnosticsHooks, install_refinery_diagnostics_hooks, false, true},
      {"ClientShipStateProbe", "advanced.diagnostics.ship_state_probe", "", "ClientShipStateProbe",
       InstallClientShipStateProbeHooks, install_client_ship_state_probe_hooks, false, true},
#endif
      {"ActionQueueGuard", "advanced.queue.thin_queue_protection|advanced.diagnostics.action_queue_guard_logging", "",
       "ActionQueueGuard", InstallActionQueueGuardHooks, install_action_queue_guard, false, true},
      {"SectionChangeRouterHooks", "", "bulk-claim|refinery-diagnostics", "SectionChangeRouterHooks",
       InstallSectionChangeRouterHooks, false, install_section_change_router_hooks, true},
#if _WIN32
      {"FreeResizeHooks", "patches.freeresizehooks", "", "", InstallFreeResizeHooks, cfg.installFreeResizeHooks, false,
       true},
#else
      {"FreeResizeHooks", "patches.freeresizehooks", "", "", nullptr, cfg.installFreeResizeHooks, false, false},
#endif
      {"TempCrashFixes", "patches.tempcrashfixes", "", "", InstallTempCrashFixes, cfg.installTempCrashFixes, false,
       true},
      {"TestPatches", "patches.testpatches", "", "", InstallTestPatches, cfg.installTestPatches, false, true},
      {"MiscPatches", "patches.miscpatches", "", "", InstallMiscPatches, cfg.installMiscPatches, false, true},
      {"ChatPatches", "patches.chatpatches", "", "", InstallChatPatches, cfg.installChatPatches, false, true},
      {"ResolutionListFix", "patches.resolutionlistfix", "", "", InstallResolutionListFix, cfg.installResolutionListFix,
       false, true},
      {"SyncPatches", "patches.syncpatches", "", "SyncHooks", InstallSyncPatches, cfg.installSyncPatches, false, true},
      {"ObjectTracker", "patches.objecttracker", "", "ObjectTrackerHooks", InstallObjectTrackers,
       cfg.installObjectTracker, false, true},
      {"FleetArrival", "patches.fleetarrivalhooks", "", "", InstallFleetArrivalHooks, cfg.installFleetArrivalHooks,
       false, true},
      {"LoadingScreen", "patches.loadingscreenhooks", "", "", InstallLoadingScreenHooks, cfg.installLoadingScreenHooks,
       false, true},
      {"TransitionScreen", "patches.transitionscreenhooks", "", "", InstallTransitionScreenHooks,
       cfg.installTransitionScreenHooks, false, true},
      {"LoadingTip", "graphics.loader_tip_enabled", "", "", InstallLoadingTipHooks, cfg.loader_tip_enabled, false,
       true},
  };
  printf("il2cpp_init_hook(%s)\n", domain_name);

  auto r = original(domain_name);

  auto                              patch_count = 0;
  const auto                        patch_total = std::size(patches);
  std::vector<PatchInstallDecision> decisions;
  decisions.reserve(patch_total);

  for (const auto& patch : patches) {
    patch_count++;
    const auto patch_allowed_by_isolation =
        !kLiveDebugOnlyHookIsolation || std::strcmp(patch.name, "DeploymentRuntimeObservers") == 0
        || std::strcmp(patch.name, "LiveDebugHooks") == 0 || std::strcmp(patch.name, "ObjectTracker") == 0
        || std::strcmp(patch.name, "FleetArrival") == 0 || std::strcmp(patch.name, "FrameTickHooks") == 0
        || std::strcmp(patch.name, "HotkeyHooks") == 0;
    const auto decision = build_patch_install_decision({.requested          = patch.requested,
                                                        .dependency_enabled = patch.dependency_enabled,
                                                        .platform_available = patch.platform_available,
                                                        .isolation_allowed  = patch_allowed_by_isolation});
    decisions.push_back(decision);
    const auto patch_mode = decision.effective ? "+ Patch" : "x Skipp";
    spdlog::info(" {}ing {:>2} of {} ({})", patch_mode, patch_count, patch_total, patch.name);
    spdlog::info("[PatchPlan] module={} config={} requested={} dependency={} available={} isolation={} effective={} "
                 "reason={} enabled_by={}",
                 patch.name, patch.config_key, patch.requested, patch.dependency_enabled, patch.platform_available,
                 patch_allowed_by_isolation, decision.effective, patch_install_reason_name(decision.reason),
                 patch.enabled_by);

    if (decision.effective && patch.install) {
      patch.install();
    }
  }

  for (size_t index = 0; index < patch_total; ++index) {
    const auto& patch           = patches[index];
    const auto  registry_backed = patch.registry_module[0] != '\0';
    const auto  hooks           = registry_backed ? hook_install_audit_snapshot(patch.registry_module)
                                                  : HookAuditModuleSnapshot{.module = patch.name};
    const auto  audit           = audit_patch_install(decisions[index], hooks, registry_backed);
    const auto  audit_message =
        fmt::format("[PatchAudit] module={} registry_module={} requested={} effective={} status={} installed={} "
                    "failed={} skipped={} attempted={} total={}",
                    patch.name, patch.registry_module, decisions[index].requested, decisions[index].effective,
                    patch_install_audit_status_name(audit), hooks.installed, hooks.failed, hooks.skipped,
                    hooks.attempted, hooks.total);

    if (audit == PatchInstallAuditStatus::DisabledModuleInstalled || audit == PatchInstallAuditStatus::HookInstallFailed
        || audit == PatchInstallAuditStatus::MissingRegistryEvidence) {
      spdlog::error("{}", audit_message);
    } else {
      spdlog::info("{}", audit_message);
    }
  }

  spdlog::info("");

#if VERSION_PATCH
  spdlog::info("Installed beta version {} (base {}.{}.{} patch {})", VER_RUNTIME_VERSION_STR, VERSION_MAJOR,
               VERSION_MINOR, VERSION_REVISION, VERSION_PATCH);
#else
  spdlog::info("Installed release version {}", VER_RUNTIME_VERSION_STR);
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
  spdlog::shutdown();
}
