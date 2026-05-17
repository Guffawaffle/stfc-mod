#include <doctest/doctest.h>

#include "patches/sync_transport_policy.h"

TEST_SUITE("sync_transport_policy")
{
  TEST_CASE("verify_ssl stays enabled unless the explicit unsafe override is also set")
  {
    SyncConfig config;

    auto decision = http::DecideSyncTlsVerification(config);
    CHECK_FALSE(decision.disable_verification);
    CHECK_FALSE(decision.warn_verify_ssl_ignored);
    CHECK_FALSE(decision.emit_unsafe_tls_error);

    config.verify_ssl = false;
    decision = http::DecideSyncTlsVerification(config);
    CHECK_FALSE(decision.disable_verification);
    CHECK(decision.warn_verify_ssl_ignored);
    CHECK_FALSE(decision.emit_unsafe_tls_error);

    config.allow_unsafe_tls_without_certificate_validation = true;
    decision = http::DecideSyncTlsVerification(config);
    CHECK(decision.disable_verification);
    CHECK_FALSE(decision.warn_verify_ssl_ignored);
    CHECK(decision.emit_unsafe_tls_error);
  }

  TEST_CASE("scopely session headers are rebuilt from the latest snapshot")
  {
    http::headers::SessionHeaderSnapshot first_snapshot{
        .gameServerUrl = "https://example.invalid",
        .instanceSessionId = "session-one",
        .instanceId = 7,
        .unityVersion = "6000.0.52f1",
        .primeVersion = "1.000.1",
    };

    const auto first_headers = http::BuildScopelySessionHeaders(first_snapshot, "txn-1");
    CHECK(first_headers.transaction_id == "txn-1");
    CHECK(first_headers.auth_session_id == "session-one");
    CHECK(first_headers.prime_version == "1.000.1");
    CHECK(first_headers.instance_id == "007");
    CHECK(first_headers.unity_version == "6000.0.52f1");

    http::headers::SessionHeaderSnapshot second_snapshot = first_snapshot;
    second_snapshot.instanceSessionId = "session-two";
    second_snapshot.instanceId = 42;
    second_snapshot.primeVersion = "1.000.2";
    second_snapshot.unityVersion = "6000.0.60f1";

    const auto second_headers = http::BuildScopelySessionHeaders(second_snapshot, "txn-2");
    CHECK(second_headers.transaction_id == "txn-2");
    CHECK(second_headers.auth_session_id == "session-two");
    CHECK(second_headers.prime_version == "1.000.2");
    CHECK(second_headers.instance_id == "042");
    CHECK(second_headers.unity_version == "6000.0.60f1");
  }

  TEST_CASE("target mode controls auth header and Majel envelope use")
  {
    SyncTargetConfig target;
    target.token = "secret-token";

    auto headers = http::BuildSyncTargetHeaders(target, "test-powered-by");
    CHECK(headers["stfc-sync-token"] == "secret-token");
    CHECK(headers.find("Authorization") == headers.end());
    CHECK_FALSE(http::SyncTargetUsesMajelEnvelope(target.mode));

    target.mode = SyncTargetConfig::Mode::Majel;
    headers = http::BuildSyncTargetHeaders(target, "test-powered-by");
    CHECK(headers["Authorization"] == "Bearer secret-token");
    CHECK(headers.find("stfc-sync-token") == headers.end());
    CHECK(http::SyncTargetUsesMajelEnvelope(target.mode));

    target.mode = SyncTargetConfig::Mode::SidecarBroker;
    headers = http::BuildSyncTargetHeaders(target, "test-powered-by");
    CHECK(headers["stfc-sync-token"] == "secret-token");
    CHECK(headers.find("Authorization") == headers.end());
    CHECK(http::SyncTargetUsesMajelEnvelope(target.mode));
  }

  TEST_CASE("Majel envelope preserves schema payloads and wraps legacy deltas")
  {
    const auto fleet_payload = nlohmann::json{
        {"schemaVersion", "stfc.fleet.runtime_snapshot.v1"},
        {"selectedIndex", 1},
    };

    auto envelope = http::BuildMajelIngestEnvelope({
        .sync_type = SyncConfig::Type::FleetRuntime,
        .payload = fleet_payload,
        .event_id = "event-1",
        .source_version = "2.0.1-test",
        .install_id = "install-1",
        .session_id = "session-1",
        .sequence = 7,
        .observed_at = "2026-05-17T22:00:00Z",
    });

    CHECK(envelope["protocolVersion"] == "majel.ingest.v1");
    CHECK(envelope["source"] == "stfc-community-mod");
    CHECK(envelope["schema"] == "stfc.fleet.runtime_snapshot.v1");
    CHECK(envelope["classification"] == "cloud_private");
    CHECK(envelope["payload"] == fleet_payload);

    const auto legacy_items = nlohmann::json::array({{{"sid", "slot-1"}}});
    envelope = http::BuildMajelIngestEnvelope({
        .sync_type = SyncConfig::Type::Slots,
        .payload = legacy_items,
        .event_id = "event-2",
        .source_version = "2.0.1-test",
        .install_id = "install-1",
        .session_id = "session-1",
        .sequence = 8,
        .observed_at = "2026-05-17T22:00:01Z",
    });

    CHECK(envelope["schema"] == "stfc.sync.delta_batch.v1");
    CHECK(envelope["payload"]["syncType"] == "slot");
    CHECK(envelope["payload"]["items"] == legacy_items);
  }
}
