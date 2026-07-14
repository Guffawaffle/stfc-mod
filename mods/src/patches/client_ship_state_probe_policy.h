#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ship_state_probe
{
class RepairStatusTransitionCache
{
public:
  static constexpr size_t kCapacity = 16;

  bool record(uint64_t fleet_id, int32_t status)
  {
    for (auto& observed : observed_) {
      if (!observed.occupied || observed.fleet_id != fleet_id) {
        continue;
      }
      if (observed.status == status) {
        return false;
      }
      observed.status = status;
      return true;
    }

    for (auto& observed : observed_) {
      if (observed.occupied) {
        continue;
      }
      observed = {fleet_id, status, true};
      return true;
    }

    observed_[replacement_index_++ % observed_.size()] = {fleet_id, status, true};
    return true;
  }

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
