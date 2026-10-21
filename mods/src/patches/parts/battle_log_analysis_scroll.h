#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace battle_analysis
{
struct ScrollWindow {
  std::size_t begin = 0, end = 0;
  float       offset = 0, extent = 0;
};

// Native ScrollRect owns motion. Retain only the visible rows plus one row on
// either side, even for a full Armada timeline at the reader's event limit.
inline ScrollWindow scroll_window(std::size_t count, float row, float viewport, float offset)
{
  if (!std::isfinite(row) || !std::isfinite(viewport) || row <= 0 || viewport <= 0)
    return {};
  const float extent = std::max(viewport, static_cast<float>(count) * row);
  offset             = std::isfinite(offset) ? std::clamp(offset, 0.f, extent - viewport) : 0.f;
  const auto first   = std::min(count, static_cast<std::size_t>(offset / row));
  const auto end     = std::min(count, static_cast<std::size_t>(std::ceil((offset + viewport) / row)) + 1);
  return {first ? first - 1 : 0, end, offset, extent};
}
} // namespace battle_analysis
