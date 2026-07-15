#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ship_state_probe
{
struct RepairStatusTransition {
  bool    changed         = false;
  bool    has_previous    = false;
  int32_t previous_status = 0;
};

constexpr bool should_capture_ready_while_repairing_caller(const RepairStatusTransition& transition,
                                                           int32_t current_status, int32_t current_fleet_state)
{
  constexpr int32_t kActionStatusReady   = 100;
  constexpr int32_t kFleetStateRepairing = 32;
  return transition.has_previous && current_status == kActionStatusReady && current_fleet_state == kFleetStateRepairing;
}

class RepairStatusTransitionCache
{
public:
  static constexpr size_t kCapacity = 16;

  RepairStatusTransition observe(uint64_t fleet_id, int32_t status)
  {
    for (auto& observed : observed_) {
      if (!observed.occupied || observed.fleet_id != fleet_id) {
        continue;
      }
      if (observed.status == status) {
        return {};
      }
      const auto previous_status = observed.status;
      observed.status            = status;
      return {true, true, previous_status};
    }

    for (auto& observed : observed_) {
      if (observed.occupied) {
        continue;
      }
      observed = {fleet_id, status, true};
      return {true, false, 0};
    }

    observed_[replacement_index_++ % observed_.size()] = {fleet_id, status, true};
    return {true, false, 0};
  }

  bool record(uint64_t fleet_id, int32_t status)
  { return observe(fleet_id, status).changed; }

private:
  struct ObservedRepairStatus {
    uint64_t fleet_id = 0;
    int32_t  status   = 0;
    bool     occupied = false;
  };

  std::array<ObservedRepairStatus, kCapacity> observed_{};
  size_t                                      replacement_index_ = 0;
};
} // namespace ship_state_probe
