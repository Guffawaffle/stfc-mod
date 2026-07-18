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

struct RepairInstantContextSnapshot {
  int32_t current_fleet_state  = 0;
  int32_t previous_fleet_state = 0;
  int64_t amount               = 0;
  bool    context_present      = false;
  bool    interactable         = false;
  bool    has_amount           = false;

  constexpr bool operator==(const RepairInstantContextSnapshot& other) const
  {
    return current_fleet_state == other.current_fleet_state && previous_fleet_state == other.previous_fleet_state
           && amount == other.amount && context_present == other.context_present && interactable == other.interactable
           && has_amount == other.has_amount;
  }
};

struct RepairStatusSnapshot {
  int32_t original_status = 0;
  int32_t returned_status = 0;
};

constexpr bool should_capture_ready_while_repairing_caller(const RepairStatusTransition& transition,
                                                           int32_t current_status, int32_t current_fleet_state)
{
  constexpr int32_t kActionStatusReady   = 100;
  constexpr int32_t kFleetStateRepairing = 32;
  return transition.has_previous && current_status == kActionStatusReady && current_fleet_state == kFleetStateRepairing;
}

constexpr int32_t project_repair_action_status(int32_t original_status, int32_t current_fleet_state, bool guard_enabled)
{
  constexpr int32_t kActionStatusDisabled = 0;
  constexpr int32_t kActionStatusReady    = 100;
  constexpr int32_t kFleetStateRepairing  = 32;
  return guard_enabled && original_status == kActionStatusReady && current_fleet_state == kFleetStateRepairing
             ? kActionStatusDisabled
             : original_status;
}

constexpr bool should_suppress_stale_instant_click_after_repair(int32_t current_fleet_state,
                                                                int32_t previous_fleet_state, int32_t original_status,
                                                                const RepairInstantContextSnapshot& instant_context)
{
  constexpr int32_t kActionStatusReady   = 100;
  constexpr int32_t kFleetStateDocked    = 2;
  constexpr int32_t kFleetStateRepairing = 32;
  return current_fleet_state == kFleetStateDocked && previous_fleet_state == kFleetStateRepairing
         && original_status == kActionStatusReady && instant_context.context_present && instant_context.interactable
         && instant_context.has_amount;
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

class RepairInstantContextTransitionCache
{
public:
  static constexpr size_t kCapacity = 16;

  bool record(uint64_t fleet_id, const RepairInstantContextSnapshot& snapshot)
  {
    for (auto& observed : observed_) {
      if (!observed.occupied || observed.fleet_id != fleet_id) {
        continue;
      }
      if (observed.snapshot == snapshot) {
        return false;
      }
      observed.snapshot = snapshot;
      return true;
    }

    for (auto& observed : observed_) {
      if (observed.occupied) {
        continue;
      }
      observed = {fleet_id, snapshot, true};
      return true;
    }

    observed_[replacement_index_++ % observed_.size()] = {fleet_id, snapshot, true};
    return true;
  }

  bool get(uint64_t fleet_id, RepairInstantContextSnapshot& snapshot) const
  {
    for (const auto& observed : observed_) {
      if (observed.occupied && observed.fleet_id == fleet_id) {
        snapshot = observed.snapshot;
        return true;
      }
    }
    return false;
  }

private:
  struct ObservedRepairInstantContext {
    uint64_t                     fleet_id = 0;
    RepairInstantContextSnapshot snapshot{};
    bool                         occupied = false;
  };

  std::array<ObservedRepairInstantContext, kCapacity> observed_{};
  size_t                                              replacement_index_ = 0;
};

class RepairStatusSnapshotCache
{
public:
  static constexpr size_t kCapacity = 16;

  void record(uint64_t fleet_id, const RepairStatusSnapshot& snapshot)
  {
    for (auto& observed : observed_) {
      if (observed.occupied && observed.fleet_id == fleet_id) {
        observed.snapshot = snapshot;
        return;
      }
    }

    for (auto& observed : observed_) {
      if (!observed.occupied) {
        observed = {fleet_id, snapshot, true};
        return;
      }
    }

    observed_[replacement_index_++ % observed_.size()] = {fleet_id, snapshot, true};
  }

  bool get(uint64_t fleet_id, RepairStatusSnapshot& snapshot) const
  {
    for (const auto& observed : observed_) {
      if (observed.occupied && observed.fleet_id == fleet_id) {
        snapshot = observed.snapshot;
        return true;
      }
    }
    return false;
  }

private:
  struct ObservedRepairStatusSnapshot {
    uint64_t             fleet_id = 0;
    RepairStatusSnapshot snapshot{};
    bool                 occupied = false;
  };

  std::array<ObservedRepairStatusSnapshot, kCapacity> observed_{};
  size_t                                              replacement_index_ = 0;
};

class RepairCoherentStatusHoldCache
{
public:
  static constexpr size_t kCapacity = 16;

  int32_t project(uint64_t fleet_id, int32_t original_status, int32_t current_fleet_state,
                  int32_t /*previous_fleet_state*/)
  {
    auto* observed = find_or_insert(fleet_id, original_status);
    if (observed == nullptr) {
      return original_status;
    }

    constexpr int32_t kActionStatusReady      = 100;
    constexpr int32_t kFleetStateRepairing    = 32;
    const auto        repair_lifecycle_active = current_fleet_state == kFleetStateRepairing;

    if (original_status == kActionStatusReady && repair_lifecycle_active
        && is_coherent_repair_progress(observed->last_coherent_status)) {
      return observed->last_coherent_status;
    }

    observed->last_coherent_status = original_status;
    return original_status;
  }

private:
  struct ObservedRepairStatus {
    uint64_t fleet_id             = 0;
    int32_t  last_coherent_status = 0;
    bool     occupied             = false;
  };

  static constexpr bool is_coherent_repair_progress(int32_t status)
  { return status == 200 || status == 201 || status == 202 || status == 300; }

  ObservedRepairStatus* find_or_insert(uint64_t fleet_id, int32_t original_status)
  {
    for (auto& observed : observed_) {
      if (observed.occupied && observed.fleet_id == fleet_id) {
        return &observed;
      }
    }

    for (auto& observed : observed_) {
      if (!observed.occupied) {
        observed = {fleet_id, original_status, true};
        return &observed;
      }
    }

    auto& observed = observed_[replacement_index_++ % observed_.size()];
    observed       = {fleet_id, original_status, true};
    return &observed;
  }

  std::array<ObservedRepairStatus, kCapacity> observed_{};
  size_t                                      replacement_index_ = 0;
};
} // namespace ship_state_probe
