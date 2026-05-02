/**
 * @file fleet_actions.h
 * @brief Ship selection, space actions (engage/scan/recall/repair), and fleet management.
 *
 * Handles all hotkey-driven fleet interactions: selecting ships via number keys
 * (with double-tap-to-locate), executing contextual space actions (engage,
 * scan, mine, recall, repair, warp, queue), and Shift+number tow requests.
 */
#pragma once

#include "prime/FleetBarViewController.h"
#include "prime/HullSpec.h"

#include <cstdint>

struct BattleTargetData;

/** When true, the next frame will re-attempt the primary space action (deferred engage). */
extern bool force_space_action_next_frame;

/** Clear any deferred primary space action. */
void     ClearDeferredSpaceAction();

/** Current deferred action generation, incremented whenever the deferred action changes. */
uint64_t DeferredSpaceActionGeneration();

/**
 * @brief Process a ship selection request from number keys 1-8.
 *
 * Handles Shift+number for tow-to-Discovery, plain number for select (with
 * double-tap-to-locate within the configured timer window).
 *
 * @param ship_select_request 0-based ship index, or -1 if no selection key was pressed.
 * @return true if the selection was handled (caller should skip original).
 */
bool     HandleShipSelection(int ship_select_request);

/**
 * @brief Execute the contextual space action for the currently selected fleet.
 *
 * Inspects visible object viewers and pre-scan widgets to determine the
 * correct action: engage, scan, mine, warp, join armada, add-to-queue, recall,
 * or repair. Supports deferred actions via force_space_action_next_frame.
 *
 * @param fleet_bar The active FleetBarViewController.
 */
void     ExecuteSpaceAction(FleetBarViewController* fleet_bar);

/**
 * @brief Attempt to recall the currently selected fleet.
 * @param fleet_bar The active FleetBarViewController.
 * @return true if recall was successfully requested.
 */
bool     DidExecuteRecall(FleetBarViewController* fleet_bar);

/**
 * @brief Attempt to repair the currently selected fleet.
 * @param fleet_bar The active FleetBarViewController.
 * @return true if repair was successfully requested.
 */
bool     DidExecuteRepair(FleetBarViewController* fleet_bar);

/**
 * @brief Extract the HullType from a BattleTargetData context.
 * @param context The battle target data (may be null).
 * @return The target's hull type, or HullType::Any if context is null/incomplete.
 */
HullType GetHullTypeFromBattleTarget(BattleTargetData* context);
