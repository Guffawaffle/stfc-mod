#include "patches/fleet_deferred_action.h"

namespace fleet_deferred_action
{
void Clear(State& state) noexcept
{
  state.pending = false;
  state.fleet_id = 0;
  state.widget_identity = 0;
  state.target_identity = 0;
  ++state.generation;
}

void Arm(State& state, const uint64_t fleet_id, const uintptr_t widget_identity, const uintptr_t target_identity) noexcept
{
  if (fleet_id == 0 || widget_identity == 0) {
    Clear(state);
    return;
  }

  state.pending = true;
  state.fleet_id = fleet_id;
  state.widget_identity = widget_identity;
  state.target_identity = target_identity;
  ++state.generation;
}

bool MatchesFleet(const State& state, const uint64_t fleet_id) noexcept
{
  return state.pending && fleet_id != 0 && state.fleet_id == fleet_id;
}

bool MatchesTarget(const State& state,
                   const uint64_t fleet_id,
                   const uintptr_t widget_identity,
                   const uintptr_t target_identity) noexcept
{
  if (!MatchesFleet(state, fleet_id) || widget_identity == 0 || state.widget_identity != widget_identity) {
    return false;
  }

  return state.target_identity == 0 || state.target_identity == target_identity;
}
} // namespace fleet_deferred_action