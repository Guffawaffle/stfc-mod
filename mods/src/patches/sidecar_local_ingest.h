#pragma once

#include "patches/sidecar_local_dispatch_context.h"

#include <cstddef>
#include <cstdint>

#include <nlohmann/json_fwd.hpp>

namespace sidecar_local_ingest
{
struct EnqueueResult {
  bool     accepted        = false;
  bool     coalesced       = false;
  size_t   depth           = 0;
  uint64_t enqueued        = 0;
  uint64_t dropped         = 0;
  uint64_t coalesced_total = 0;
};

bool          BattleEventsEnabled();
bool          FleetRuntimeEnabled();
bool          EnqueueBattleEvents(const nlohmann::json& events, const SidecarLocalDispatchContext& context);
EnqueueResult EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload, const SidecarLocalDispatchContext& context);
void          Shutdown();
} // namespace sidecar_local_ingest
