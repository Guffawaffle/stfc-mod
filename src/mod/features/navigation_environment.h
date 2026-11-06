#pragma once
namespace unity
{
struct Camera;
struct Transform;
struct Vector3;
} // namespace unity
namespace navigation_environment
{
void Clear();
// Orbit owns all clip extensions so reset/disable can restore their shared baseline.
using ClipWriter = bool (*)(unity::Camera*, float baseline, float expanded);
void ApplyDrawDistance(unity::Camera* camera, unity::Vector3 position, float clip, ClipWriter write_clip);
void Update(unity::Camera* camera, unity::Transform* transform);
} // namespace navigation_environment
