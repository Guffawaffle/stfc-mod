#pragma once

#include <optional>

struct ShipTileWidget;
struct Transform;

namespace ship_tech_indicators
{
enum class PinBadgeHighlight { None, Source, Target };

struct PinGroupBounds {
  float left, bottom, right, top;
};

bool Available();
void RefreshPinBadge(ShipTileWidget* widget);
void SetPinBadgeHighlight(ShipTileWidget* widget, PinBadgeHighlight highlight);
void UpdatePinGroupBand(Transform* content, std::optional<PinGroupBounds> bounds);
}
