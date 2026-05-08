#pragma once

struct SpaceActionInputs {
  bool primary = false;
  bool secondary = false;
  bool recall = false;
  bool repair = false;
  bool queue = false;
  bool queue_clear = false;
  bool recall_cancel = false;

  [[nodiscard]] constexpr bool any_requested() const
  {
    return primary || secondary || recall || repair || queue || queue_clear || recall_cancel;
  }
};