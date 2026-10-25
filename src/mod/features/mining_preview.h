#pragma once
struct RewardsButtonWidget;
namespace mining_preview
{
void Update();
bool Cycle();
bool SuppressAutomaticCargo(RewardsButtonWidget* widget);
} // namespace mining_preview
