/**
 * @file hotkey_router_native_fleet_guard.h
 * @brief Pure helpers for the native fleet-selection guard / bypass logic.
 *
 * These constexpr free functions own the per-frame slot-mask decision so the
 * underlying logic can be unit-tested without dragging in IL2CPP types.
 * `hotkey_router.cc` keeps the singleton state and delegates the actual
 * suppression/bypass decisions to the helpers below.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace hotkey_router_native_fleet
{
inline constexpr std::size_t kSlotCount = 8;

/**
 * @brief True when the per-frame native fleet-selection hook should be suppressed for @p index.
 *
 * Out-of-range indices, an unset slot, or an active bypass scope all return false.
 */
constexpr bool should_suppress(const std::int32_t index, std::span<const bool> slots, const bool bypassed) noexcept
{
  if (bypassed) {
    return false;
  }
  if (index < 0) {
    return false;
  }
  if (static_cast<std::size_t>(index) >= slots.size()) {
    return false;
  }
  return slots[static_cast<std::size_t>(index)];
}

/**
 * @brief True when any slot is guarded for a consumed non-selection chord.
 *
 * Returns false while a bypass scope is active so legitimate native selections
 * (initiated from inside the bypass) are not mis-classified as overlapping
 * suppression targets.
 */
constexpr bool should_suppress_any(std::span<const bool> slots, const bool bypassed) noexcept
{
  if (bypassed) {
    return false;
  }
  for (const auto slot : slots) {
    if (slot) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Saturating decrement used by `HotkeyRouterNativeFleetSelectionBypass` destructor.
 *
 * Pulled out so the RAII counter math can be exercised by tests without
 * needing the singleton or its translation-unit dependencies.
 */
constexpr int bypass_decrement(const int depth) noexcept
{
  return depth > 0 ? depth - 1 : depth;
}
}  // namespace hotkey_router_native_fleet
