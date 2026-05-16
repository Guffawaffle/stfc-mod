#pragma once

#include "prime/KeyCode.h"

struct SpaceActionInputs {
  bool primary       = false;
  bool secondary     = false;
  bool recall        = false;
  bool repair        = false;
  bool queue         = false;
  bool queue_clear   = false;
  bool recall_cancel = false;

  KeyCode primary_key       = KeyCode::None;
  KeyCode secondary_key     = KeyCode::None;
  KeyCode recall_key        = KeyCode::None;
  KeyCode repair_key        = KeyCode::None;
  KeyCode queue_key         = KeyCode::None;
  KeyCode queue_clear_key   = KeyCode::None;
  KeyCode recall_cancel_key = KeyCode::None;

  [[nodiscard]] constexpr bool any_requested() const
  { return primary || secondary || recall || repair || queue || queue_clear || recall_cancel; }
};