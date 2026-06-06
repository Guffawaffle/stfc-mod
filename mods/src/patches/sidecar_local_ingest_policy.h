#pragma once

#include "config.h"

enum class SidecarLocalIngestKind {
  BattleEvents = 0,
  FleetRuntime,
  ObservedHostiles,
};

bool SidecarLocalSyncTransportReady(const SidecarSyncConfig& config);
bool SidecarLocalSyncEnabledFor(const SidecarSyncConfig& config, SidecarLocalIngestKind kind);
bool BattleHeaderProcessingNeedsSidecarLocal(const SidecarSyncConfig& config);
bool BattleHeaderProcessingEnabledForSync(bool sync_battlelogs, bool sidecar_logging_jsonl,
                                          bool external_battles_enabled, const SidecarSyncConfig& sidecar_sync);
