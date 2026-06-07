/**
 * @file hostile_observation_probe.cc
 * @brief Retired frame-probed hostile observation surfaces.
 */
#include "patches/hostile_observation_probe.h"

#include "config.h"
#include "patches/hostile_observation_sidecar_sync.h"

#include "prime/ScreenManager.h"

#include <nlohmann/json.hpp>

namespace
{
constexpr int kProbeIntervalMs = 250;
constexpr auto kRetiredReason =
    "UI and interaction hostile observation probes are disabled. FleetDataSystem hooks remain the canonical source.";
}

bool hostile_observation_frame_subscriber_enabled()
{ return false; }

void hostile_observation_tick(ScreenManager* screen_manager)
{
  (void)screen_manager;
}

nlohmann::json hostile_observation_state()
{
  return nlohmann::json{{"enabled", false},
                        {"probeConfigured", SidecarProbesSettings().hostile_observation},
                        {"objectTrackerInstalled", Config::Get().installObjectTracker},
                        {"sidecarTransportReady", hostile_observation_sidecar_delivery_enabled()},
                        {"probeIntervalMs", kProbeIntervalMs},
                        {"retired", true},
                        {"retiredReason", kRetiredReason},
                        {"trackedPreScanWidgetCount", 0},
                        {"trackedNavigationControllerCount", 0},
                        {"visibleHostileCount", 0},
                        {"navigationCandidateCount", 0},
                        {"visibleHostilePreScanTargets", nlohmann::json::array()},
                        {"navigationHostileCandidates", nlohmann::json::array()}};
}
