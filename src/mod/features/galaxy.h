#pragma once
#include <cstdint>
struct NavigationZoom;
struct NavigationLOD;
struct NavigationZoomEventHandler;
enum class ZoomLevels : int32_t;
namespace galaxy_controls
{
bool       LabelsAvailable();
bool       SelectionAvailable();
bool       HousingAvailable();
bool       StarbaseAvailable();
void       AfterZoom(NavigationZoom* zoom);
ZoomLevels LodLevel(NavigationLOD* lod, ZoomLevels native);
ZoomLevels VisibilityLevel(NavigationZoomEventHandler* handler, ZoomLevels native);
void       Refresh();
void       SetOverlay(int32_t mode, bool enabled);
} // namespace galaxy_controls
