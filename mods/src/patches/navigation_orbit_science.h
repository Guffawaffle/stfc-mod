#pragma once

// Temporary play-dev observation points. No hooks, camera writes, or release code.
namespace navigation_orbit_science
{
#ifdef _MODDBG
void Orbit(void *zoom, const char *phase, float yaw, float pitch, bool overridden, bool dragging, bool force = false);
void Pan(void *pan, const char *phase);
#else
inline void Orbit(void *, const char *, float, float, bool, bool, bool = false) {}
inline void Pan(void *, const char *) {}
#endif
} // namespace navigation_orbit_science
