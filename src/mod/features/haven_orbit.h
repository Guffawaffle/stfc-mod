#pragma once

struct OrbitFrameProvider;

namespace haven_orbit
{
// Called by the one shared OrbitFrameProvider.UpdateConstrains owner, after native constraints.
void ApplyConstraints(OrbitFrameProvider* provider);
} // namespace haven_orbit
