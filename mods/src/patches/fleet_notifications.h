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

#include <cstdint>

struct FleetPlayerData;

/**
 * @brief Initialize notification dependencies used by fleet notifications.
 */
void fleet_notifications_init();

/**
 * @brief Observe a fleet-bar state refresh and emit notifications as needed.
 * @param fleet The fleet currently bound to the widget.
 */
void fleet_notifications_observe_fleet_bar(FleetPlayerData* fleet);

/**
 * @brief Observe a mining node depletion event for a fleet.
 * @param fleetId Stable fleet id provided by the game's observer callback.
 */
void fleet_notifications_observe_node_depleted(int64_t fleetId);

/**
 * @brief Emit an incoming attack notification from a native incoming-fleet signal.
 */
void fleet_notifications_notify_incoming_attack_detected(const char* source);

/**
 * @brief Emit an incoming attack notification using a known target fleet id when available.
 */
void fleet_notifications_notify_incoming_attack_target(const char* source, uint64_t targetFleetId, int targetType,
													   int attackerFleetType = 0);

/**
 * @brief Observe the current mining ETA from the mining viewer.
 * @param selectedFleet The fleet currently bound to the mining viewer.
 * @param remainingTicks Remaining time in .NET TimeSpan ticks.
 */
void fleet_notifications_observe_mining_timer(FleetPlayerData* selectedFleet, int64_t remainingTicks);
