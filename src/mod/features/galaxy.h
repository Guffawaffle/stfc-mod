#pragma once
#include "game/navigation.h"
namespace galaxy_controls
{
bool       LabelsAvailable();
bool       SelectionAvailable();
bool       HousingAvailable();
void       AfterZoom(NavigationZoom* zoom);
ZoomLevels LodLevel(NavigationLOD* lod, ZoomLevels native);
void       Refresh();
void       SetOverlay(int32_t mode, bool enabled);
} // namespace galaxy_controls
