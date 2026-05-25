#include <doctest/doctest.h>

#include "config_redaction.h"
#include "config_sidecar.h"

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
  TEST_CASE("parses sidecar namespaces and rejects legacy sync jsonl keys")
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
fleet_runtime = true

[sidecar.probes]
ship_identity = true
battle_log_decoder = true
battle_catalog = false

[sidecar.logging]
jsonl = true
jsonl_replay_seconds = 45
jsonl_recent_logs = 12

[sidecar.diagnostics]
debug = true
logging = true

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
    CHECK(result.config.sync.fleet_runtime);

    CHECK(result.config.probes.ship_identity);
    CHECK(result.config.probes.battle_log_decoder);
    CHECK_FALSE(result.config.probes.battle_catalog);

    CHECK(result.config.logging.jsonl);
    CHECK(result.config.logging.jsonl_replay_seconds == 45);
    CHECK(result.config.logging.jsonl_recent_logs == 12);

    CHECK(result.config.diagnostics.debug);
    CHECK(result.config.diagnostics.logging);

    CHECK(has_diagnostic(result.diagnostics, "sync.sidecar_jsonl", config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.sidecar_jsonl_replay_seconds",
                         config_schema::DiagnosticSeverity::Error));
    CHECK(has_diagnostic(result.diagnostics, "sync.sidecar_jsonl_recent_logs",
                         config_schema::DiagnosticSeverity::Error));
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
    SidecarConfig config;
    config.sync.enabled                             = true;
    config.sync.url                                 = "http://127.0.0.1:43127/api/sidecar/ingest";
    config.sync.token                               = "secret-sidecar-token";
    config.sync.proxy                               = "http://user:pass@example.invalid:8080";
    config.sync.battlelogs_realtime                 = true;
    config.logging.jsonl                            = true;
    config.logging.jsonl_replay_seconds             = 15;
    config.logging.jsonl_recent_logs                = 7;
    config.diagnostics.debug                        = true;

    toml::table runtime_snapshot;
    WriteSidecarConfigRuntimeSnapshot(runtime_snapshot, config);

    REQUIRE(runtime_snapshot["sidecar"]["sync"]["token"].is_string());
    REQUIRE(runtime_snapshot["sidecar"]["sync"]["proxy"].is_string());
    CHECK(runtime_snapshot["sidecar"]["sync"]["token"].value<std::string>().value_or("")
          == config_redaction::redact_secret_for_runtime_snapshot(config.sync.token));
    CHECK(runtime_snapshot["sidecar"]["sync"]["proxy"].value<std::string>().value_or("")
          == config_redaction::mask_proxy_userinfo(config.sync.proxy));
    CHECK(runtime_snapshot["sidecar"]["logging"]["jsonl"].value<bool>().value_or(false));
    CHECK(runtime_snapshot["sidecar"]["logging"]["jsonl_replay_seconds"].value<int>().value_or(0) == 15);
  }
}