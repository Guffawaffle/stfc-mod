/**
 * @file fleet_notifications.h
 * @brief Fleet notification runtime logic independent from hook installation.
 *
 * The hook layer captures live fleet-bar and mining-viewer events, then hands
 * those observations to this module. This file contains the notification state
 * machine and message formatting, while the `parts/` layer stays limited to
 * IL2CPP method discovery and hook injection.
 */
#pragma once

#include "patches/signals.h"

#include <cstdint>
#include <string_view>

struct FleetPlayerData;

/**
 * @brief Initialize notification dependencies used by fleet notifications.
 */
void fleet_notifications_init();

/**
 * @brief True when runtime deployment-event observation is worth installing for fleet notifications.
 */
bool fleet_notifications_runtime_events_enabled();

/**
 * @brief Observe a fleet-bar state refresh, emit notifications, and report a meaningful runtime trigger source.
 * @param fleet The fleet currently bound to the widget.
 * @return A high-signal runtime trigger source name for meaningful state transitions, or nullptr.
 */
const char* fleet_notifications_observe_fleet_bar(FleetPlayerData* fleet);

/**
 * @brief Observe current FleetsManager state for all fleet slots and feed the fleet notification state machine.
 */
void fleet_notifications_observe_runtime_fleets();

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
													   int attackerFleetType = 0, std::string_view attackerIdentity = {});

/**
 * @brief Observe the current mining ETA from the mining viewer.
 * @param selectedFleet The fleet currently bound to the mining viewer.
 * @param remainingTicks Remaining time in .NET TimeSpan ticks.
 */
void fleet_notifications_observe_mining_timer(FleetPlayerData* selectedFleet, int64_t remainingTicks);
