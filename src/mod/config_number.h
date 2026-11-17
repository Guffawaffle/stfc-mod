#pragma once

#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace config_number
{
// TOML stores doubles. Keep genuine double settings unchanged.
inline double ForToml(double value)
{ return value; }

// Preserve the float on reload without persisting the decimal expansion produced
// by widening its binary representation. Canonicalize before save/status tracking
// so the writer compares exactly the same double that it writes to the file.
inline double ForToml(float value)
{
  if (!std::isfinite(value))
    return static_cast<double>(value); // Existing consumers retain their nonfinite validation.
  char       text[64];
  const auto written = std::to_chars(text, text + sizeof(text), value);
  if (written.ec != std::errc{})
    throw std::runtime_error("could not format float setting");
  // Floating from_chars is unavailable on the oldest supported macOS runtime.
  std::istringstream input{std::string(text, written.ptr)};
  input.imbue(std::locale::classic());
  double number{};
  input >> std::noskipws >> number;
  if (!input || input.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("could not encode float setting");
  // FLT_MAX's shortest rounded decimal lies just beyond its exact value; TOML's
  // checked float reader rejects it. Keep that boundary lossless as well.
  if (std::abs(number) > static_cast<double>(std::numeric_limits<float>::max()))
    return static_cast<double>(value);
  return number;
}
} // namespace config_number
