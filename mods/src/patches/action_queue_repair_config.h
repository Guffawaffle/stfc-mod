#pragma once

#include "config_schema.h"

#include <string_view>
#include <vector>

#include <toml++/toml.h>

struct KirsharaQueueRepairConfig {
  bool enabled                         = false;
  bool try_plan_path_and_engage_target = false;
  bool completion_markers              = false;
  bool course_target_completion        = false;
  bool handle_stall                    = false;
  bool remove_action_from_queue        = false;
};

struct KirsharaQueueRepairConfigParseResult {
  KirsharaQueueRepairConfig              config;
  std::vector<config_schema::Diagnostic> diagnostics;
};

struct KirsharaQueueRepairInstallPlan {
  bool install_repair_hooks                          = false;
  bool emit_probe_logs                               = false;
  bool install_diagnostic_hooks                      = false;
  bool install_completion_markers                    = false;
  bool install_course_target_completion              = false;
  bool ignore_try_plan_path_and_engage_target_unsafe = false;
  bool install_handle_stall                          = false;
  bool install_remove_action_from_queue              = false;
  int  selected_hook_count                           = 0;
};

constexpr std::string_view kKirsharaQueueRepairEnabledPath                     = "advanced.kirshara_queue.enabled";
constexpr std::string_view kKirsharaQueueRepairTryPlanPathAndEngageTargetPath =
    "advanced.kirshara_queue.try_plan_path_and_engage_target";
constexpr std::string_view kKirsharaQueueRepairCompletionMarkersPath = "advanced.kirshara_queue.completion_markers";
constexpr std::string_view kKirsharaQueueRepairCourseTargetCompletionPath =
    "advanced.kirshara_queue.course_target_completion";
constexpr std::string_view kKirsharaQueueRepairHandleStallPath              = "advanced.kirshara_queue.handle_stall";
constexpr std::string_view kKirsharaQueueRepairRemoveActionFromQueuePath    =
    "advanced.kirshara_queue.remove_action_from_queue";
constexpr std::string_view kKirsharaQueueRepairTryPlanUnsafeMessage =
    "ignoring TryPlanPathAndEngageTarget because the current dump returns IEnumerator; keep this false until a "
    "coroutine-aware investigation lands";
constexpr int kKirsharaQueueRepairSupportedHookCount = 3;

[[nodiscard]] KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config);
void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config);
[[nodiscard]] KirsharaQueueRepairInstallPlan BuildKirsharaQueueRepairInstallPlan(
    const KirsharaQueueRepairConfig& config, bool detailed_runtime_trace);
