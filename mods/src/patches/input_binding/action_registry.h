#pragma once

#include "patches/input_binding/input_binding.h"

#include <span>
#include <string_view>

namespace input_binding
{
[[nodiscard]] std::span<const ActionSpec> ActionRegistry();
[[nodiscard]] const ActionSpec*           FindAction(InputActionId id);
[[nodiscard]] const ActionSpec*           FindAction(std::string_view canonical_key);
[[nodiscard]] const ActionSpec*           FindActionByGameFunction(GameFunction game_function);
[[nodiscard]] GameFunction                ActionGameFunction(InputActionId id);
[[nodiscard]] ActionCompositionSpec       ActionComposition(InputActionId id);
[[nodiscard]] std::string_view            CompositionModeName(CompositionMode mode);
[[nodiscard]] std::string_view            CompositionGroupName(CompositionGroup group);

} // namespace input_binding
