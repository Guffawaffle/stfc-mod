#pragma once

#include <cstdint>
#include <optional>

struct FleetPlayerData;
struct ShipManagementViewContext;
struct ShipTileWidget;

namespace ship_pins
{
bool InputReady();
bool IsPinned(FleetPlayerData* ship);
bool Toggle(FleetPlayerData* ship, ShipManagementViewContext* context);
bool Place(FleetPlayerData* ship, std::optional<int64_t> target, ShipManagementViewContext* context);
} // namespace ship_pins

namespace ship_pin_input
{
bool Install();
bool Available();
bool ConsumeTileClick(ShipTileWidget* tile);
void RefreshVisible();
void Tick();
} // namespace ship_pin_input
