#pragma once
struct NavigationZoom;
namespace navigation_orbit
{
// Runtime capability, independent of the saved preference and startup install record.
bool Available();
// Reapply after the ordinary zoom feature writes its configured native clip.
void ApplyDrawDistance(NavigationZoom* zoom);
} // namespace navigation_orbit
