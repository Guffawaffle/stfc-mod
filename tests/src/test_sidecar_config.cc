#include <doctest/doctest.h>

#include "config_redaction.h"
#include "config_sidecar.h"
#include "patches/sidecar_local_ingest_policy.h"

#include <string>
#include <vector>

namespace
{
bool has_diagnostic(const std::vector<config_schema::Diagnostic>& diagnostics, std::string_view path,
                    config_schema::DiagnosticSeverity severity)
{
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.path == path && diagnostic.severity == severity) {
      return true;
    }
  }

  return false;
}

bool has_diagnostic_source(const std::vector<config_schema::Diagnostic>& diagnostics, std::string_view path,
                           std::string_view source_path, config_schema::DiagnosticSeverity severity)
{
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.path == path && diagnostic.source_path == source_path && diagnostic.severity == severity) {
      return true;
    }
  }

  return false;
}

bool has_diagnostic_message_fragment(const std::vector<config_schema::Diagnostic>& diagnostics, std::string_view path,
                                     std::string_view fragment)
{
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.path == path && diagnostic.message.find(fragment) != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool has_rejected_target(const std::vector<SidecarRejectedSyncTarget>& rejected_targets, std::string_view target_name)
{
  for (const auto& rejected : rejected_targets) {
    if (rejected.target_name == target_name) {
      return true;
    }
  }

  return false;
}
} // namespace

TEST_SUITE("sidecar_config")
{
  TEST_CASE("sidecar sync producers default off")
  {
    auto config = toml::parse(R"(
[sidecar.sync]
)");

    const auto result = ParseSidecarConfig(config);

    CHECK_FALSE(result.config.sync.enabled);
    CHECK(result.config.sync.transport == "legacy_http");
    CHECK(result.config.sync.pipe_name.empty());
    CHECK_FALSE(result.config.sync.battlelogs_realtime);
    CHECK_FALSE(result.config.sync.battlelog_enrichment);
    CHECK_FALSE(result.config.sync.fleet_runtime);
    CHECK(result.config.sync.fleet_runtime_mode == "normal");
  }

  TEST_CASE("omitting advanced namespaces keeps normal config parsing backward compatible")
  {
    auto config = toml::parse(R"(
[sidecar.sync]
enabled = true
transport = "named_pipe"
pipe_name = "stfc-mod-bridge.battle.v1"
url = "http://127.0.0.1:43127/api/sidecar/ingest"

[sidecar.logging]
jsonl = false
jsonl_replay_seconds = 30
jsonl_recent_logs = 300
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.config.sync.enabled);
    CHECK(result.config.sync.transport == "named_pipe");
    CHECK(result.config.sync.pipe_name == "stfc-mod-bridge.battle.v1");
    CHECK(result.config.sync.url == "http://127.0.0.1:43127/api/sidecar/ingest");
    CHECK_FALSE(result.advanced.diagnostics.ship_identity);
    CHECK_FALSE(result.advanced.diagnostics.battle_log_decoder);
    CHECK_FALSE(result.advanced.diagnostics.battle_catalog);
    CHECK_FALSE(result.advanced.diagnostics.debug);
    CHECK_FALSE(result.advanced.diagnostics.logging);
    CHECK_FALSE(result.advanced.diagnostics.hotkey_suppression_logging);
    CHECK_FALSE(result.advanced.diagnostics.notification_skip_logging);
    CHECK_FALSE(result.advanced.diagnostics.fleet_selection_timing_logging);
    CHECK_FALSE(result.advanced.diagnostics.live_query);
    CHECK(result.advanced.diagnostics.runtime_trace == "off");
    CHECK_FALSE(result.advanced.diagnostics.runtime_trace_track_overhead);
    CHECK_FALSE(result.advanced.diagnostics.action_queue_guard_logging);
    CHECK_FALSE(result.advanced.diagnostics.mod_impact_monitor);
    CHECK(result.advanced.diagnostics.runtime_trace_report_interval_ms == 5000);
    CHECK_FALSE(result.advanced.diagnostics.refinery_diagnostics);
    CHECK(result.advanced.diagnostics.files.root.empty());
    CHECK(result.advanced.diagnostics.files.navhook_trace_max_kb == 4096);
    CHECK(result.advanced.diagnostics.files.navhook_trace_files == 3);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_max_kb == 8192);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_files == 3);
    CHECK_FALSE(result.advanced.queue.queue_repair_enabled);
    CHECK(result.advanced.queue.thin_queue_protection);
    CHECK_FALSE(result.advanced.queue.queue_add_direct_handler);
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("invalid explicit sidecar transport stays invalid and emits an error")
  {
    auto config = toml::parse(R"(
[sidecar.sync]
enabled = true
transport = "Named_Pipe"
pipe_name = "stfc-mod-bridge.battle.v1"
token = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
url = "http://127.0.0.1:43127/api/sidecar/ingest"
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.config.sync.transport == "invalid");
    CHECK(has_diagnostic(result.diagnostics, "sidecar.sync.transport", config_schema::DiagnosticSeverity::Error));
    CHECK_FALSE(SidecarLocalSyncTransportReady(result.config.sync));

    config            = toml::parse("[sidecar.sync]\nenabled = true\ntransport = 1\n");
    const auto typed  = ParseSidecarConfig(config);
    CHECK(typed.config.sync.transport == "invalid");
    CHECK_FALSE(SidecarLocalSyncTransportReady(typed.config.sync));
  }

  TEST_CASE("parses advanced diagnostics including runtime trace, keeps sidecar sync and logging canonical, and "
            "rejects legacy sync jsonl keys")
  {
    auto config = toml::parse(R"(
[sidecar.sync]
enabled = true
url = "http://127.0.0.1:43127/api/sidecar/ingest"
token = "sidecar-token"
proxy = "http://user:pass@example.invalid:8080"
verify_ssl = false
allow_unsafe_tls_without_certificate_validation = true
battlelogs_realtime = true
battlelog_enrichment = true
fleet_runtime = true
fleet_runtime_mode = "snapshot_only"

[advanced.diagnostics]
ship_identity = true
battle_log_decoder = true
battle_catalog = false
debug = true
logging = true
hotkey_suppression_logging = true
notification_skip_logging = true
fleet_selection_timing_logging = true
live_query = true
runtime_trace = "verbose"
runtime_trace_track_overhead = true
action_queue_guard_logging = true
mod_impact_monitor = true
runtime_trace_report_interval_ms = 9000
refinery_diagnostics = true

[advanced.diagnostics.files]
root = "custom/native-logs"
navhook_trace_max_kb = 2048
navhook_trace_files = 5
action_queue_probe_max_kb = 6144
action_queue_probe_files = 6

[advanced.queue]
queue_repair_enabled = true
thin_queue_protection = true
queue_add_direct_handler = true
queue_add_hide_viewers = false

[sidecar.logging]
jsonl = true
jsonl_replay_seconds = 45
jsonl_recent_logs = 12

[sync]
sidecar_jsonl = true
sidecar_jsonl_replay_seconds = 90
sidecar_jsonl_recent_logs = 120
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.config.sync.enabled);
    CHECK(result.config.sync.url == "http://127.0.0.1:43127/api/sidecar/ingest");
    CHECK(result.config.sync.token == "sidecar-token");
    CHECK(result.config.sync.proxy == "http://user:pass@example.invalid:8080");
    CHECK_FALSE(result.config.sync.verify_ssl);
    CHECK(result.config.sync.allow_unsafe_tls_without_certificate_validation);
    CHECK(result.config.sync.battlelogs_realtime);
    CHECK(result.config.sync.battlelog_enrichment);
    CHECK(result.config.sync.fleet_runtime);
    CHECK(result.config.sync.fleet_runtime_mode == "snapshot_only");

    CHECK(result.advanced.diagnostics.ship_identity);
    CHECK(result.advanced.diagnostics.battle_log_decoder);
    CHECK_FALSE(result.advanced.diagnostics.battle_catalog);
    CHECK(result.advanced.diagnostics.debug);
    CHECK(result.advanced.diagnostics.logging);
    CHECK(result.advanced.diagnostics.hotkey_suppression_logging);
    CHECK(result.advanced.diagnostics.notification_skip_logging);
    CHECK(result.advanced.diagnostics.fleet_selection_timing_logging);
    CHECK(result.advanced.diagnostics.live_query);
    CHECK(result.advanced.diagnostics.runtime_trace == "verbose");
    CHECK(result.advanced.diagnostics.runtime_trace_track_overhead);
    CHECK(result.advanced.diagnostics.action_queue_guard_logging);
    CHECK(result.advanced.diagnostics.mod_impact_monitor);
    CHECK(result.advanced.diagnostics.runtime_trace_report_interval_ms == 9000);
    CHECK(result.advanced.diagnostics.refinery_diagnostics);
    CHECK(result.advanced.diagnostics.files.root == "custom/native-logs");
    CHECK(result.advanced.diagnostics.files.navhook_trace_max_kb == 2048);
    CHECK(result.advanced.diagnostics.files.navhook_trace_files == 5);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_max_kb == 6144);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_files == 6);
    CHECK(result.advanced.queue.queue_repair_enabled);
    CHECK(result.advanced.queue.thin_queue_protection);
    CHECK(result.advanced.queue.queue_add_direct_handler);
    CHECK(has_diagnostic_source(result.diagnostics, "advanced.queue.queue_add_hide_viewers",
                                "advanced.queue.queue_add_hide_viewers", config_schema::DiagnosticSeverity::Warning));

    CHECK(result.config.probes.ship_identity);
    CHECK(result.config.probes.battle_log_decoder);
    CHECK_FALSE(result.config.probes.battle_catalog);

    CHECK(result.config.logging.jsonl);
    CHECK(result.config.logging.jsonl_replay_seconds == 45);
    CHECK(result.config.logging.jsonl_recent_logs == 12);
    CHECK_FALSE(has_diagnostic(result.diagnostics, "advanced.queue", config_schema::DiagnosticSeverity::Warning));

    CHECK(has_diagnostic(result.diagnostics, "sync.sidecar_jsonl", config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.sidecar_jsonl_replay_seconds",
                         config_schema::DiagnosticSeverity::Error));
    CHECK(
        has_diagnostic(result.diagnostics, "sync.sidecar_jsonl_recent_logs", config_schema::DiagnosticSeverity::Error));
  }

  TEST_CASE("advanced diagnostics debug does not enable noisy breadcrumb logging")
  {
    auto config = toml::parse(R"(
[advanced.diagnostics]
debug = true
logging = true
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.advanced.diagnostics.debug);
    CHECK(result.advanced.diagnostics.logging);
    CHECK_FALSE(result.advanced.diagnostics.hotkey_suppression_logging);
    CHECK_FALSE(result.advanced.diagnostics.notification_skip_logging);
    CHECK_FALSE(result.advanced.diagnostics.fleet_selection_timing_logging);
  }

  TEST_CASE("deprecated queue viewer key warns when its value is not boolean")
  {
    auto config = toml::parse(R"(
[advanced.queue]
queue_add_hide_viewers = "false"
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(has_diagnostic_source(result.diagnostics, "advanced.queue.queue_add_hide_viewers",
                                "advanced.queue.queue_add_hide_viewers", config_schema::DiagnosticSeverity::Warning));
    CHECK(has_diagnostic_message_fragment(result.diagnostics, "advanced.queue.queue_add_hide_viewers",
                                          "Expected boolean, found string"));
  }

  TEST_CASE("invalid sidecar fleet runtime mode falls back to normal")
  {
    auto config = toml::parse(R"(
[sidecar.sync]
fleet_runtime_mode = "surprise"
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.config.sync.fleet_runtime_mode == "normal");
    CHECK(has_diagnostic(result.diagnostics, "sidecar.sync.fleet_runtime_mode",
                         config_schema::DiagnosticSeverity::Warning));
  }

  TEST_CASE("sidecar battlelog enrichment defaults closed and accepts legacy decoder alias")
  {
    {
      auto config = toml::parse(R"(
[sidecar.sync]
enabled = true
battlelogs_realtime = true

[advanced.diagnostics]
battle_log_decoder = true
battle_catalog = true
)");

      const auto result = ParseSidecarConfig(config);

      CHECK(result.config.sync.battlelogs_realtime);
      CHECK_FALSE(result.config.sync.battlelog_enrichment);
      CHECK(result.advanced.diagnostics.battle_log_decoder);
      CHECK(result.advanced.diagnostics.battle_catalog);
      CHECK_FALSE(has_diagnostic_source(result.diagnostics, "sidecar.sync.battlelog_enrichment",
                                        "battle_log_decoder.enabled", config_schema::DiagnosticSeverity::Info));
    }

    {
      auto config = toml::parse(R"(
[battle_log_decoder]
enabled = true
)");

      const auto result = ParseSidecarConfig(config);

      CHECK(result.config.sync.battlelog_enrichment);
      CHECK(has_diagnostic_source(result.diagnostics, "sidecar.sync.battlelog_enrichment", "battle_log_decoder.enabled",
                                  config_schema::DiagnosticSeverity::Info));
    }

    {
      auto config = toml::parse(R"(
[sidecar.sync]
battlelog_enrichment = false

[battle_log_decoder]
enabled = true
)");

      const auto result = ParseSidecarConfig(config);

      CHECK_FALSE(result.config.sync.battlelog_enrichment);
      CHECK(has_diagnostic_source(result.diagnostics, "sidecar.sync.battlelog_enrichment", "battle_log_decoder.enabled",
                                  config_schema::DiagnosticSeverity::Warning));
    }
  }

  TEST_CASE("deprecated sidecar observability aliases still populate advanced diagnostics")
  {
    auto config = toml::parse(R"(
[sidecar.probes]
ship_identity = true
battle_log_decoder = false
battle_catalog = true

[sidecar.diagnostics]
debug = true
logging = false
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.advanced.diagnostics.ship_identity);
    CHECK_FALSE(result.advanced.diagnostics.battle_log_decoder);
    CHECK(result.advanced.diagnostics.battle_catalog);
    CHECK(result.advanced.diagnostics.debug);
    CHECK_FALSE(result.advanced.diagnostics.logging);

    CHECK(has_diagnostic_source(result.diagnostics, "advanced.diagnostics.ship_identity",
                                "sidecar.probes.ship_identity", config_schema::DiagnosticSeverity::Info));
    CHECK(has_diagnostic_source(result.diagnostics, "advanced.diagnostics.battle_catalog",
                                "sidecar.probes.battle_catalog", config_schema::DiagnosticSeverity::Info));
    CHECK(has_diagnostic_source(result.diagnostics, "advanced.diagnostics.debug", "sidecar.diagnostics.debug",
                                config_schema::DiagnosticSeverity::Info));
  }

  TEST_CASE("rejects legacy sidecar sync targets and loopback sync urls without flagging external targets")
  {
    auto config = toml::parse(R"(
[sync]
url = "http://127.0.0.1:43127/api/majel/ingest"
token = "legacy-token"

[sync.targets.sidecar]
url = "http://127.0.0.1:43127/api/events"
token = "local-token"
mode = "sidecar_broker"

[sync.targets.sidecar_battle]
url = "http://127.0.0.1:43127/api/events"
token = "battle-token"
mode = "legacy"

[sync.targets.external]
url = "https://majel.example.test/api/ingest/events"
token = "cloud-token"
mode = "majel"
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.reject_legacy_sync_url);
    CHECK(has_rejected_target(result.rejected_sync_targets, "sidecar"));
    CHECK(has_rejected_target(result.rejected_sync_targets, "sidecar_battle"));
    CHECK_FALSE(has_rejected_target(result.rejected_sync_targets, "external"));

    CHECK(has_diagnostic(result.diagnostics, "sync.targets.sidecar", config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.targets.sidecar.mode", config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.targets.sidecar.url", config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.targets.sidecar_battle.url",
                         config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.url", config_schema::DiagnosticSeverity::Error));
  }

  TEST_CASE("runtime snapshot redacts sidecar secrets")
  {
    SidecarConfig  sidecar;
    AdvancedConfig advanced;
    sidecar.sync.enabled                                  = true;
    sidecar.sync.transport                                = "named_pipe";
    sidecar.sync.pipe_name                                = "stfc-mod-bridge.battle.v1";
    sidecar.sync.url                                      = "http://127.0.0.1:43127/api/sidecar/ingest";
    sidecar.sync.token                                    = "secret-sidecar-token";
    sidecar.sync.proxy                                    = "http://user:pass@example.invalid:8080";
    sidecar.sync.battlelogs_realtime                      = true;
    sidecar.sync.battlelog_enrichment                     = true;
    sidecar.logging.jsonl                                 = true;
    sidecar.logging.jsonl_replay_seconds                  = 15;
    sidecar.logging.jsonl_recent_logs                     = 7;
    advanced.diagnostics.debug                            = true;
    advanced.diagnostics.ship_identity                    = true;
    advanced.diagnostics.hotkey_suppression_logging       = true;
    advanced.diagnostics.notification_skip_logging        = true;
    advanced.diagnostics.fleet_selection_timing_logging   = true;
    advanced.diagnostics.live_query                       = true;
    advanced.diagnostics.runtime_trace                    = "detailed";
    advanced.diagnostics.runtime_trace_track_overhead     = false;
    advanced.diagnostics.action_queue_guard_logging       = true;
    advanced.diagnostics.mod_impact_monitor               = true;
    advanced.diagnostics.runtime_trace_report_interval_ms = 7000;
    advanced.diagnostics.refinery_diagnostics             = true;
    advanced.diagnostics.files.root                       = "custom/native-logs";
    advanced.diagnostics.files.navhook_trace_max_kb       = 4096;
    advanced.diagnostics.files.navhook_trace_files        = 4;
    advanced.diagnostics.files.action_queue_probe_max_kb  = 8192;
    advanced.diagnostics.files.action_queue_probe_files   = 5;
    advanced.queue.queue_repair_enabled                   = true;
    advanced.queue.thin_queue_protection                  = true;
    advanced.queue.queue_add_direct_handler               = true;

    toml::table runtime_snapshot;
    WriteSidecarConfigRuntimeSnapshot(runtime_snapshot, sidecar);
    WriteAdvancedConfigRuntimeSnapshot(runtime_snapshot, advanced);

    REQUIRE(runtime_snapshot["sidecar"]["sync"]["token"].is_string());
    REQUIRE(runtime_snapshot["sidecar"]["sync"]["proxy"].is_string());
    CHECK(runtime_snapshot["sidecar"]["sync"]["transport"].value<std::string>().value_or("") == "named_pipe");
    CHECK(runtime_snapshot["sidecar"]["sync"]["pipe_name"].value<std::string>().value_or("")
          == "stfc-mod-bridge.battle.v1");
    CHECK(runtime_snapshot["sidecar"]["sync"]["token"].value<std::string>().value_or("")
          == config_redaction::redact_secret_for_runtime_snapshot(sidecar.sync.token));
    CHECK(runtime_snapshot["sidecar"]["sync"]["proxy"].value<std::string>().value_or("")
          == config_redaction::mask_proxy_userinfo(sidecar.sync.proxy));
    CHECK(runtime_snapshot["sidecar"]["logging"]["jsonl"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["sidecar"]["sync"]["battlelog_enrichment"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["sidecar"]["logging"]["jsonl_replay_seconds"].value<int>().value_or(0) == 15);
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["debug"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["ship_identity"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["hotkey_suppression_logging"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["notification_skip_logging"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["fleet_selection_timing_logging"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["live_query"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["runtime_trace"].value<std::string>().value_or("") == "detailed");
    CHECK_FALSE(
        runtime_snapshot["advanced"]["diagnostics"]["runtime_trace_track_overhead"].value<bool>().value_or(true));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["action_queue_guard_logging"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["mod_impact_monitor"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["runtime_trace_report_interval_ms"].value<int>().value_or(0)
          == 7000);
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["refinery_diagnostics"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["files"]["root"].value<std::string>().value_or("")
          == "custom/native-logs");
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["files"]["navhook_trace_max_kb"].value<int>().value_or(0)
          == 4096);
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["files"]["navhook_trace_files"].value<int>().value_or(0) == 4);
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["files"]["action_queue_probe_max_kb"].value<int>().value_or(0)
          == 8192);
    CHECK(runtime_snapshot["advanced"]["diagnostics"]["files"]["action_queue_probe_files"].value<int>().value_or(0)
          == 5);
    REQUIRE(runtime_snapshot["advanced"]["queue"].is_table());
    CHECK(runtime_snapshot["advanced"]["queue"]["queue_repair_enabled"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["queue"]["thin_queue_protection"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["advanced"]["queue"]["queue_add_direct_handler"].value<bool>().value_or(false));
    CHECK_FALSE(runtime_snapshot["advanced"]["queue"].as_table()->contains("queue_add_hide_viewers"));

    const auto* sidecar_table = runtime_snapshot["sidecar"].as_table();
    REQUIRE(sidecar_table != nullptr);
    CHECK_FALSE(sidecar_table->contains("probes"));
    CHECK_FALSE(sidecar_table->contains("diagnostics"));
  }

  TEST_CASE("clamps advanced diagnostics file policy to positive bounds")
  {
    auto config = toml::parse(R"(
[advanced.diagnostics.files]
navhook_trace_max_kb = -10
navhook_trace_files = 0
action_queue_probe_max_kb = -1
action_queue_probe_files = 0
)");

    const auto result = ParseSidecarConfig(config);

    CHECK(result.advanced.diagnostics.files.navhook_trace_max_kb == 1);
    CHECK(result.advanced.diagnostics.files.navhook_trace_files == 1);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_max_kb == 1);
    CHECK(result.advanced.diagnostics.files.action_queue_probe_files == 1);
  }

  TEST_CASE("opt-in runtime diagnostics are omitted from generated user config")
  {
    const auto source = R"toml(
[advanced.diagnostics]
runtime_trace = "off"
runtime_trace_track_overhead = false
runtime_trace_report_interval_ms = 5000
mod_impact_monitor = false
action_queue_guard_logging = false
notification_skip_logging = false
)toml";

    auto user_config = toml::parse(source);

    OmitOptInRuntimeDiagnosticsFromGeneratedUserConfig(user_config);

    const auto* diagnostics = user_config["advanced"]["diagnostics"].as_table();
    REQUIRE(diagnostics != nullptr);
    CHECK_FALSE(diagnostics->contains("runtime_trace"));
    CHECK_FALSE(diagnostics->contains("runtime_trace_track_overhead"));
    CHECK_FALSE(diagnostics->contains("runtime_trace_report_interval_ms"));
    CHECK_FALSE(diagnostics->contains("mod_impact_monitor"));
    CHECK_FALSE(diagnostics->contains("action_queue_guard_logging"));
    CHECK(diagnostics->contains("notification_skip_logging"));
  }
}
