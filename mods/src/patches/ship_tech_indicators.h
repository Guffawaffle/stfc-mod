#pragma once

#include <optional>

struct ShipTileWidget;
struct Transform;

namespace ship_tech_indicators
{
enum class PinBadgeHighlight { None, Source, Target, UnpinTarget };

struct PinGroupBounds {
  float left, bottom, right, top;
};

bool Available();
void RefreshPinBadge(ShipTileWidget* widget);
void SetPinBadgeHighlight(ShipTileWidget* widget, PinBadgeHighlight highlight);
void SetFirstPinDropCue(Transform* viewport, bool visible, bool hovered);
void SetPinGroupHover(Transform* content, bool hovered);
void UpdatePinGroupBand(Transform* content, std::optional<PinGroupBounds> bounds);
}
