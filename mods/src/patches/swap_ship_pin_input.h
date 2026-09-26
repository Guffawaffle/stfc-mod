#pragma once

struct ShipTileWidget;

namespace swap_ship_pin_input
{
bool Install();
void RegisterTile(ShipTileWidget* tile);
void RefreshVisibleBadges();
bool HandleTileClick(ShipTileWidget* tile);
} // namespace swap_ship_pin_input
