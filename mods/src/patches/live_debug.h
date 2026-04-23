/**
 * @file live_debug.h
 * @brief Main-thread live debug/query channel for AX and runtime inspection.
 *
 * The live debug channel polls a JSON request file from the game directory,
 * executes one safe read-only command on the Unity main thread, and writes a
 * JSON response file back for AX or other tooling to consume.
 */
#pragma once

struct FleetBarViewController;
struct FleetPlayerData;
struct ScreenManager;

/**
 * @brief Install hook-driven live debug event sources.
 */
void InstallLiveDebugHooks();

/**
 * @brief Record a targeted trace when the space-action warp-cancel path fires.
 */
void live_debug_record_space_action_warp_cancel(FleetBarViewController* fleet_bar, FleetPlayerData* fleet,
												bool has_primary, bool has_secondary, bool has_queue,
												bool has_queue_clear, bool has_recall, bool has_repair,
												bool has_recall_cancel, bool force_space_action,
												int visible_pre_scan_target_count, bool mining_viewer_visible,
												bool star_node_viewer_visible,
												bool navigation_interaction_visible);

/**
 * @brief Record a targeted trace when a warp-cancel input is intentionally suppressed.
 */
void live_debug_record_space_action_warp_cancel_suppressed(FleetBarViewController* fleet_bar,
													 FleetPlayerData* fleet, bool has_primary,
													 bool has_secondary, bool has_queue,
													 bool has_queue_clear, bool has_recall,
													 bool has_repair, bool has_recall_cancel,
													 bool force_space_action,
													 int visible_pre_scan_target_count,
													 bool mining_viewer_visible,
													 bool star_node_viewer_visible,
													 bool navigation_interaction_visible);

/**
 * @brief Poll and execute at most one live debug request on the main thread.
 * @param screen_manager The current ScreenManager instance from the Update hook.
 */
void live_debug_tick(ScreenManager* screen_manager);