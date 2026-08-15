#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
std::filesystem::path find_repo_file(const std::string_view relative_path)
{
  auto current = std::filesystem::current_path();
  while (!current.empty()) {
    const auto candidate = current / std::filesystem::path(relative_path);
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }

    if (!current.has_parent_path()) {
      break;
    }

    const auto parent = current.parent_path();
    if (parent == current) {
      break;
    }

    current = parent;
  }

  return {};
}

std::string read_text_file(const std::string_view relative_path)
{
  const auto path = find_repo_file(relative_path);
  REQUIRE_MESSAGE(!path.empty(), "Failed to find " << std::string(relative_path));

  std::ifstream input(path, std::ios::binary);
  REQUIRE_MESSAGE(input.good(), "Failed to open " << path.string());

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool contains(const std::string& text, const std::string_view needle)
{ return text.find(needle) != std::string::npos; }
} // namespace

TEST_CASE("generated native shortcut pointer callback guard family stays quarantined")
{
  const auto hotkeys_source = read_text_file("mods/src/patches/parts/hotkeys.cc");

  CHECK_FALSE(contains(hotkeys_source, "SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS"));
  CHECK_FALSE(contains(hotkeys_source, "OnShipLocateAction"));

  const auto has_generated_callback_guard_install =
      contains(hotkeys_source, "INSTALL_SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARD")
      || contains(hotkeys_source, "HandleNativeShortcutPointerCallback")
      || contains(hotkeys_source, "should_install_native_shortcut_pointer_callback_guards")
      || contains(hotkeys_source, "native_shortcut_pointer_callback_guard_reason")
      || contains(hotkeys_source, "hotkey_router_should_suppress_native_shortcut_callback");

  CHECK_FALSE(has_generated_callback_guard_install);
}

TEST_CASE("fleet runtime sync requests require gameplay dispatch provenance")
{
  const auto sync_header = read_text_file("mods/src/patches/fleet_runtime_sync.h");
  CHECK_FALSE(contains(sync_header, "fleet_runtime_sync_trigger(std::string_view"));
  CHECK_FALSE(contains(sync_header, "fleet_runtime_sync_capture(std::string_view"));

  const auto live_debug_source = read_text_file("mods/src/patches/parts/live_debug.cc");
  CHECK_FALSE(contains(live_debug_source, "fleet_runtime_sync_trigger(\""));
}

TEST_CASE("fleet notifications use one targeted runtime owner and keep broad deployment observers dormant")
{
  const auto fleet_arrival_source = read_text_file("mods/src/patches/parts/fleet_arrival.cc");
  CHECK_FALSE(contains(fleet_arrival_source, "FleetEvents.TriggerPlayerFleetsChangedEvent"));
  CHECK_FALSE(contains(fleet_arrival_source, "FleetEvents_TriggerPlayerFleetsChangedEvent_Hook"));
  CHECK(contains(fleet_arrival_source, "HOOK_REGISTRY_SPUD_STATIC_DETOUR"));
  CHECK(contains(fleet_arrival_source, "fleet_notifications_observe_fleet_state"));

  const auto deployment_observer_source = read_text_file("mods/src/patches/parts/deployment_runtime_observers.cc");
  CHECK_FALSE(contains(deployment_observer_source, "fleet_notifications_observe_runtime_fleets"));

  const auto patch_source = read_text_file("mods/src/patches/patches.cc");
  CHECK(contains(patch_source, "install_deployment_runtime_observers = false"));
  CHECK(contains(patch_source, "fleet_notifications_runtime_events_enabled()"));

  const auto frame_tick_source = read_text_file("mods/src/patches/frame_tick.cc");
  CHECK(contains(frame_tick_source, "fleet_notifications_tick()"));
  CHECK(contains(frame_tick_source, "subscriber=fleet_notifications"));

  const auto fleet_notifications_source = read_text_file("mods/src/patches/fleet_notifications.cc");
  CHECK(contains(fleet_notifications_source, "status=scan-requested"));
  CHECK(contains(fleet_notifications_source, "FleetNotificationScanObservation::Settled"));
  CHECK(contains(fleet_notifications_source, "reason=no-fleets"));
  CHECK(contains(fleet_notifications_source, "reason=max-lifetime"));
}

TEST_CASE("temporary targeted diagnostics remain registered, searchable, and sunset-bound")
{
  const auto registry_source = read_text_file("mods/src/targeted_diagnostic_registry.cc");
  CHECK(contains(registry_source, "TARGET_DIAGNOSTIC_REGISTER(fleet_notification_diagnostics::Concern())"));
  CHECK(contains(registry_source, "TARGET_DIAGNOSTIC_REGISTER(runtime_impact_diagnostics::Concern())"));
  CHECK(contains(registry_source, "TARGET_DIAGNOSTIC_REGISTER(server_transfer_diagnostics::Concern())"));
  CHECK(contains(registry_source, "ValidateConcernSpecs(kSpecs, kCurrentVersion, true)"));

  const auto concern_header = read_text_file("mods/src/patches/fleet_notification_diagnostics.h");
  CHECK(contains(concern_header, "= \"fleet-notification-scan\""));
  CHECK(contains(concern_header, "= \"#255\""));
  CHECK(contains(concern_header, "= {2, 2, 0}"));

  const auto concern_source = read_text_file("mods/src/patches/fleet_notification_diagnostics.cc");
  CHECK(contains(concern_source, "TARGET_DIAGNOSTIC_WRITE"));
  CHECK(contains(concern_source, "TARGET_DIAGNOSTIC_ENABLED"));

  const auto impact_header = read_text_file("mods/src/patches/runtime_impact_diagnostics.h");
  CHECK(contains(impact_header, "= \"runtime-impact\""));
  CHECK(contains(impact_header, "= \"#257\""));
  CHECK(contains(impact_header, "= {2, 2, 0}"));

  const auto impact_source = read_text_file("mods/src/patches/runtime_impact_diagnostics.cc");
  CHECK(contains(impact_source, "TARGET_DIAGNOSTIC_WRITE"));
  CHECK(contains(impact_source, "TARGET_DIAGNOSTIC_ENABLED"));

  const auto transfer_header = read_text_file("mods/src/patches/server_transfer_diagnostics.h");
  CHECK(contains(transfer_header, "= \"server-transfer\""));
  CHECK(contains(transfer_header, "= \"#258\""));
  CHECK(contains(transfer_header, "= {2, 2, 0}"));

  const auto transfer_source = read_text_file("mods/src/patches/server_transfer_diagnostics.cc");
  CHECK(contains(transfer_source, "TARGET_DIAGNOSTIC_WRITE"));
  CHECK(contains(transfer_source, "TARGET_DIAGNOSTIC_ENABLED"));

  const auto transfer_hooks = read_text_file("mods/src/patches/server_transfer_diagnostics_hooks.cc");
  CHECK(contains(transfer_hooks, "HOOK_REGISTRY_SPUD_STATIC_DETOUR"));
  CHECK(contains(transfer_hooks, "HookSupportTier::Science"));
  CHECK_FALSE(contains(transfer_hooks, "GSError.Data"));
  CHECK_FALSE(contains(transfer_hooks, "temporaryTransferReference"));

  const auto producer_source = read_text_file("mods/src/patches/fleet_notifications.cc");
  CHECK(contains(producer_source, "CacheSnapshotDue"));
  CHECK(contains(producer_source, "CountStaleFleetCacheEntries"));
  CHECK(contains(producer_source, "CompleteTick"));
  CHECK_FALSE(contains(producer_source, "kStaleCacheInspectionLimit"));
  CHECK_FALSE(contains(producer_source, "nlohmann"));
}

TEST_CASE("runtime impact diagnostics have no parallel legacy logger or configuration path")
{
  CHECK(find_repo_file("mods/src/runtime_trace_config.h").empty());
  CHECK(find_repo_file("mods/src/patches/fleet_runtime_diagnostics.h").empty());
  CHECK(find_repo_file("mods/src/patches/fleet_runtime_diagnostics.cc").empty());

  const auto config_header = read_text_file("mods/src/config.h");
  const auto defaults      = read_text_file("mods/src/defaultconfig.h");
  const auto example       = read_text_file("example_community_patch_settings.toml");
  for (const auto legacy_setting : {"runtime_trace", "mod_impact_monitor", "fleet_runtime_mode"}) {
    CHECK_FALSE(contains(config_header, legacy_setting));
    CHECK_FALSE(contains(defaults, legacy_setting));
    CHECK_FALSE(contains(example, legacy_setting));
  }

  const auto impact_monitor = read_text_file("mods/src/patches/runtime_impact_monitor.cc");
  const auto fleet_sync     = read_text_file("mods/src/patches/fleet_runtime_sync.cc");
  const auto fleet_arrival  = read_text_file("mods/src/patches/parts/fleet_arrival.cc");
  CHECK(contains(impact_monitor, "runtime_impact_diagnostics::RecordProbeWindow"));
  CHECK_FALSE(contains(impact_monitor, "spdlog::"));
  CHECK_FALSE(contains(fleet_sync, "[FleetRuntimeSync]"));
  CHECK_FALSE(contains(fleet_arrival, "[FleetRuntimeTrigger]"));
}

TEST_CASE("sidecar local enqueue requires copied payload provenance")
{
  const auto sidecar_header = read_text_file("mods/src/patches/sidecar_local_ingest.h");
  CHECK_FALSE(contains(sidecar_header, "EnqueueBattleEvents(const nlohmann::json& events);"));
  CHECK_FALSE(contains(sidecar_header, "EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload);"));
  CHECK(contains(sidecar_header, "const SidecarLocalDispatchContext& context"));

  const auto battle_source = read_text_file("mods/src/patches/sync_battle_logs.cc");
  CHECK_FALSE(contains(battle_source, "EnqueueBattleEvents(sidecar_events);"));

  const auto fleet_source = read_text_file("mods/src/patches/fleet_runtime_sync.cc");
  CHECK_FALSE(contains(fleet_source, "EnqueueFleetRuntimeSnapshot(payload);"));
}
