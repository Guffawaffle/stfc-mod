#pragma once

struct RewardsButtonWidget;

namespace mining_preview
{
// Runs on the existing shortcut update thread; installs no additional hooks.
void Update();
bool Cycle();
bool SuppressAutomaticCargo(RewardsButtonWidget* widget);
} // namespace mining_preview
