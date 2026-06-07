#include "patches/action_queue_repair_config.h"

#include "defaultconfig.h"

KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config)
{
  KirsharaQueueRepairConfigParseResult result;

  const auto enabled = config_schema::read_bool(
      config, {kKirsharaQueueRepairEnabledPath, DefaultConfig::Advanced::KirsharaQueue::enabled, {},
               "enable the Kir'shara queued-combat advancement repair"});
  const auto try_plan_path_and_engage_target = config_schema::read_bool(
      config, {kKirsharaQueueRepairTryPlanPathAndEngageTargetPath,
               DefaultConfig::Advanced::KirsharaQueue::try_plan_path_and_engage_target, {},
               "install the TryPlanPathAndEngageTarget Kir'shara repair hook"});
  const auto completion_markers = config_schema::read_bool(
      config, {kKirsharaQueueRepairCompletionMarkersPath, DefaultConfig::Advanced::KirsharaQueue::completion_markers, {},
               "install narrow completion markers for queued-combat remove-and-advance investigation"});
  const auto course_target_completion = config_schema::read_bool(
      config, {kKirsharaQueueRepairCourseTargetCompletionPath,
               DefaultConfig::Advanced::KirsharaQueue::course_target_completion, {},
               "synthesize the missing ProcessQueue target commit for off-screen queued combat"});
  const auto handle_stall = config_schema::read_bool(
      config,
      {kKirsharaQueueRepairHandleStallPath, DefaultConfig::Advanced::KirsharaQueue::handle_stall, {},
       "install the HandleStall Kir'shara repair hook"});
  const auto remove_action_from_queue = config_schema::read_bool(
      config, {kKirsharaQueueRepairRemoveActionFromQueuePath,
               DefaultConfig::Advanced::KirsharaQueue::remove_action_from_queue, {},
               "install the RemoveActionFromQueue Kir'shara repair hook"});

  result.config.enabled                         = enabled.value;
  result.config.try_plan_path_and_engage_target = try_plan_path_and_engage_target.value;
  result.config.completion_markers              = completion_markers.value;
  result.config.course_target_completion        = course_target_completion.value;
  result.config.handle_stall                    = handle_stall.value;
  result.config.remove_action_from_queue        = remove_action_from_queue.value;
  result.diagnostics                            = enabled.diagnostics;
  result.diagnostics.insert(result.diagnostics.end(), try_plan_path_and_engage_target.diagnostics.begin(),
                            try_plan_path_and_engage_target.diagnostics.end());
  result.diagnostics.insert(result.diagnostics.end(), completion_markers.diagnostics.begin(),
                            completion_markers.diagnostics.end());
  result.diagnostics.insert(result.diagnostics.end(), course_target_completion.diagnostics.begin(),
                            course_target_completion.diagnostics.end());
  result.diagnostics.insert(result.diagnostics.end(), handle_stall.diagnostics.begin(), handle_stall.diagnostics.end());
  result.diagnostics.insert(result.diagnostics.end(), remove_action_from_queue.diagnostics.begin(),
                            remove_action_from_queue.diagnostics.end());
  if (try_plan_path_and_engage_target.value) {
    result.diagnostics.push_back({config_schema::DiagnosticSeverity::Warning,
                                  std::string(kKirsharaQueueRepairTryPlanPathAndEngageTargetPath),
                                  try_plan_path_and_engage_target.source_path.empty()
                                      ? std::string(kKirsharaQueueRepairTryPlanPathAndEngageTargetPath)
                                      : try_plan_path_and_engage_target.source_path,
                                  std::string(kKirsharaQueueRepairTryPlanUnsafeMessage)});
  }
  return result;
}

void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config)
{
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairEnabledPath, config.enabled);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairTryPlanPathAndEngageTargetPath,
                            config.try_plan_path_and_engage_target);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairCompletionMarkersPath, config.completion_markers);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairCourseTargetCompletionPath,
                            config.course_target_completion);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairHandleStallPath, config.handle_stall);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairRemoveActionFromQueuePath,
                            config.remove_action_from_queue);
}

KirsharaQueueRepairInstallPlan BuildKirsharaQueueRepairInstallPlan(const KirsharaQueueRepairConfig& config,
                                                                   const bool                        detailed_runtime_trace)
{
  if (!config.enabled) {
    return {};
  }

  const auto selected_hook_count = static_cast<int>(config.course_target_completion) +
                                   static_cast<int>(config.handle_stall) +
                                   static_cast<int>(config.remove_action_from_queue);

  return {
      .install_repair_hooks                          = selected_hook_count > 0,
      .emit_probe_logs                               = detailed_runtime_trace && selected_hook_count > 0,
      .install_diagnostic_hooks                      = false,
      .install_completion_markers                    = config.completion_markers,
      .install_course_target_completion              = config.course_target_completion,
      .ignore_try_plan_path_and_engage_target_unsafe = config.try_plan_path_and_engage_target,
      .install_handle_stall                          = config.handle_stall,
      .install_remove_action_from_queue              = config.remove_action_from_queue,
      .selected_hook_count                           = selected_hook_count,
  };
}
