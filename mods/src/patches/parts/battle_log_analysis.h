#pragma once

// Shares the existing ScreenManager.Update dispatcher; installs no detours.
void UpdateBattleAnalysis();
bool BattleAnalysisIsOpen();
