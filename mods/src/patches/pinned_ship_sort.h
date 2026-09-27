#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

struct FleetPlayerData;
struct Il2CppObject;

namespace pinned_ship_sort
{
bool Available();
std::optional<std::size_t> Rank(FleetPlayerData* ship);
bool HandleCardAction(FleetPlayerData* ship, Il2CppObject* selection_context);
bool PlacePinnedShip(FleetPlayerData* source, std::optional<int64_t> target_id, Il2CppObject* selection_context);
} // namespace pinned_ship_sort
