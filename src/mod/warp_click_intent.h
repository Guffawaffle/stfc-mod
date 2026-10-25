#pragma once

#include <cstdint>
#include <utility>

// One manual move request; every subsequent move replaces it and the first
// popup consumes it. Only scalar identities survive native planning delays.
struct WarpClickIntent {
  int64_t fleet       = 0;
  int64_t destination = 0;
  bool    unmatched   = false;

  void Begin(bool ask, int64_t fleet_id, int64_t node_id)
  {
    fleet       = ask && fleet_id > 0 && node_id > 0 ? fleet_id : 0;
    destination = fleet ? node_id : 0;
    unmatched   = ask && !fleet;
  }

  bool Consume(bool observed, int64_t fleet_id, int64_t node_id)
  {
    const auto pending = std::exchange(*this, {});
    // A user-requested choice survives an unavailable identity observation.
    // A positively identified different trip does not inherit the override.
    return pending.unmatched
           || (pending.fleet && (!observed || (pending.fleet == fleet_id && pending.destination == node_id)));
  }
};
