#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

namespace sidecar_local_ingest
{
struct EnqueueResult {
  bool     accepted = false;
  bool     coalesced = false;
  size_t   depth = 0;
  uint64_t enqueued = 0;
  uint64_t dropped = 0;
  uint64_t coalesced_total = 0;
};

bool BattleEventsEnabled();
bool FleetRuntimeEnabled();
std::string_view FleetRuntimeMode();
bool FleetRuntimeRequestOnlyMode();
bool FleetRuntimeSnapshotOnlyMode();
bool FleetRuntimeEnqueueNoTransportMode();
bool EnqueueBattleEvents(const nlohmann::json& events);
EnqueueResult EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload);
void Shutdown();
} // namespace sidecar_local_ingest
