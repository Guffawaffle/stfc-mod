/**
 * @file live_debug.h
 * @brief Main-thread live debug/query channel for AX and runtime inspection.
 *
 * The live debug channel polls a JSON request file from the game directory,
 * executes one safe read-only command on the Unity main thread, and writes a
 * JSON response file back for AX or other tooling to consume.
 */
#pragma once

#include <cstdint>
#include <string_view>

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
 * @brief Record station warning controller lifecycle activity and any bound payload.
 */
void live_debug_record_station_warning(std::string_view phase, bool has_context, int target_type,
									   uint64_t target_fleet_id, std::string_view target_user_id,
									   uint64_t quick_scan_target_fleet_id,
									   std::string_view quick_scan_target_id);

/**
 * @brief Record when an incoming-fleet notification payload is materialized before UI binding.
 */
void live_debug_record_incoming_fleet_materialized(std::string_view phase, int target_type,
												   uint64_t target_fleet_id,
												   uint64_t quick_scan_target_fleet_id,
												   std::string_view quick_scan_target_id);

/**
 * @brief Record a pointer-only incoming fleet materialization breadcrumb.
 */
void live_debug_record_incoming_fleet_materialized_pointer(std::string_view phase,
                                                          const void* notification,
                                                          const void* incoming_fleet_params_json);

/**
 * @brief Record ToastFleetObserver producer changes used by incoming attack notifications.
 */
void live_debug_record_toast_fleet_producer(std::string_view phase, const void* observer, int producer_type);

/**
 * @brief Record NavigationInteractionUIViewController lifecycle activity and any bound context.
 */
void live_debug_record_navigation_interaction(std::string_view phase,
										 std::string_view controller_pointer,
										 bool has_context,
										 int context_data_state,
										 int input_interaction_type,
										 std::string_view user_id,
										 bool is_marauder,
										 int threat_level,
										 bool valid_navigation_input,
										 bool show_set_course_arm,
										 int64_t location_translation_id,
										 std::string_view poi_pointer,
										 std::string_view sender_pointer,
										 std::string_view sender_class_namespace,
										 std::string_view sender_class_name,
										 std::string_view callback_context_pointer,
										 std::string_view callback_context_class_namespace,
										 std::string_view callback_context_class_name);

/**
 * @brief Queue a minimal raw-pointer breadcrumb from an experimental navigation hook.
 */
void live_debug_note_navigation_hook(const char* phase,
								 const void* controller,
								 const void* sender = nullptr,
								 const void* callback_context = nullptr);

/**
 * @brief Append a low-level raw trace breadcrumb for navigation hook crash localization.
 */
void live_debug_trace_navigation_hook_step(const char* step,
						  const char* phase,
						  const void* controller = nullptr,
						  const void* sender = nullptr,
						  const void* callback_context = nullptr);

/**
 * @brief Poll and execute at most one live debug request on the main thread.
 * @param screen_manager The current ScreenManager instance from the Update hook.
 */
void live_debug_tick(ScreenManager* screen_manager);