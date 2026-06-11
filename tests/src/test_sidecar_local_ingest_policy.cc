#include <doctest/doctest.h>

#include "patches/gameplay_dispatch_context.h"
#include "patches/sidecar_local_dispatch_context.h"
#include "patches/sidecar_local_ingest_policy.h"

namespace
{
  SidecarSyncConfig configured_sidecar_sync()
  {
    SidecarSyncConfig config;
    config.enabled = true;
    config.url = "http://127.0.0.1:43127/api/sidecar/ingest";
    config.token = "local-sidecar-token";
    return config;
  }
} // namespace

TEST_SUITE("sidecar_local_ingest_policy")
{
  TEST_CASE("local sidecar delivery requires enabled transport and per-kind opt-in")
  {
    SidecarSyncConfig config;
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetAlertEvidence));

    config = configured_sidecar_sync();
    CHECK(SidecarLocalSyncTransportReady(config));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetAlertEvidence));

    config.battlelogs_realtime = true;
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetAlertEvidence));

    config.fleet_runtime = true;
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetAlertEvidence));

    config.enabled = false;
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetAlertEvidence));
  }

  TEST_CASE("missing sidecar token or url disables only the local sidecar path")
  {
    auto config = configured_sidecar_sync();
    config.battlelogs_realtime = true;

    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));

    config.token.clear();
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, true, false, config));

    config = configured_sidecar_sync();
    config.battlelogs_realtime = true;
    config.url.clear();
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(BattleHeaderProcessingEnabledForSync(false, true, false, false, config));
  }

  TEST_CASE("battle header processing stays alive for dedicated sidecar realtime delivery")
  {
    auto config = configured_sidecar_sync();
    config.battlelogs_realtime = true;

    CHECK(BattleHeaderProcessingNeedsSidecarLocal(config));
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, false, false, config));

    SidecarSyncConfig disabled_config = config;
    disabled_config.enabled = false;
    CHECK_FALSE(BattleHeaderProcessingNeedsSidecarLocal(disabled_config));
    CHECK_FALSE(BattleHeaderProcessingEnabledForSync(false, false, false, false, disabled_config));
  }

  TEST_CASE("sidecar local dispatch context preserves copied payload provenance")
  {
    const auto dispatch = gameplay_dispatch_context("battle-result-headers",
                                                    "SyncEntityGroupHooks",
                                                    "entity-group-json.battle_result_headers",
                                                    "battle-result-headers-observed",
                                                    "enqueue-battle-journal-fetch");
    const auto context = sidecar_local_dispatch_context(dispatch,
                                                        "stfc.sidecar.events.v0",
                                                        "runtime-evidence",
                                                        "sidecar-local.battle-events",
                                                        "copied battle events only");

    CHECK(context.dispatch.source == "battle-result-headers");
    CHECK(context.dispatch.owner == "SyncEntityGroupHooks");
    CHECK(context.dispatch.seam == "entity-group-json.battle_result_headers");
    CHECK(context.dispatch.reason == "battle-result-headers-observed");
    CHECK(context.dispatch.effect == "enqueue-battle-journal-fetch");
    CHECK(context.evidence_kind == "stfc.sidecar.events.v0");
    CHECK(context.classification == "runtime-evidence");
    CHECK(context.boundary == "sidecar-local.battle-events");
    CHECK(context.validation == "copied battle events only");
  }
}
