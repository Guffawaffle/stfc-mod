#include "patches/sidecar_local_ingest_policy.h"

bool SidecarLocalSyncTransportReady(const SidecarSyncConfig& config)
{ return config.enabled && !config.url.empty() && !config.token.empty(); }

bool SidecarLocalSyncEnabledFor(const SidecarSyncConfig& config, const SidecarLocalIngestKind kind)
{
  if (!SidecarLocalSyncTransportReady(config)) {
    return false;
  }

  switch (kind) {
    case SidecarLocalIngestKind::BattleEvents:
      return config.battlelogs_realtime;
    case SidecarLocalIngestKind::FleetRuntime:
      return config.fleet_runtime;
  }

  return false;
}

bool BattleHeaderProcessingNeedsSidecarLocal(const SidecarSyncConfig& config)
{ return SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents); }

bool BattleHeaderProcessingEnabledForSync(const bool sync_battlelogs, const bool sidecar_logging_jsonl,
                                          const bool external_battles_enabled, const SidecarSyncConfig& sidecar_sync)
{
  return sync_battlelogs || sidecar_logging_jsonl || external_battles_enabled
         || BattleHeaderProcessingNeedsSidecarLocal(sidecar_sync);
}
