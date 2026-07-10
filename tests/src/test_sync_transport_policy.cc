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
  }

  TEST_CASE("mod capability snapshots only target Majel-envelope transports")
  {
    SyncTargetConfig target;
    target.ships = true;
    target.slots = true;
    target.fleet_runtime = true;

    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::Ships));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::ModCapabilities));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetRuntime));

    target.mode = SyncTargetConfig::Mode::Majel;
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::ModCapabilities));
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetRuntime));

    target.slots = false;
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
  }

  TEST_CASE("mod capability snapshot is redacted and declares supported schemas")
  {
    const auto snapshot = http::BuildModCapabilitySnapshot({
        .source_version = "2.0.1-test",
        .platform = "windows",
        .targets =
            {
                {
                    .name = "majel",
                    .mode = SyncTargetConfig::Mode::Majel,
                    .enabled_sync_types = {"ship", "slot"},
                },
            },
        .supported_schemas = {"stfc.mod.capability_snapshot.v1", "stfc.sync.delta_batch.v1"},
    });

    CHECK(snapshot["schemaVersion"] == "stfc.mod.capability_snapshot.v1");
    CHECK(snapshot["modVersion"] == "2.0.1-test");
    CHECK(snapshot["platform"] == "windows");
    CHECK(snapshot["targets"][0]["name"] == "majel");
    CHECK(snapshot["targets"][0]["mode"] == "majel");
    CHECK(snapshot["targets"][0]["enabledSyncTypes"][1] == "slot");
    CHECK(snapshot["privacy"]["tokenRedacted"] == true);
    CHECK(snapshot["privacy"]["containsEndpointUrls"] == false);
    CHECK(snapshot.dump().find("secret-token") == std::string::npos);
    CHECK(snapshot.dump().find("https://") == std::string::npos);
  }

  TEST_CASE("fleet assignment snapshot projects existing fleet preset slot deltas")
  {
    constexpr int64_t kDrydockId = 2644013931949275600LL;
    constexpr int64_t kShipId    = 2682548280591992155LL;
    constexpr int64_t kOfficerA  = 4294967297LL;
    constexpr int64_t kOfficerB  = 8589934593LL;

    const auto slot_delta = nlohmann::json{
        {"type", "slot"},
        {"sid", 123456},
        {"slot_type", 7},
        {"spec_id", 789},
        {"item_id", nullptr},
        {"params",
         {
             {"name", "Freebooter X"},
             {"order", 2},
             {"setup",
              nlohmann::json::array({
                  {
                      {"drydock_id", kDrydockId},
                      {"ship_id", kShipId},
                      {"officer_ids", nlohmann::json::array({kOfficerA, kOfficerB})},
                  },
                  {
                      {"drydock_id", 2},
                      {"ship_id", nullptr},
                      {"officer_ids", nlohmann::json::array()},
                  },
              })},
         }},
    };

    const auto snapshot = http::BuildFleetAssignmentSnapshot(slot_delta);
    REQUIRE(snapshot.has_value());
    CHECK((*snapshot)["schemaVersion"] == "stfc.fleet.assignment_snapshot.v1");
    CHECK((*snapshot)["sourceSlotId"] == "123456");
    CHECK((*snapshot)["sourceSlotSpecId"] == 789);
    CHECK((*snapshot)["presetName"] == "Freebooter X");
    CHECK((*snapshot)["presetOrder"] == 2);
    CHECK((*snapshot)["assignments"][0]["drydockId"] == std::to_string(kDrydockId));
    CHECK((*snapshot)["assignments"][0]["shipId"] == std::to_string(kShipId));
    CHECK((*snapshot)["assignments"][0]["officerIds"]
          == nlohmann::json::array({std::to_string(kOfficerA), std::to_string(kOfficerB)}));
    CHECK((*snapshot)["assignments"][1]["drydockId"] == "2");
    CHECK((*snapshot)["assignments"][1]["shipId"].is_null());
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
