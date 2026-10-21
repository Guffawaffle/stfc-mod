// Downstream integration only. AAI owns its implementation in stfc-aai.
#ifdef STFC_AAI_ENABLED
#include "config.h"
#include "patches/screen_update_hook.h"
#include <spdlog/spdlog.h>
#include <stfc_aai/overlay.h>

void InstallBattleLogSummary()
{
  stfc_aai::Configure([] { return Config::Get().battle_log_summary_preview; });
  const bool backReady = stfc_aai::InstallBackHook();
  if (install_screen_manager_update_hook()) {
    register_screen_manager_update_callback(stfc_aai::Update);
    spdlog::info(
        "After-Action Intelligence registered (ui.battle_log_summary_preview); native Back ready={}; source={} ({})",
        backReady, STFC_AAI_SOURCE_REVISION, STFC_AAI_SOURCE_STATE);
  }
}
#else
void InstallBattleLogSummary() {}
#endif
