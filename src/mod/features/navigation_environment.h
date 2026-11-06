#pragma once
namespace unity
{
struct Camera;
}
namespace navigation_environment
{
void Clear();
// Orbit owns all clip extensions so reset/disable can restore their shared baseline.
using ClipWriter = bool (*)(unity::Camera*, float baseline, float expanded);
void ApplyDrawDistance(unity::Camera* camera, ClipWriter write_clip);
void Update(unity::Camera* camera, ClipWriter write_clip);
} // namespace navigation_environment
