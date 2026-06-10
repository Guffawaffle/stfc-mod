/**
 * @file frame_tick.cc
 * @brief Coordinates per-frame subscribers from the ScreenManager::Update hook.
 */
#include "config.h"
#include "errormsg.h"

#include <exception>
#include <spdlog/spdlog.h>

#include "patches/frame_tick.h"
#include "patches/fleet_runtime_sync.h"
#include "patches/hook_registry.h"
#include "patches/hotkey_router.h"
#include "patches/live_debug.h"
#include "patches/mod_impact_monitor.h"

#include "prime/ScreenManager.h"

namespace {
constexpr bool kEnableFrameTickHook = true;
constexpr bool kEnableHotkeyFrameSubscriber = true;
constexpr bool kEnableLiveDebugFrameSubscriber = true;
constexpr bool kEnableFleetRuntimeSyncFrameSubscriber = true;

constexpr HookDescriptor kScreenManagerUpdateHook = {
  "ScreenManager.Update",
  "fan out frame ticks to hotkeys, live-debug, and future frame observers",
  {"Assembly-CSharp", "Digit.Client.UI", "ScreenManager", "Update"},
  "frame-driven hotkeys or diagnostics will not tick",
};

bool hotkey_frame_subscriber_enabled()
{
  return kEnableHotkeyFrameSubscriber && Config::Get().installHotkeyHooks;
}

bool live_debug_frame_subscriber_enabled()
{
#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
  return kEnableLiveDebugFrameSubscriber && LiveDebugChannelEnabled();
#else
  return false;
#endif
}

bool fleet_runtime_sync_frame_subscriber_allowed()
{
  return kEnableFleetRuntimeSyncFrameSubscriber && fleet_runtime_sync_frame_subscriber_enabled();
}

void log_frame_tick_subscribers()
{
  spdlog::info("[FrameTick] subscriber=hotkey_router enabled={} reason=installHotkeyHooks compile_time_enabled={}",
               hotkey_frame_subscriber_enabled(),
               kEnableHotkeyFrameSubscriber);
  spdlog::info("[FrameTick] subscriber=live_debug enabled={} reason=live_debug_channel compile_time_enabled={}",
               live_debug_frame_subscriber_enabled(),
               kEnableLiveDebugFrameSubscriber);
  spdlog::info("[FrameTick] subscriber=fleet_runtime_sync enabled={} reason=fleet_runtime compile_time_enabled={}",
               fleet_runtime_sync_frame_subscriber_allowed(),
               kEnableFleetRuntimeSyncFrameSubscriber);
}

void tick_live_debug(ScreenManager* screen_manager)
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::FrameTickLiveDebug, ModImpactMonitorEnabled());

  if (!live_debug_frame_subscriber_enabled()) {
    return;
  }

  try {
    live_debug_tick(screen_manager);
  } catch (const std::exception& ex) {
    spdlog::error("[FrameTick] subscriber=live_debug status=failed error='{}'", ex.what());
  } catch (...) {
    spdlog::error("[FrameTick] subscriber=live_debug status=failed error='unknown exception'");
  }
}

void tick_fleet_runtime_sync()
{
  if (!fleet_runtime_sync_frame_subscriber_allowed()) {
    return;
  }

  try {
    fleet_runtime_sync_process_pending();
  } catch (const std::exception& ex) {
    spdlog::error("[FrameTick] subscriber=fleet_runtime_sync status=failed error='{}'", ex.what());
  } catch (...) {
    spdlog::error("[FrameTick] subscriber=fleet_runtime_sync status=failed error='unknown exception'");
  }
}

bool tick_hotkeys(ScreenManager* screen_manager)
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::FrameTickHotkeys, ModImpactMonitorEnabled());

  if (!hotkey_frame_subscriber_enabled()) {
    return true;
  }

  try {
    const auto router_allows_original = hotkey_router_screen_update(screen_manager);
    return hotkey_router_should_call_original_screen_update(router_allows_original);
  } catch (const std::exception& ex) {
    spdlog::error("[FrameTick] subscriber=hotkey_router status=failed error='{}'", ex.what());
  } catch (...) {
    spdlog::error("[FrameTick] subscriber=hotkey_router status=failed error='unknown exception'");
  }

  return true;
}

void ScreenManager_Update_FrameTick_Hook(auto original, ScreenManager* screen_manager)
{
  ScopedModImpactTimer impact_timer(ModImpactProbe::FrameTickTotal, ModImpactMonitorEnabled());

  const auto should_call_original = tick_hotkeys(screen_manager);
  if (should_call_original) {
    impact_timer.ExcludeCall([&] { original(screen_manager); });
  }

  tick_live_debug(screen_manager);
  tick_fleet_runtime_sync();
}
}

void InstallFrameTickHooks()
{
  HookModuleHealth hooks("FrameTickHooks");
  log_frame_tick_subscribers();

  if (!kEnableFrameTickHook) {
    hooks.record_skipped(kScreenManagerUpdateHook, "compile-time disabled");
    hooks.log_summary();
    return;
  }

  if (!hotkey_frame_subscriber_enabled() && !live_debug_frame_subscriber_enabled()
      && !fleet_runtime_sync_frame_subscriber_allowed()) {
    hooks.record_skipped(kScreenManagerUpdateHook, "no enabled frame subscribers");
    hooks.log_summary();
    return;
  }

  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  if (!screen_manager_helper.isValidHelper()) {
    hooks.record_missing_helper(kScreenManagerUpdateHook);
    hooks.log_summary();
    return;
  }

  auto ptr_update = screen_manager_helper.GetMethod("Update");
  if (ptr_update == nullptr) {
    hooks.record_missing_method(kScreenManagerUpdateHook);
    hooks.log_summary();
    return;
  }

  HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kScreenManagerUpdateHook, ptr_update, ScreenManager_Update_FrameTick_Hook);
  hooks.log_summary();
}
