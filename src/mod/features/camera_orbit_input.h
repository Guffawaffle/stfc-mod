#pragma once

#include <algorithm>
#include <cmath>

// Pitch can be absolute or relative to the native view. The caller supplies bounds in the same space.
inline void ApplyOrbitDrag(float& yaw, float& pitch, float horizontal, float vertical, float min_pitch, float max_pitch)
{
  constexpr float degrees_per_pixel = 0.15f;
  yaw                               = std::remainder(yaw + horizontal * degrees_per_pixel, 360.0f);
  pitch                             = std::clamp(pitch - vertical * degrees_per_pixel, min_pitch, max_pitch);
}
