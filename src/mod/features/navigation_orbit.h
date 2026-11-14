#pragma once
struct NavigationZoom;
namespace navigation_orbit
{
// Runtime capability, independent of the saved preference and startup install record.
bool Available();
// Pan and NavigationOrbit share one MoveCamera detour, including when either hook group is disabled.
bool EnsurePanMovement();
// One game-thread camera-authoring span. Native camera callbacks still reapply
// their pose; clipping is completed after zoom's final write.
struct ZoomUpdateScope {
  explicit ZoomUpdateScope(NavigationZoom*);
  ZoomUpdateScope(const ZoomUpdateScope&)            = delete;
  ZoomUpdateScope& operator=(const ZoomUpdateScope&) = delete;
  ~ZoomUpdateScope() noexcept;
  void             Complete();
  NavigationZoom*  zoom;
  ZoomUpdateScope* previous;
  bool             active = true, pending = false, flushing = false;
};
// Reapply after the ordinary zoom feature writes its configured native clip.
void ApplyDrawDistance(NavigationZoom* zoom);
} // namespace navigation_orbit
