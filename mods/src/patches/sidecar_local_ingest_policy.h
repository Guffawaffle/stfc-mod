#pragma once

#include "config.h"

enum class SidecarLocalIngestKind {
  BattleEvents = 0,
  FleetRuntime,
  FleetAlertEvidence,
};

bool SidecarLocalSyncTransportReady(const SidecarSyncConfig& config);
bool SidecarLocalSyncEnabledFor(const SidecarSyncConfig& config, SidecarLocalIngestKind kind);
bool BattleHeaderProcessingNeedsSidecarLocal(const SidecarSyncConfig& config);
bool BattleHeaderProcessingEnabledForSync(bool                   sync_battlelogs,
                                          bool                   sidecar_logging_jsonl,
                                          bool                   external_battles_enabled,
                                          bool                   external_battlelogs_realtime_enabled,
                                          const SidecarSyncConfig& sidecar_sync);
