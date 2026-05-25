#pragma once

#include <nlohmann/json_fwd.hpp>

namespace sidecar_local_ingest
{
bool BattleEventsEnabled();
bool FleetRuntimeEnabled();
bool EnqueueBattleEvents(const nlohmann::json& events);
bool EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload);
void Shutdown();
} // namespace sidecar_local_ingest
