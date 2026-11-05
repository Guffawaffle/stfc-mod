#pragma once
namespace unity
{
struct Camera;
}
namespace navigation_environment
{
void Clear();
void ApplyDrawDistance(unity::Camera* camera);
void Update(unity::Camera* camera);
} // namespace navigation_environment
