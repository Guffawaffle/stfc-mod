#include <doctest/doctest.h>

#include "patches/action_queue_repair_config.h"

namespace
{
KirsharaQueueRepairConfig staged_repair_config(bool enabled, bool try_plan_path_and_engage_target, bool handle_stall,
                                               bool remove_action_from_queue, bool completion_markers = false,
                                               bool course_target_completion = false)
{
  return {
      .enabled                         = enabled,
      .try_plan_path_and_engage_target = try_plan_path_and_engage_target,
      .completion_markers              = completion_markers,
      .course_target_completion        = course_target_completion,
      .handle_stall                    = handle_stall,
      .remove_action_from_queue        = remove_action_from_queue,
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
} // namespace

TEST_SUITE("action_queue_repair_config")
{
  TEST_CASE("Kirshara queue repair defaults disabled")
  {
    const toml::table config;

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK_FALSE(result.config.enabled);
    CHECK_FALSE(result.config.try_plan_path_and_engage_target);
    CHECK_FALSE(result.config.completion_markers);
    CHECK_FALSE(result.config.course_target_completion);
    CHECK_FALSE(result.config.handle_stall);
    CHECK_FALSE(result.config.remove_action_from_queue);
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("Kirshara queue repair parses focused advanced flag")
  {
    const auto config = toml::parse(R"(
[advanced.kirshara_queue]
enabled = true
try_plan_path_and_engage_target = true
completion_markers = true
course_target_completion = true
handle_stall = true
remove_action_from_queue = true
)");

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK(result.config.enabled);
    CHECK(result.config.try_plan_path_and_engage_target);
    CHECK(result.config.completion_markers);
    CHECK(result.config.course_target_completion);
    CHECK(result.config.handle_stall);
    CHECK(result.config.remove_action_from_queue);
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

  TEST_CASE("Kirshara queue repair writes runtime snapshot under focused namespace")
  {
    toml::table runtime_snapshot;

    WriteKirsharaQueueRepairRuntimeSnapshot(
        runtime_snapshot, staged_repair_config(true, true, true, true, true, true));

    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["enabled"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["try_plan_path_and_engage_target"].value<bool>().value_or(
        false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["completion_markers"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["course_target_completion"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["handle_stall"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["remove_action_from_queue"].value<bool>().value_or(false));
  }

  TEST_CASE("Kirshara queue repair install plan is a true no-op when disabled")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(false, true, true, true), true);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan stays inert when master gate is on with no hooks selected")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan can enable completion markers without repair hooks")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false, true), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan can stage course target completion")
  {
    const auto plan =
        BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, false, false, true), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan ignores unsafe TryPlanPathAndEngageTarget requests")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, true, false, false), false);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 0);
  }

  TEST_CASE("Kirshara queue repair install plan can stage only HandleStall")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, true, false), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan can stage only RemoveActionFromQueue")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, false, false, true), false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK_FALSE(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK_FALSE(plan.install_handle_stall);
    CHECK(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan stages HandleStall while ignoring unsafe TryPlan")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, true, true, false), true);

    CHECK(plan.install_repair_hooks);
    CHECK(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK_FALSE(plan.install_completion_markers);
    CHECK_FALSE(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK_FALSE(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == 1);
  }

  TEST_CASE("Kirshara queue repair install plan emits logs for all supported staged hooks")
  {
    const auto plan =
        BuildKirsharaQueueRepairInstallPlan(staged_repair_config(true, true, true, true, true, true), true);

    CHECK(plan.install_repair_hooks);
    CHECK(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK(plan.install_completion_markers);
    CHECK(plan.install_course_target_completion);
    CHECK(plan.ignore_try_plan_path_and_engage_target_unsafe);
    CHECK(plan.install_handle_stall);
    CHECK(plan.install_remove_action_from_queue);
    CHECK(plan.selected_hook_count == kKirsharaQueueRepairSupportedHookCount);
  }
}
