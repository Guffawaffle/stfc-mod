#include <doctest/doctest.h>

#include "patches/action_queue_repair_config.h"

namespace
{
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
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("Kirshara queue repair parses focused advanced flag")
  {
    const auto config = toml::parse(R"(
[advanced.kirshara_queue]
enabled = true
)");

    const auto result = ParseKirsharaQueueRepairConfig(config);

    CHECK(result.config.enabled);
    CHECK(result.diagnostics.empty());
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

    WriteKirsharaQueueRepairRuntimeSnapshot(runtime_snapshot, {.enabled = true});

    CHECK(runtime_snapshot["advanced"]["kirshara_queue"]["enabled"].value<bool>().value_or(false));
  }

  TEST_CASE("Kirshara queue repair install plan is a true no-op when disabled")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(false, true);

    CHECK_FALSE(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
  }

  TEST_CASE("Kirshara queue repair install plan installs only repair hooks when enabled")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(true, false);

    CHECK(plan.install_repair_hooks);
    CHECK_FALSE(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
    CHECK(kKirsharaQueueRepairHookCount == 3);
  }

  TEST_CASE("Kirshara queue repair trace mode emits logs without diagnostic detours")
  {
    const auto plan = BuildKirsharaQueueRepairInstallPlan(true, true);

    CHECK(plan.install_repair_hooks);
    CHECK(plan.emit_probe_logs);
    CHECK_FALSE(plan.install_diagnostic_hooks);
  }
}
