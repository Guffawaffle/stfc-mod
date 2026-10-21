#pragma once

#include <algorithm>
#include <cmath>

namespace battle_analysis
{
// Movement cancels a gesture only within a scroll region. Fixed controls use
// press/release hit identity instead, matching ordinary button behavior.
struct PointerGesture {
  bool  active = false, scrollable = false, dragged = false;
  float x = 0, y = 0, threshold = 5;

  void press(float px, float py, bool can_scroll, float drag_threshold)
  {
    active     = true;
    scrollable = can_scroll;
    dragged    = false;
    x          = px;
    y          = py;
    threshold  = std::isfinite(drag_threshold) ? std::max(1.f, drag_threshold) : 5.f;
  }
  void move(float px, float py)
  {
    if (active && scrollable && (px - x) * (px - x) + (py - y) * (py - y) > threshold * threshold)
      dragged = true;
  }
  bool release(float px, float py)
  {
    move(px, py);
    const bool click = active && !dragged;
    cancel();
    return click;
  }
  void cancel()
  { active = scrollable = dragged = false; }
};
} // namespace battle_analysis
