#pragma once

#include <nlohmann/json_fwd.hpp>

namespace sidecar_local_ingest
{
bool BattleEventsEnabled();
bool FleetRuntimeEnabled();
bool ObservedHostilesEnabled();
bool EnqueueBattleEvents(const nlohmann::json& events);
bool EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload);
bool EnqueueObservedHostileEvents(const nlohmann::json& events);
void Shutdown();
} // namespace sidecar_local_ingest
