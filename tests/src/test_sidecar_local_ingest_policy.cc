#include <doctest/doctest.h>

#include "patches/sidecar_local_ingest_policy.h"

namespace
{
SidecarSyncConfig configured_sidecar_sync()
{
  SidecarSyncConfig config;
  config.enabled = true;
  config.url     = "http://127.0.0.1:43127/api/sidecar/ingest";
  config.token   = "local-sidecar-token";
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

    config = configured_sidecar_sync();
    CHECK(SidecarLocalSyncTransportReady(config));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));

    config.battlelogs_realtime = true;
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));

    config.fleet_runtime = true;
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));

    config.enabled = false;
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
  }

  TEST_CASE("missing sidecar token or url disables only the local sidecar path")
  {
    auto config                = configured_sidecar_sync();
    config.battlelogs_realtime = true;

    CHECK(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));

    config.token.clear();
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, true, config));

    config                     = configured_sidecar_sync();
    config.battlelogs_realtime = true;
    config.url.clear();
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(BattleHeaderProcessingEnabledForSync(false, true, false, config));
  }

  TEST_CASE("battle header processing stays alive for dedicated sidecar realtime delivery")
  {
    auto config                = configured_sidecar_sync();
    config.battlelogs_realtime = true;

    CHECK(BattleHeaderProcessingNeedsSidecarLocal(config));
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, false, config));

    SidecarSyncConfig disabled_config = config;
    disabled_config.enabled           = false;
    CHECK_FALSE(BattleHeaderProcessingNeedsSidecarLocal(disabled_config));
    CHECK_FALSE(BattleHeaderProcessingEnabledForSync(false, false, false, disabled_config));
  }
}
