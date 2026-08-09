#include <doctest/doctest.h>

#include "patches/sync_transport_policy.h"

#include <stdexcept>

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
    target.battlelogs = true;
    target.battlelogs_realtime = true;

    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::Ships));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::ModCapabilities));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetRuntime));
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::Battles));
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::BattlelogsRealtime));

    target.mode = SyncTargetConfig::Mode::Majel;
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::ModCapabilities));
    CHECK(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetRuntime));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::Battles));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::BattlelogsRealtime));

    target.slots = false;
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::FleetAssignments));
  }

  TEST_CASE("battle normalization disables Majel while preserving Legacy")
  {
    for (const auto type : {SyncConfig::Type::Battles, SyncConfig::Type::BattlelogsRealtime}) {
      CHECK(http::NormalizeSyncTargetTypeForMode(SyncTargetConfig::Mode::Legacy, type, true));
      CHECK_FALSE(http::NormalizeSyncTargetTypeForMode(SyncTargetConfig::Mode::Majel, type, true));
      CHECK_FALSE(http::NormalizeSyncTargetTypeForMode(SyncTargetConfig::Mode::Legacy, type, false));
      CHECK_FALSE(http::NormalizeSyncTargetTypeForMode(SyncTargetConfig::Mode::Majel, type, false));
    }
    CHECK(http::NormalizeSyncTargetTypeForMode(SyncTargetConfig::Mode::Majel, SyncConfig::Type::Ships, true));
  }

  TEST_CASE("explicit malformed target modes fail closed while omission preserves Legacy")
  {
    CHECK(http::ParseSyncTargetMode(false, std::nullopt) == SyncTargetConfig::Mode::Legacy);
    CHECK(http::ParseSyncTargetMode(true, std::string("legacy")) == SyncTargetConfig::Mode::Legacy);
    CHECK(http::ParseSyncTargetMode(true, std::string("majel")) == SyncTargetConfig::Mode::Majel);

    for (const auto& malformed : {std::optional<std::string>(std::nullopt), std::optional<std::string>(""),
                                  std::optional<std::string>("maje1"), std::optional<std::string>("Majel"),
                                  std::optional<std::string>("majel ")}) {
      const auto parsed = http::ParseSyncTargetMode(true, malformed);
      CHECK_FALSE(parsed.has_value());
    }
  }

  TEST_CASE("mod capability snapshot is redacted and declares supported schemas")
  {
    CHECK(http::MajelAdvertisedSchemas()
          == std::vector<std::string>{
              "stfc.mod.capability_snapshot.v1",
              "stfc.sync.delta_batch.v1",
              "stfc.fleet.assignment_snapshot.v1",
          });

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

  TEST_CASE("Majel routing rejects raw battle journals and captures before envelope construction")
  {
    SyncTargetConfig target;
    target.mode = SyncTargetConfig::Mode::Majel;
    target.battlelogs = true;
    target.battlelogs_realtime = true;

    const auto raw_journal = nlohmann::json::array({{{"journal", {{"tokens", {"journal-secret", "-96"}}}}}});
    const auto raw_capture = nlohmann::json{
        {"type", "battle.capture"},
        {"schemaVersion", "stfc.battle.capture.v1"},
        {"capture", {{"battleLog", {{"tokens", {"secret-token", "-96", "42"}}}}}},
    };

    CHECK(raw_journal.dump().find("journal-secret") != std::string::npos);
    CHECK(raw_capture.dump().find("secret-token") != std::string::npos);
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::Battles));
    CHECK_FALSE(http::SyncTargetAcceptsType(target, SyncConfig::Type::BattlelogsRealtime));
    CHECK_THROWS_WITH_AS(
        static_cast<void>(http::BuildMajelIngestEnvelope({.sync_type = SyncConfig::Type::Battles,
                                                          .payload = raw_journal})),
        "raw battle payloads are not supported by Majel", std::invalid_argument);
    CHECK_THROWS_WITH_AS(
        static_cast<void>(http::BuildMajelIngestEnvelope({.sync_type = SyncConfig::Type::BattlelogsRealtime,
                                                          .payload = raw_capture})),
        "raw battle payloads are not supported by Majel", std::invalid_argument);
  }

  TEST_CASE("warning coalescing is deterministic and reports suppressed occurrences")
  {
    http::WarningCoalescingState state;

    auto decision = http::ObserveWarning(state, 1'000, 60'000);
    CHECK(decision.emit);
    CHECK(decision.suppressed == 0);

    decision = http::ObserveWarning(state, 1'001, 60'000);
    CHECK_FALSE(decision.emit);
    decision = http::ObserveWarning(state, 60'999, 60'000);
    CHECK_FALSE(decision.emit);

    decision = http::ObserveWarning(state, 61'000, 60'000);
    CHECK(decision.emit);
    CHECK(decision.suppressed == 2);

    decision = http::ObserveWarning(state, 500, 60'000);
    CHECK(decision.emit);
    CHECK(decision.suppressed == 0);
  }

}
