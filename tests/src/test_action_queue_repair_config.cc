#include <doctest/doctest.h>

#include "patches/action_queue_repair_config.h"

namespace
{
KirsharaQueueDiagnosticsConfig all_diagnostics_config(bool enabled = true)
{
  return {
      .enabled                       = enabled,
      .dump_interesting_methods      = true,
      .on_strike_complete            = true,
      .remove_target_and_attack_next = true,
      .check_to_clear_action_queue   = true,
      .is_target_valid               = true,
      .process_queue_deployed        = true,
      .process_queue_target          = true,
      .on_set_course_response        = true,
      .on_player_fleet_state_changed = true,
      .on_fleet_state_change         = true,
      .on_fleets_disposed            = true,
  };
}

KirsharaQueueRepairConfig staged_repair_config(bool enabled, bool try_plan_path_and_engage_target, bool handle_stall,
                                               bool remove_action_from_queue, bool course_target_completion = false,
                                               KirsharaQueueDiagnosticsConfig diagnostics = {})
{
  return {
      .enabled                         = enabled,
      .try_plan_path_and_engage_target = try_plan_path_and_engage_target,
      .course_target_completion        = course_target_completion,
      .handle_stall                    = handle_stall,
      .remove_action_from_queue        = remove_action_from_queue,
      .diagnostics                     = diagnostics,
  };
}

bool has_warning(const std::vector<config_schema::Diagnostic>& diagnostics, const std::string_view path)
{
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.path == path && diagnostic.severity == config_schema::DiagnosticSeverity::Warning) {
      return true;
    }
  }

  return false;
}

void check_no_marker_hooks(const KirsharaQueueRepairInstallPlan& plan)
{
  CHECK_FALSE(plan.install_dump_interesting_methods);
  CHECK_FALSE(plan.install_on_strike_complete_marker);
  CHECK_FALSE(plan.install_remove_target_and_attack_next_marker);
  CHECK_FALSE(plan.install_check_to_clear_action_queue_marker);
  CHECK_FALSE(plan.install_is_target_valid_marker);
  CHECK_FALSE(plan.install_process_queue_deployed_marker);
  CHECK_FALSE(plan.install_process_queue_target_marker);
  CHECK_FALSE(plan.install_on_set_course_response_marker);
  CHECK_FALSE(plan.install_on_player_fleet_state_changed_marker);
  CHECK_FALSE(plan.install_on_fleet_state_change_marker);
  CHECK_FALSE(plan.install_on_fleets_disposed_marker);
}

void check_all_marker_hooks(const KirsharaQueueRepairInstallPlan& plan)
{
  CHECK(plan.install_dump_interesting_methods);
  CHECK(plan.install_on_strike_complete_marker);
  CHECK(plan.install_remove_target_and_attack_next_marker);
  CHECK(plan.install_check_to_clear_action_queue_marker);
  CHECK(plan.install_is_target_valid_marker);
  CHECK(plan.install_process_queue_deployed_marker);
  CHECK(plan.install_process_queue_target_marker);
  CHECK(plan.install_on_set_course_response_marker);
  CHECK(plan.install_on_player_fleet_state_changed_marker);
  CHECK(plan.install_on_fleet_state_change_marker);
  CHECK(plan.install_on_fleets_disposed_marker);
}
} // namespace

TEST_SUITE("action_queue_repair_config")
{
  TEST_CASE("Kirshara queue repair defaults disabled")
  {
    const toml::table config;

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK_FALSE(result.config.enabled);
    CHECK_FALSE(result.config.try_plan_path_and_engage_target);
    CHECK_FALSE(result.config.course_target_completion);
    CHECK_FALSE(result.config.handle_stall);
    CHECK_FALSE(result.config.remove_action_from_queue);
    CHECK_FALSE(result.config.diagnostics.enabled);
    CHECK_FALSE(result.config.diagnostics.dump_interesting_methods);
    CHECK_FALSE(result.config.diagnostics.on_strike_complete);
    CHECK_FALSE(result.config.diagnostics.remove_target_and_attack_next);
    CHECK_FALSE(result.config.diagnostics.check_to_clear_action_queue);
    CHECK_FALSE(result.config.diagnostics.is_target_valid);
    CHECK_FALSE(result.config.diagnostics.process_queue_deployed);
    CHECK_FALSE(result.config.diagnostics.process_queue_target);
    CHECK_FALSE(result.config.diagnostics.on_set_course_response);
    CHECK_FALSE(result.config.diagnostics.on_player_fleet_state_changed);
    CHECK_FALSE(result.config.diagnostics.on_fleet_state_change);
    CHECK_FALSE(result.config.diagnostics.on_fleets_disposed);
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("Kirshara queue repair parses focused repair and diagnostic flags")
  {
    const auto config = toml::parse(R"(
[advanced.kirshara_queue]
enabled = true
try_plan_path_and_engage_target = true
course_target_completion = true
handle_stall = true
remove_action_from_queue = true

[advanced.diagnostics.kirshara_queue]
enabled = true
dump_interesting_methods = true
on_strike_complete = true
remove_target_and_attack_next = true
check_to_clear_action_queue = true
is_target_valid = true
process_queue_deployed = true
process_queue_target = true
on_set_course_response = true
on_player_fleet_state_changed = true
on_fleet_state_change = true
on_fleets_disposed = true
)");

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK(result.config.enabled);
    CHECK(result.config.try_plan_path_and_engage_target);
    CHECK(result.config.course_target_completion);
    CHECK(result.config.handle_stall);
    CHECK(result.config.remove_action_from_queue);
    CHECK(result.config.diagnostics.enabled);
    CHECK(result.config.diagnostics.dump_interesting_methods);
    CHECK(result.config.diagnostics.on_strike_complete);
    CHECK(result.config.diagnostics.remove_target_and_attack_next);
    CHECK(result.config.diagnostics.check_to_clear_action_queue);
    CHECK(result.config.diagnostics.is_target_valid);
    CHECK(result.config.diagnostics.process_queue_deployed);
    CHECK(result.config.diagnostics.process_queue_target);
    CHECK(result.config.diagnostics.on_set_course_response);
    CHECK(result.config.diagnostics.on_player_fleet_state_changed);
    CHECK(result.config.diagnostics.on_fleet_state_change);
    CHECK(result.config.diagnostics.on_fleets_disposed);
    CHECK(has_warning(result.diagnostics, kKirsharaQueueRepairTryPlanPathAndEngageTargetPath));
  }

  TEST_CASE("legacy advanced queue gate does not enable Kirshara queue repair")
  {
    const auto config = toml::parse(R"(
[advanced.queue]
queue_repair_enabled = true
)");

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK_FALSE(result.config.enabled);
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("Kirshara queue repair rejects invalid flag types")
  {
    const auto config = toml::parse(R"(
[advanced.kirshara_queue]
enabled = "yes"
)");

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK_FALSE(result.config.enabled);
    CHECK(has_warning(result.diagnostics, kKirsharaQueueRepairEnabledPath));
  }

  TEST_CASE("Kirshara queue repair writes runtime snapshot under focused namespaces")
  {
    toml::table runtime_snapshot;

    WriteKirsharaQueueRepairRuntimeSnapshot(
        runtime_snapshot, staged_repair_config(true, true, true, true, true, all_diagnostics_config()));

    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["enabled"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["try_plan_path_and_engage_target"].value<bool>().value_or(
        false));
    CHECK_FALSE(runtime_snapshot["advanced"]["kirshara_queue"].as_table()->contains("completion_markers"));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["course_target_completion"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["handle_stall"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["remove_action_from_queue"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["enabled"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["dump_interesting_methods"]
              .value<bool>()
              .value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["on_strike_complete"].value<bool>().value_or(
        false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["remove_target_and_attack_next"]
              .value<bool>()
              .value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["check_to_clear_action_queue"]
              .value<bool>()
              .value_or(false));
    CHECK(
        runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["is_target_valid"].value<bool>().value_or(false));
    CHECK(
        runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["process_queue_deployed"].value<bool>().value_or(
            false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["process_queue_target"].value<bool>().value_or(
        false));
    CHECK(
        runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["on_set_course_response"].value<bool>().value_or(
            false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["on_player_fleet_state_changed"]
              .value<bool>()
              .value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["on_fleet_state_change"].value<bool>().value_or(
        false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["kirshara_queue"]["on_fleets_disposed"].value<bool>().value_or(
        false));
  }

  TEST_CASE("Kirshara queue repair install plan is a true no-op when disabled")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(
        staged_repair_config(false, true, true, true, true, all_diagnostics_config()), true);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan stays inert when master gate is on with no hooks selected")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan requires diagnostic master gate for markers")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(
        staged_repair_config(true, false, false, false, false, all_diagnostics_config(false)), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue diagnostics can enable one marker independently")
  {
    KirsharaQueueDiagnosticsConfig diagnostics;
    diagnostics.enabled                = true;
    diagnostics.process_queue_deployed = true;

    const auto plan =
        BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false, false, diagnostics), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.install_process_queue_target_marker);
    CHECK(plan.install_process_queue_deployed_marker);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue diagnostics can enable all markers independently")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(
        staged_repair_config(true, false, false, false, false, all_diagnostics_config()), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    check_all_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan can stage course target completion without marker flags")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false, true), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan ignores unsafe TryPlanPathAndEngageTarget requests")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, true, false, false), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan can stage only HandleStall")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, true, false), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan can stage only RemoveActionFromQueue")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, true), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan stages HandleStall while ignoring unsafe TryPlan")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, true, true, false), true);

    CHECK(plan.install_repair_hooks);
    CHECK(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    check_no_marker_hooks(plan);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan emits logs for all supported staged hooks")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(
        staged_repair_config(true, true, true, true, true, all_diagnostics_config()), true);

    CHECK(plan.install_repair_hooks);
    CHECK(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK(plan.install_remove_action_from_queue);
    check_all_marker_hooks(plan);
    CHECK(plan.selected_hook_count == kKirsharaQueueRepairSupportedHookCount);
  }
}
