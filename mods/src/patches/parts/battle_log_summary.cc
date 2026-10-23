// Downstream integration only. AAI owns its implementation in stfc-aai.
#ifdef STFC_AAI_ENABLED
#include "config.h"
#include "patches/screen_update_hook.h"
#include <spdlog/spdlog.h>
#include <stfc_aai/overlay.h>

void InstallBattleLogSummary()
{
  stfc_aai::Configure([] { return Config::Get().battle_log_summary_preview; });
  const auto hooks           = stfc_aai::InstallHooks();
  const bool dispatcherReady = install_screen_manager_update_hook();
  const bool callbackReady   = dispatcherReady && register_screen_manager_update_callback(stfc_aai::Update);
  if (callbackReady) {
    spdlog::info("After-Action Intelligence registered (ui.battle_log_summary_preview={}); report_observer={} back={} "
                 "update_dispatch=ready callback=registered discovery={}; source={} ({})",
                 Config::Get().battle_log_summary_preview, stfc_aai::hooks::StatusName(hooks.report_observer),
                 stfc_aai::hooks::StatusName(hooks.back),
                 stfc_aai::hooks::Ready(hooks.report_observer) ? "native-observer" : "bounded-fallback",
                 STFC_AAI_SOURCE_REVISION, STFC_AAI_SOURCE_STATE);
    if (!hooks.Complete())
      spdlog::warn("AAI registered with degraded native capabilities; inspect per-target hook statuses above");
  } else
    spdlog::warn("After-Action Intelligence unavailable: report_observer={} back={} update_dispatch={} callback={}; "
                 "source={} ({})",
                 stfc_aai::hooks::StatusName(hooks.report_observer), stfc_aai::hooks::StatusName(hooks.back),
                 dispatcherReady ? "ready" : "failed", dispatcherReady ? "registration-failed" : "not-attempted",
                 STFC_AAI_SOURCE_REVISION, STFC_AAI_SOURCE_STATE);
}
#else
void InstallBattleLogSummary() {}
#endif
