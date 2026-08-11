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
  config.url     = "http://127.0.0.1:43127/api/sidecar/ingest";
  config.token   = "local-sidecar-token";
  return config;
}

SidecarSyncConfig configured_named_pipe_sync()
{
  auto config      = configured_sidecar_sync();
  config.transport = "named_pipe";
  config.url.clear();
  config.pipe_name = "stfc-mod-bridge.battle.v1";
  config.token     = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
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
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, true, false, config));

    config                     = configured_sidecar_sync();
    config.battlelogs_realtime = true;
    config.url.clear();
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK(BattleHeaderProcessingEnabledForSync(false, true, false, false, config));
  }

  TEST_CASE("named pipe transport requires an exact safe name and 32-byte base64url credential")
  {
    auto config = configured_named_pipe_sync();
#if _WIN32
    CHECK(SidecarLocalSyncTransportReady(config));
#else
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
#endif
    CHECK(SidecarLocalSyncUsesNamedPipe(config));
    CHECK(SidecarLocalNamedPipeNameValid(config.pipe_name));
    CHECK(SidecarLocalNamedPipeCredentialValid(config.token));
    for (const auto final_character : std::string_view{"AEIMQUYcgkosw048"}) {
      config.token.back() = final_character;
      CHECK(SidecarLocalNamedPipeCredentialValid(config.token));
    }

    config.pipe_name = R"(\\.\pipe\attacker)";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config           = configured_named_pipe_sync();
    config.pipe_name = "../battle";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config       = configured_named_pipe_sync();
    config.token = "short";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config              = configured_named_pipe_sync();
    config.token.back() = '=';
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config              = configured_named_pipe_sync();
    config.token.back() = '_';
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config           = configured_named_pipe_sync();
    config.pipe_name = "battle\xC3\xA9";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
  }

  TEST_CASE("malformed or incomplete explicit transport never falls back to HTTP")
  {
    auto config      = configured_sidecar_sync();
    config.transport = "named-pipe";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    config.transport.clear();
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
  }

  TEST_CASE("disabled and unconfigured local producer paths remain inert despite per-kind requests")
  {
    SidecarSyncConfig config;
    config.battlelogs_realtime = true;
    config.fleet_runtime       = true;

    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
    CHECK_FALSE(BattleHeaderProcessingNeedsSidecarLocal(config));
    CHECK_FALSE(BattleHeaderProcessingEnabledForSync(false, false, false, false, config));

    config.enabled = true;
    config.url     = "http://127.0.0.1:43127/api/sidecar/ingest";
    CHECK_FALSE(SidecarLocalSyncTransportReady(config));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents));
    CHECK_FALSE(SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::FleetRuntime));
  }

  TEST_CASE("battle header processing stays alive for dedicated sidecar realtime delivery")
  {
    auto config                = configured_sidecar_sync();
    config.battlelogs_realtime = true;

    CHECK(BattleHeaderProcessingNeedsSidecarLocal(config));
    CHECK(BattleHeaderProcessingEnabledForSync(false, false, false, false, config));

    SidecarSyncConfig disabled_config = config;
    disabled_config.enabled           = false;
    CHECK_FALSE(BattleHeaderProcessingNeedsSidecarLocal(disabled_config));
    CHECK_FALSE(BattleHeaderProcessingEnabledForSync(false, false, false, false, disabled_config));
  }

  TEST_CASE("sidecar local dispatch context preserves copied payload provenance")
  {
    const auto dispatch = gameplay_dispatch_context("battle-result-headers", "SyncEntityGroupHooks",
                                                    "entity-group-json.battle_result_headers",
                                                    "battle-result-headers-observed", "enqueue-battle-journal-fetch");
    const auto context  = sidecar_local_dispatch_context(dispatch, "stfc.sidecar.events.v0", "runtime-evidence",
                                                         "sidecar-local.battle-events", "copied battle events only");

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
