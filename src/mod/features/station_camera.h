#pragma once

#include "features/camera_orbit_input.h"

#include <unity/core.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace station_camera
{
constexpr float radians = std::numbers::pi_v<float> / 180.0f;

inline unity::Vector3 Forward(unity::Quaternion q)
{ return {2 * (q.x * q.z + q.w * q.y), 2 * (q.y * q.z - q.w * q.x), 1 - 2 * (q.x * q.x + q.y * q.y)}; }

inline unity::Quaternion Multiply(unity::Quaternion a, unity::Quaternion b)
{
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

struct View {
  float yaw = 0, tilt = 0;

  bool Active() const
  { return yaw != 0 || tilt != 0; }

  void DragOrbit(unity::Vector2 delta, unity::Quaternion native)
  {
    const auto pitch = std::asin(std::clamp(-Forward(native).y, -1.0f, 1.0f)) / radians;
    ApplyOrbitDrag(yaw, tilt, delta.x, delta.y, 15.0f - pitch, 85.0f - pitch);
  }

  unity::Quaternion Rotation(unity::Quaternion native) const
  {
    if (yaw == 0 && tilt == 0)
      return native;
    const auto pitch    = std::asin(std::clamp(-Forward(native).y, -1.0f, 1.0f)) / radians;
    const auto tilt_rad = (std::clamp(pitch + tilt, 15.0f, 85.0f) - pitch) * radians * .5f;
    const auto yaw_rad  = yaw * radians * .5f;
    const auto turn     = unity::Quaternion{0, std::sin(yaw_rad), 0, std::cos(yaw_rad)};
    const auto tip      = unity::Quaternion{std::sin(tilt_rad), 0, 0, std::cos(tilt_rad)};
    return Multiply(Multiply(turn, native), tip);
  }

  static unity::Vector3 Position(unity::Vector3 eye, unity::Quaternion native, unity::Quaternion look, float distance)
  {
    // Pivot around the native blended target, including any native pan, at the final zoom distance.
    const auto from = Forward(native), to = Forward(look);
    return {eye.x + (from.x - to.x) * distance, eye.y + (from.y - to.y) * distance, eye.z + (from.z - to.z) * distance};
  }
};
} // namespace station_camera
