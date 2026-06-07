#include "patches/action_queue_repair_config.h"

#include "defaultconfig.h"

KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config)
{
  KirsharaQueueRepairConfigParseResult result;

  const auto enabled = config_schema::read_bool(config, {kKirsharaQueueRepairEnabledPath,
                                                         DefaultConfig::Advanced::KirsharaQueue::enabled,
                                                         {},
                                                         "enable the Kir'shara queued-combat advancement repair"});
  const auto course_target_completion = config_schema::read_bool(
      config, {kKirsharaQueueRepairCourseTargetCompletionPath,
               DefaultConfig::Advanced::KirsharaQueue::course_target_completion,
               {},
               "synthesize the missing ProcessQueue target commit for off-screen queued combat"});
  const auto diagnostics_enabled =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsEnabledPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::enabled,
                                        {},
                                        "enable Kir'shara queue marker diagnostics"});
  const auto dump_interesting_methods =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsDumpInterestingMethodsPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::dump_interesting_methods,
                                        {},
                                        "dump current ActionQueueManager method metadata at boot"});
  const auto on_strike_complete =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsOnStrikeCompletePath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::on_strike_complete,
                                        {},
                                        "install the OnStrikeComplete Kir'shara marker"});
  const auto remove_target_and_attack_next = config_schema::read_bool(
      config, {kKirsharaQueueDiagnosticsRemoveTargetAndAttackNextPath,
               DefaultConfig::Advanced::Diagnostics::KirsharaQueue::remove_target_and_attack_next,
               {},
               "install the RemoveTargetAndAttackNext Kir'shara marker"});
  const auto check_to_clear_action_queue = config_schema::read_bool(
      config, {kKirsharaQueueDiagnosticsCheckToClearActionQueuePath,
               DefaultConfig::Advanced::Diagnostics::KirsharaQueue::check_to_clear_action_queue,
               {},
               "install the CheckToClearActionQueue Kir'shara marker"});
  const auto is_target_valid =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsIsTargetValidPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::is_target_valid,
                                        {},
                                        "install the IsTargetValid Kir'shara marker"});
  const auto process_queue_deployed =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsProcessQueueDeployedPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::process_queue_deployed,
                                        {},
                                        "install the ProcessQueue(FleetDeployedData, bool) Kir'shara marker"});
  const auto process_queue_target =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsProcessQueueTargetPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::process_queue_target,
                                        {},
                                        "install the ProcessQueue(Int64, bool) Kir'shara marker"});
  const auto on_set_course_response =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsOnSetCourseResponsePath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::on_set_course_response,
                                        {},
                                        "install the OnSetCourseResponse Kir'shara marker"});
  const auto on_player_fleet_state_changed = config_schema::read_bool(
      config, {kKirsharaQueueDiagnosticsOnPlayerFleetStateChangedPath,
               DefaultConfig::Advanced::Diagnostics::KirsharaQueue::on_player_fleet_state_changed,
               {},
               "install the OnPlayerFleetStateChanged Kir'shara marker"});
  const auto on_fleet_state_change =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsOnFleetStateChangePath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::on_fleet_state_change,
                                        {},
                                        "install the OnFleetStateChange Kir'shara marker"});
  const auto on_fleets_disposed =
      config_schema::read_bool(config, {kKirsharaQueueDiagnosticsOnFleetsDisposedPath,
                                        DefaultConfig::Advanced::Diagnostics::KirsharaQueue::on_fleets_disposed,
                                        {},
                                        "install the OnFleetsDisposed Kir'shara marker"});

  result.config.enabled                                   = enabled.value;
  result.config.course_target_completion                  = course_target_completion.value;
  result.config.diagnostics.enabled                       = diagnostics_enabled.value;
  result.config.diagnostics.dump_interesting_methods      = dump_interesting_methods.value;
  result.config.diagnostics.on_strike_complete            = on_strike_complete.value;
  result.config.diagnostics.remove_target_and_attack_next = remove_target_and_attack_next.value;
  result.config.diagnostics.check_to_clear_action_queue   = check_to_clear_action_queue.value;
  result.config.diagnostics.is_target_valid               = is_target_valid.value;
  result.config.diagnostics.process_queue_deployed        = process_queue_deployed.value;
  result.config.diagnostics.process_queue_target          = process_queue_target.value;
  result.config.diagnostics.on_set_course_response        = on_set_course_response.value;
  result.config.diagnostics.on_player_fleet_state_changed = on_player_fleet_state_changed.value;
  result.config.diagnostics.on_fleet_state_change         = on_fleet_state_change.value;
  result.config.diagnostics.on_fleets_disposed            = on_fleets_disposed.value;

  result.diagnostics = enabled.diagnostics;
  result.diagnostics.insert(result.diagnostics.end(), course_target_completion.diagnostics.begin(),
                            course_target_completion.diagnostics.end());
  const config_schema::BoolReadResult* diagnostic_reads[] = {
      &diagnostics_enabled,           &dump_interesting_methods,    &on_strike_complete,
      &remove_target_and_attack_next, &check_to_clear_action_queue, &is_target_valid,
      &process_queue_deployed,        &process_queue_target,        &on_set_course_response,
      &on_player_fleet_state_changed, &on_fleet_state_change,       &on_fleets_disposed,
  };
  for (const auto* read : diagnostic_reads) {
    result.diagnostics.insert(result.diagnostics.end(), read->diagnostics.begin(), read->diagnostics.end());
  }
  return result;
}

void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config)
{
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairEnabledPath, config.enabled);
  config_schema::write_bool(runtime_config, kKirsharaQueueRepairCourseTargetCompletionPath,
                            config.course_target_completion);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsEnabledPath, config.diagnostics.enabled);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsDumpInterestingMethodsPath,
                            config.diagnostics.dump_interesting_methods);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsOnStrikeCompletePath,
                            config.diagnostics.on_strike_complete);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsRemoveTargetAndAttackNextPath,
                            config.diagnostics.remove_target_and_attack_next);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsCheckToClearActionQueuePath,
                            config.diagnostics.check_to_clear_action_queue);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsIsTargetValidPath,
                            config.diagnostics.is_target_valid);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsProcessQueueDeployedPath,
                            config.diagnostics.process_queue_deployed);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsProcessQueueTargetPath,
                            config.diagnostics.process_queue_target);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsOnSetCourseResponsePath,
                            config.diagnostics.on_set_course_response);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsOnPlayerFleetStateChangedPath,
                            config.diagnostics.on_player_fleet_state_changed);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsOnFleetStateChangePath,
                            config.diagnostics.on_fleet_state_change);
  config_schema::write_bool(runtime_config, kKirsharaQueueDiagnosticsOnFleetsDisposedPath,
                            config.diagnostics.on_fleets_disposed);
}

KirsharaQueueRepairInstallPlan BuildKirsharaQueueRepairInstallPlan(const KirsharaQueueRepairConfig& config,
                                                                   const bool detailed_runtime_trace)
{
  if (!config.enabled) {
    return {};
  }

  const auto selected_hook_count = static_cast<int>(config.course_target_completion);

  return {
      .install_repair_hooks              = selected_hook_count > 0,
      .emit_probe_logs                   = detailed_runtime_trace && selected_hook_count > 0,
      .install_course_target_completion  = config.course_target_completion,
      .install_dump_interesting_methods  = config.diagnostics.enabled && config.diagnostics.dump_interesting_methods,
      .install_on_strike_complete_marker = config.diagnostics.enabled && config.diagnostics.on_strike_complete,
      .install_remove_target_and_attack_next_marker =
          config.diagnostics.enabled && config.diagnostics.remove_target_and_attack_next,
      .install_check_to_clear_action_queue_marker =
          config.diagnostics.enabled && config.diagnostics.check_to_clear_action_queue,
      .install_is_target_valid_marker        = config.diagnostics.enabled && config.diagnostics.is_target_valid,
      .install_process_queue_deployed_marker = config.diagnostics.enabled && config.diagnostics.process_queue_deployed,
      .install_process_queue_target_marker   = config.diagnostics.enabled && config.diagnostics.process_queue_target,
      .install_on_set_course_response_marker = config.diagnostics.enabled && config.diagnostics.on_set_course_response,
      .install_on_player_fleet_state_changed_marker =
          config.diagnostics.enabled && config.diagnostics.on_player_fleet_state_changed,
      .install_on_fleet_state_change_marker = config.diagnostics.enabled && config.diagnostics.on_fleet_state_change,
      .install_on_fleets_disposed_marker    = config.diagnostics.enabled && config.diagnostics.on_fleets_disposed,
      .selected_hook_count                  = selected_hook_count,
  };
}
