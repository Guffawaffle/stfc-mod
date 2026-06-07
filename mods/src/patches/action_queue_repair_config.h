#pragma once

#include "config_schema.h"

#include <string_view>
#include <vector>

#include <toml++/toml.h>

struct KirsharaQueueDiagnosticsConfig {
  bool enabled                       = false;
  bool dump_interesting_methods      = false;
  bool on_strike_complete            = false;
  bool remove_target_and_attack_next = false;
  bool check_to_clear_action_queue   = false;
  bool is_target_valid               = false;
  bool process_queue_deployed        = false;
  bool process_queue_target          = false;
  bool on_set_course_response        = false;
  bool on_player_fleet_state_changed = false;
  bool on_fleet_state_change         = false;
  bool on_fleets_disposed            = false;
};

struct KirsharaQueueRepairConfig {
  bool                           enabled                  = false;
  bool                           course_target_completion = false;
  KirsharaQueueDiagnosticsConfig diagnostics;
};

struct KirsharaQueueRepairConfigParseResult {
  KirsharaQueueRepairConfig              config;
  std::vector<config_schema::Diagnostic> diagnostics;
};

struct KirsharaQueueRepairInstallPlan {
  bool install_repair_hooks                         = false;
  bool emit_probe_logs                              = false;
  bool install_course_target_completion             = false;
  bool install_dump_interesting_methods             = false;
  bool install_on_strike_complete_marker            = false;
  bool install_remove_target_and_attack_next_marker = false;
  bool install_check_to_clear_action_queue_marker   = false;
  bool install_is_target_valid_marker               = false;
  bool install_process_queue_deployed_marker        = false;
  bool install_process_queue_target_marker          = false;
  bool install_on_set_course_response_marker        = false;
  bool install_on_player_fleet_state_changed_marker = false;
  bool install_on_fleet_state_change_marker         = false;
  bool install_on_fleets_disposed_marker            = false;
  int  selected_hook_count                          = 0;
};

constexpr std::string_view kKirsharaQueueRepairEnabledPath = "advanced.kirshara_queue.enabled";
constexpr std::string_view kKirsharaQueueRepairCourseTargetCompletionPath =
    "advanced.kirshara_queue.course_target_completion";
constexpr std::string_view kKirsharaQueueDiagnosticsEnabledPath = "advanced.diagnostics.kirshara_queue.enabled";
constexpr std::string_view kKirsharaQueueDiagnosticsDumpInterestingMethodsPath =
    "advanced.diagnostics.kirshara_queue.dump_interesting_methods";
constexpr std::string_view kKirsharaQueueDiagnosticsOnStrikeCompletePath =
    "advanced.diagnostics.kirshara_queue.on_strike_complete";
constexpr std::string_view kKirsharaQueueDiagnosticsRemoveTargetAndAttackNextPath =
    "advanced.diagnostics.kirshara_queue.remove_target_and_attack_next";
constexpr std::string_view kKirsharaQueueDiagnosticsCheckToClearActionQueuePath =
    "advanced.diagnostics.kirshara_queue.check_to_clear_action_queue";
constexpr std::string_view kKirsharaQueueDiagnosticsIsTargetValidPath =
    "advanced.diagnostics.kirshara_queue.is_target_valid";
constexpr std::string_view kKirsharaQueueDiagnosticsProcessQueueDeployedPath =
    "advanced.diagnostics.kirshara_queue.process_queue_deployed";
constexpr std::string_view kKirsharaQueueDiagnosticsProcessQueueTargetPath =
    "advanced.diagnostics.kirshara_queue.process_queue_target";
constexpr std::string_view kKirsharaQueueDiagnosticsOnSetCourseResponsePath =
    "advanced.diagnostics.kirshara_queue.on_set_course_response";
constexpr std::string_view kKirsharaQueueDiagnosticsOnPlayerFleetStateChangedPath =
    "advanced.diagnostics.kirshara_queue.on_player_fleet_state_changed";
constexpr std::string_view kKirsharaQueueDiagnosticsOnFleetStateChangePath =
    "advanced.diagnostics.kirshara_queue.on_fleet_state_change";
constexpr std::string_view kKirsharaQueueDiagnosticsOnFleetsDisposedPath =
    "advanced.diagnostics.kirshara_queue.on_fleets_disposed";
constexpr int kKirsharaQueueRepairSupportedHookCount = 1;

[[nodiscard]] KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config);
void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config);
[[nodiscard]] KirsharaQueueRepairInstallPlan
BuildKirsharaQueueRepairInstallPlan(const KirsharaQueueRepairConfig& config, bool detailed_runtime_trace);
