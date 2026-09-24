#pragma once

#include <cstdint>
#include <utility>

// One outstanding manual move. A subsequent move (including an ordinary click)
// replaces it; the first course popup consumes it even if it belongs to another trip.
struct WarpClickIntent {
  std::int64_t fleet       = 0;
  std::int64_t destination = 0;

  void Begin(bool ask, std::int64_t fleet_id, std::int64_t node_id)
  {
    fleet       = ask && fleet_id > 0 && node_id > 0 ? fleet_id : 0;
    destination = fleet ? node_id : 0;
  }

  bool Consume(std::int64_t fleet_id, std::int64_t node_id)
  {
    const auto pending = std::exchange(*this, {});
    return pending.fleet != 0 && pending.fleet == fleet_id && pending.destination == node_id;
  }
};
