#pragma once

// Boundaries outside the actual camera/Galaxy consumers included by the map
// fixtures. No real scene, fleet-label registry, setting writer or orbit runs.

// Pan shortcut arbitration is outside this batch; the real implementation is
// covered by its own input fixtures, not this camera consumer host.
namespace Shortcut
{
bool Owns(unity::KeyCode)
{ return false; }
} // namespace Shortcut

namespace navigation_orbit
{
#ifndef STFC_ORBIT_FIXTURE
ZoomUpdateScope::ZoomUpdateScope(NavigationZoom* value)
    : zoom(value)
    , previous(nullptr)
{
}
ZoomUpdateScope::~ZoomUpdateScope() noexcept = default;
void ZoomUpdateScope::Complete() {}
void ApplyDrawDistance(NavigationZoom*) {}
#endif
} // namespace navigation_orbit

namespace haven_orbit
{
void ApplyConstraints(OrbitFrameProvider*) {}
} // namespace haven_orbit

namespace runtime_config
{
bool RegisterSetting(const char*, const char*) noexcept
{ return true; }
} // namespace runtime_config
