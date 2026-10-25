#pragma once
struct NavigationZoom;
namespace navigation_orbit
{
// Reapply after the ordinary zoom feature writes its configured native clip.
void ApplyDrawDistance(NavigationZoom* zoom);
} // namespace navigation_orbit
