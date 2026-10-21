#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace battle_analysis
{
struct ScrollWindow {
  std::size_t begin = 0, end = 0;
  float       offset = 0, extent = 0;
  float       maximum = 0;
  bool        before = false, after = false;
};

struct ScrollLayout {
  float leading = 0, visible = 0;
  bool  cues = false;
};

inline ScrollLayout scroll_layout(std::size_t count, float row, float available, float gutter)
{
  if (!std::isfinite(row) || !std::isfinite(available) || !std::isfinite(gutter) || row <= 0 || available <= 0
      || gutter < 0)
    return {};
  const bool  cues    = static_cast<float>(count) * row > available + .5f;
  const float leading = cues ? std::min(gutter, available * .25f) : 0;
  return {leading, available - 2 * leading, cues};
}

// Native ScrollRect owns motion. Retain only the visible rows plus one row on
// either side, even for a full Armada timeline at the reader's event limit.
inline ScrollWindow scroll_window(std::size_t count, float row, float viewport, float offset)
{
  if (!std::isfinite(row) || !std::isfinite(viewport) || row <= 0 || viewport <= 0)
    return {};
  const float extent    = std::max(viewport, static_cast<float>(count) * row);
  offset                = std::isfinite(offset) ? std::clamp(offset, 0.f, extent - viewport) : 0.f;
  const auto  first     = std::min(count, static_cast<std::size_t>(offset / row));
  const auto  end       = std::min(count, static_cast<std::size_t>(std::ceil((offset + viewport) / row)) + 1);
  const float maximum   = extent - viewport;
  const float tolerance = std::min(.5f, maximum * .01f);
  return {first ? first - 1 : 0, end, offset, extent, maximum, offset > tolerance, offset < maximum - tolerance};
}

inline float scroll_step(std::size_t count, float row, float viewport, float offset, bool forward)
{
  const auto window = scroll_window(count, row, viewport, offset);
  if ((!forward && !window.before) || (forward && !window.after))
    return window.offset;
  const float index =
      forward ? std::floor((window.offset + .5f) / row) + 1 : std::ceil((window.offset - .5f) / row) - 1;
  return std::clamp(index * row, 0.f, window.maximum);
}

// Reveal a bookmark with the smallest possible movement. Selecting an already
// visible shot must not recenter the strip or change the active round filter.
inline float scroll_reveal(std::size_t count, float row, float viewport, float offset, std::size_t index)
{
  const auto window = scroll_window(count, row, viewport, offset);
  if (index >= count || !std::isfinite(row) || !std::isfinite(viewport) || row <= 0 || viewport <= 0)
    return window.offset;
  const float start  = static_cast<float>(index) * row;
  const float wanted = start < window.offset                    ? start
                       : start + row > window.offset + viewport ? start + row - viewport
                                                                : window.offset;
  return std::clamp(wanted, 0.f, window.maximum);
}
} // namespace battle_analysis
