/**
 * @file fleet_notifications.h
 * @brief Fleet notification runtime logic independent from hook installation.
 *
 * The hook layer captures live fleet-state and mining-viewer events, then hands
 * those observations to this module. This file contains the notification state
 * machine and message formatting, while the `parts/` layer stays limited to
 * IL2CPP method discovery and hook injection.
 */
#pragma once

#include "patches/signals.h"

#include <array>
#include <cstdint>
#include <string_view>

struct FleetPlayerData;

struct FleetNotificationRuntimeScanResult {
  int                     observed_count          = 0;
  int                     follow_through_count    = 0;
  int                     observed_fleet_id_count = 0;
  std::array<uint64_t, 8> observed_fleet_ids{};
  std::array<int, 7>      followed_state_counts{};
};

/**
 * @brief Initialize notification dependencies used by fleet notifications.
 */
void fleet_notifications_init();

/**
 * @brief True when runtime deployment-event observation is worth installing for fleet notifications.
 */
bool fleet_notifications_runtime_events_enabled();

/**
 * @brief Observe a fleet-state refresh, emit notifications, and report a meaningful runtime trigger source.
 * @param fleet The fleet currently bound to the widget.
 * @param observation_source Stable diagnostic name for the observation seam.
 * @return A high-signal runtime trigger source name for meaningful state transitions, or nullptr.
 */
const char* fleet_notifications_observe_fleet_state(FleetPlayerData* fleet, std::string_view observation_source);

/**
 * @brief Observe current FleetsManager state for all fleet slots and feed the fleet notification state machine.
 * @return Observed slot count and the number still requiring transition follow-through.
 */
FleetNotificationRuntimeScanResult fleet_notifications_observe_runtime_fleets(uint64_t diagnostic_scan_id = 0);

/**
 * @brief Run the throttled frame subscriber that observes fleets independently of Fleet Bar visibility.
 */
void fleet_notifications_tick();

/**
 * @brief Suspend frame scanning after a runtime access failure until a safe event or widget observation rearms it.
 */
void fleet_notifications_suspend_runtime_scan();

/**
 * @brief Observe a mining node depletion event for a fleet.
 * @param fleetId Stable fleet id provided by the game's observer callback.
 */
void fleet_notifications_observe_node_depleted(int64_t fleetId);

/**
 * @brief Emit an incoming attack notification using a known target fleet id when available.
 */
void fleet_notifications_notify_incoming_attack_target(const ToastFleetQueueNotificationsSignal& signal);
void fleet_notifications_notify_incoming_attack_target(const char* source, uint64_t targetFleetId, int targetType,
                                                       int              attackerFleetType = 0,
                                                       std::string_view attackerIdentity  = {});

/**
 * @brief Observe the current mining ETA from the mining viewer.
 * @param selectedFleet The fleet currently bound to the mining viewer.
 * @param remainingTicks Remaining time in .NET TimeSpan ticks.
 */
void fleet_notifications_observe_mining_timer(FleetPlayerData* selectedFleet, int64_t remainingTicks);
