#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

// Direction-authored ambient sky. Native system artwork is deliberately not
// sampled: a ship or portal painted for an open mesh is not a sky panorama.
namespace navigation_environment_art
{
struct Direction {
  double x, y, z;
};
struct Pixel {
  uint8_t r, g, b, a;
};
enum class Theme { Cool, Warm, Purple, Vger };

inline uint32_t Hash(uint32_t value)
{
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  return value ^ (value >> 16);
}

inline uint32_t Seed(std::string_view name)
{
  uint32_t value = 2166136261u;
  for (unsigned char c : name) {
    value ^= c;
    value *= 16777619u;
  }
  return value;
}

inline Theme SelectTheme(std::string_view name)
{
  // Only choose a palette; native renderer selection and visibility never
  // depend on these names. Unknown client artwork gets a restrained cool sky.
  if (name.find("Vger") != name.npos || name.find("VGer") != name.npos)
    return Theme::Vger;
  if (name.find("Mirror") != name.npos || name.find("Purple") != name.npos)
    return Theme::Purple;
  if (name.find("Coffee") != name.npos || name.find("Orange") != name.npos || name.find("Red") != name.npos)
    return Theme::Warm;
  return Theme::Cool;
}

inline double Lattice(int x, int y, int z, uint32_t seed)
{
  return double(Hash(uint32_t(x) * 0x8da6b343u ^ uint32_t(y) * 0xd8163841u ^ uint32_t(z) * 0xcb1ab31fu ^ seed))
         / 4294967295.0;
}

inline double Noise(Direction point, uint32_t seed)
{
  const int    x = int(std::floor(point.x)), y = int(std::floor(point.y)), z = int(std::floor(point.z));
  auto         smooth = [](double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); };
  const double dx = smooth(point.x - x), dy = smooth(point.y - y), dz = smooth(point.z - z);
  const auto   row = [&](int yy, int zz) {
    return std::lerp(Lattice(x, yy, zz, seed), Lattice(x + 1, yy, zz, seed), dx);
  };
  return std::lerp(std::lerp(row(y, z), row(y + 1, z), dy), std::lerp(row(y, z + 1), row(y + 1, z + 1), dy), dz);
}

inline Direction SkyDirection(double u, double v)
{
  constexpr double pi = 3.14159265358979323846;
  // Explicit poles and longitude closure avoid floating-point noise seams.
  if (v <= 0.0)
    return {0.0, 1.0, 0.0};
  if (v >= 1.0)
    return {0.0, -1.0, 0.0};
  const double theta = pi * v, phi = 2.0 * pi * (u - std::floor(u));
  return {std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
}

inline Pixel Sample(Direction direction, Theme theme, uint32_t seed)
{
  double clouds = 0.0, weight = 0.55, frequency = 2.2;
  for (int octave = 0; octave < 5; ++octave) {
    clouds += weight
              * Noise({direction.x * frequency + 3.1, direction.y * frequency - 1.7, direction.z * frequency + 8.2},
                      seed + uint32_t(octave) * 977u);
    weight *= 0.5;
    frequency *= 2.05;
  }
  // A broad asymmetric band and wisps, sampled in 3D, close smoothly at every
  // longitude and pole. This is distant artwork, not additional world geometry.
  const double band    = std::exp(-std::pow((direction.y + 0.24 * direction.x - 0.12 * direction.z) * 2.4, 2.0));
  const double density = std::pow(std::max(0.0, clouds - 0.28), 1.6) * (0.3 + 0.7 * band);
  double       red = 26.0, green = 57.0, blue = 91.0, strength = 1.0;
  if (theme == Theme::Warm) {
    red   = 104.0;
    green = 48.0;
    blue  = 33.0;
  } else if (theme == Theme::Purple) {
    red   = 70.0;
    green = 36.0;
    blue  = 103.0;
  } else if (theme == Theme::Vger) {
    strength = 0.25;
  }
  // Sparse small stars also live in direction space, so the poles do not
  // accumulate the dense rows typical of a randomly painted UV rectangle.
  const double star_scale = 150.0;
  const int    sx = int(std::floor(direction.x * star_scale)), sy = int(std::floor(direction.y * star_scale)),
               sz       = int(std::floor(direction.z * star_scale));
  double       star     = 0.0;
  const double selector = Lattice(sx, sy, sz, seed ^ 0x53544152u);
  if (selector > 0.987) {
    const double dx = direction.x * star_scale - sx - 0.5, dy = direction.y * star_scale - sy - 0.5,
                 dz = direction.z * star_scale - sz - 0.5;
    star            = 160.0 * std::exp(-(dx * dx + dy * dy + dz * dz) * 22.0);
  }
  auto channel = [](double value) { return uint8_t(std::clamp(std::lround(value), 0l, 255l)); };
  return {channel(1.5 + red * density * strength + star), channel(2.0 + green * density * strength + star * 0.94),
          channel(3.0 + blue * density * strength + star * 0.86), 255};
}

inline std::vector<Pixel> Generate(int width, int height, Theme theme, uint32_t seed)
{
  if (width < 2 || height < 2 || width > 2048 || height > 1024)
    return {};
  std::vector<Pixel> pixels(size_t(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width - 1; ++x)
      pixels[size_t(y) * width + x] =
          Sample(SkyDirection(double(x) / (width - 1), double(y) / (height - 1)), theme, seed);
    pixels[size_t(y) * width + width - 1] = pixels[size_t(y) * width];
  }
  return pixels;
}

// A sphere radius beyond the farthest corner of any restored decorative bounds
// places all native scenery in front of the ambient sky. It expands rendering
// reach only; it cannot move, clamp, or recenter the camera.
inline float SceneryFarClip(float baseline, Direction camera, Direction center, Direction extents)
{
  if (!std::isfinite(baseline) || baseline <= 0.0f || !std::isfinite(camera.x) || !std::isfinite(camera.y)
      || !std::isfinite(camera.z) || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)
      || !std::isfinite(extents.x) || !std::isfinite(extents.y) || !std::isfinite(extents.z) || extents.x < 0.0
      || extents.y < 0.0 || extents.z < 0.0)
    return baseline;
  const double radius = std::hypot(std::abs(camera.x - center.x) + extents.x, std::abs(camera.y - center.y) + extents.y,
                                   std::abs(camera.z - center.z) + extents.z);
  const double expanded = (radius + 32.0) / 0.95;
  return std::isfinite(expanded) && expanded <= 1.0e8 ? float(std::max(double(baseline), expanded)) : baseline;
}
} // namespace navigation_environment_art
