#pragma once
#include <optional>
struct RewardsButtonWidget;
namespace mining_preview
{
void Update();
bool Cycle();
// An active mining cycle owns its occupant's cargo policy; unrelated targets retain normal cargo settings.
std::optional<bool> AutomaticCargo(RewardsButtonWidget* widget);
} // namespace mining_preview
