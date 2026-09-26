#pragma once

struct ShipTileWidget;
struct Il2CppObject;

namespace swap_ship_tile
{
bool IsInSelection(ShipTileWidget* widget);
Il2CppObject* SelectionContext(ShipTileWidget* widget);
} // namespace swap_ship_tile
