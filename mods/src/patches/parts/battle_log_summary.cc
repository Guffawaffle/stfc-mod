#include "battle_log_analysis.h"
#include "patches/screen_update_hook.h"

#include <spdlog/spdlog.h>

// Preserve the existing patch/config names for users of the summary POC. The
// Command Overlay replaces its inline panel and never resizes the native log.
void InstallBattleLogSummary()
{
  const bool backReady = InstallBattleAnalysisBackHook();
  if (install_screen_manager_update_hook()) {
    register_screen_manager_update_callback(UpdateBattleAnalysis);
    spdlog::info("After-Action Intelligence registered (ui.battle_log_summary_preview); native Back ready={}",
                 backReady);
  }
}
