#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace repair_action_interlock
{
struct InstantContextSnapshot {
  int64_t amount          = 0;
  bool    context_present = false;
  bool    interactable    = false;
  bool    has_amount      = false;
};

struct PresentationDecision {
  bool     hold       = false;
  bool     changed    = false;
  uint64_t elapsed_ms = 0;
};

class State
{
public:
  static constexpr size_t   kCapacity       = 16;
  static constexpr uint64_t kHoldDurationMs = 2500;

  int32_t project_status(uint64_t fleet_id, int32_t original_status, int32_t current_fleet_state)
  {
    auto& entry = find_or_insert(fleet_id);

    auto returned_status = original_status;
    if (entry.status_initialized && original_status == kActionStatusReady && current_fleet_state == kFleetStateRepairing
        && is_coherent_repair_progress(entry.last_coherent_status)) {
      returned_status = entry.last_coherent_status;
    } else {
      entry.last_coherent_status = original_status;
    }

    entry.original_status    = original_status;
    entry.returned_status    = returned_status;
    entry.status_initialized = true;
    return returned_status;
  }

  PresentationDecision observe_instant_context(uint64_t fleet_id, int32_t current_fleet_state,
                                               int32_t                       previous_fleet_state,
                                               const InstantContextSnapshot& instant_context, uint64_t now_ms)
  {
    auto& entry               = find_or_insert(fleet_id);
    entry.instant_context     = instant_context;
    entry.context_initialized = true;

    const auto stale_transition = entry.status_initialized && current_fleet_state == kFleetStateDocked
                                  && previous_fleet_state == kFleetStateRepairing
                                  && entry.original_status == kActionStatusReady;
    if (!stale_transition) {
      const auto changed          = entry.presentation_holding;
      entry.presentation_active   = false;
      entry.presentation_released = false;
      entry.presentation_holding  = false;
      return {false, changed, 0};
    }

    if (!entry.presentation_active) {
      entry.presentation_started_ms = now_ms;
      entry.presentation_active     = true;
      entry.presentation_released   = false;
    }

    const auto elapsed_ms = elapsed_since(entry.presentation_started_ms, now_ms);
    if (elapsed_ms >= kHoldDurationMs) {
      entry.presentation_released = true;
    }

    const auto hold            = !entry.presentation_released;
    const auto changed         = hold != entry.presentation_holding;
    entry.presentation_holding = hold;
    return {hold, changed, elapsed_ms};
  }

  bool should_suppress_instant_click(uint64_t fleet_id, int32_t current_fleet_state, int32_t previous_fleet_state,
                                     uint64_t now_ms) const
  {
    const auto* entry = find(fleet_id);
    if (entry == nullptr || !entry->status_initialized || !entry->context_initialized || !entry->presentation_active
        || entry->presentation_released || elapsed_since(entry->presentation_started_ms, now_ms) >= kHoldDurationMs) {
      return false;
    }

    return current_fleet_state == kFleetStateDocked && previous_fleet_state == kFleetStateRepairing
           && entry->original_status == kActionStatusReady && entry->instant_context.context_present
           && entry->instant_context.interactable && entry->instant_context.has_amount;
  }

  size_t occupied_count() const
  {
    size_t count = 0;
    for (const auto& entry : entries_) {
      count += entry.occupied ? 1 : 0;
    }
    return count;
  }

private:
  static constexpr int32_t kActionStatusReady   = 100;
  static constexpr int32_t kFleetStateDocked    = 2;
  static constexpr int32_t kFleetStateRepairing = 32;

  struct Entry {
    uint64_t               fleet_id             = 0;
    int32_t                original_status      = 0;
    int32_t                returned_status      = 0;
    int32_t                last_coherent_status = 0;
    InstantContextSnapshot instant_context{};
    uint64_t               presentation_started_ms = 0;
    bool                   status_initialized      = false;
    bool                   context_initialized     = false;
    bool                   presentation_active     = false;
    bool                   presentation_released   = false;
    bool                   presentation_holding    = false;
    bool                   occupied                = false;
  };

  static constexpr bool is_coherent_repair_progress(int32_t status)
  { return status == 200 || status == 201 || status == 202 || status == 300; }

  static constexpr uint64_t elapsed_since(uint64_t started_ms, uint64_t now_ms)
  { return now_ms >= started_ms ? now_ms - started_ms : 0; }

  Entry* find(uint64_t fleet_id)
  {
    for (auto& entry : entries_) {
      if (entry.occupied && entry.fleet_id == fleet_id) {
        return &entry;
      }
    }
    return nullptr;
  }

  const Entry* find(uint64_t fleet_id) const
  {
    for (const auto& entry : entries_) {
      if (entry.occupied && entry.fleet_id == fleet_id) {
        return &entry;
      }
    }
    return nullptr;
  }

  Entry& find_or_insert(uint64_t fleet_id)
  {
    if (auto* entry = find(fleet_id); entry != nullptr) {
      return *entry;
    }

    for (auto& entry : entries_) {
      if (!entry.occupied) {
        entry = {.fleet_id = fleet_id, .occupied = true};
        return entry;
      }
    }

    auto& entry = entries_[replacement_index_++ % entries_.size()];
    entry       = {.fleet_id = fleet_id, .occupied = true};
    return entry;
  }

  std::array<Entry, kCapacity> entries_{};
  size_t                       replacement_index_ = 0;
};
} // namespace repair_action_interlock
