#pragma once

// Shares the existing ScreenManager.Update dispatcher and intercepts the
// active report's Back handler while the analysis overlay is open.
void UpdateBattleAnalysis();
bool BattleAnalysisIsOpen();
bool InstallBattleAnalysisBackHook();
