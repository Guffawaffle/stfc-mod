#pragma once

#include "ui.h"

namespace streamer_mode::game
{
struct PlayerIdDisplay : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.UI", "PlayerIdViewController">;
  TextLocalizer* Label()
  { return Field<"_textLocalizer", TextLocalizer*>(); }
};
} // namespace streamer_mode::game
