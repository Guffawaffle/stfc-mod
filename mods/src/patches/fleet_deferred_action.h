#pragma once

#include <cstdint>

namespace fleet_deferred_action
{
struct State {
  bool      pending = false;
  uint64_t  fleet_id = 0;
  uintptr_t widget_identity = 0;
  uintptr_t target_identity = 0;
  uint64_t  generation = 0;
};

void Clear(State& state) noexcept;
void Arm(State& state, uint64_t fleet_id, uintptr_t widget_identity, uintptr_t target_identity) noexcept;

[[nodiscard]] bool MatchesFleet(const State& state, uint64_t fleet_id) noexcept;
[[nodiscard]] bool MatchesTarget(const State& state, uint64_t fleet_id, uintptr_t widget_identity,
                                 uintptr_t target_identity) noexcept;
} // namespace fleet_deferred_action