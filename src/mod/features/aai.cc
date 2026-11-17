// ui.battle_log_summary_preview: pinned native AAI, on the host's one frame owner.
#include "features.h"

#if defined(STFC_AAI_ENABLED)
#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <stfc_aai/overlay.h>

namespace
{
const stfc_aai::hooks::InstallReport& AaiHooks()
{
  static const auto result = [] {
    stfc_aai::Configure([] { return Config::Get().ui.battle_log_summary_preview; });
    const auto report = stfc_aai::InstallHooks();
    // AAI's compatibility Attempt deliberately contains installer exceptions.
    // The host's monotonic latch must survive that boundary and stop startup.
    if (il2cpp::HookStateIndeterminate())
      throw il2cpp::HookInstallError({il2cpp::HookInstallState::kIndeterminate, spud::detour_error::kOk});
    spdlog::info("[BattleLogSummary] source={} ({}) report_observer={} back={} discovery={}; frame readiness is "
                 "reported independently by the host",
                 STFC_AAI_SOURCE_REVISION, STFC_AAI_SOURCE_STATE, stfc_aai::hooks::StatusName(report.report_observer),
                 stfc_aai::hooks::StatusName(report.back),
                 stfc_aai::hooks::Ready(report.report_observer) ? "native-observer" : "bounded-fallback");
    return report;
  }();
  return result;
}

bool InstallAaiReportObserver()
{ return stfc_aai::hooks::Ready(AaiHooks().report_observer); }

bool InstallAaiBack()
{ return stfc_aai::hooks::Ready(AaiHooks().back); }

bool PrepareAaiOverlay()
{
  (void)AaiHooks();
  return true; // Missing optional detours retain the original bounded fallback.
}
} // namespace

// Per-target capability status is independent from the frame service. A missing
// observer must not remove the native report fallback or claim Back is missing.
const Feature kAaiReportObserver{"AAIReportObserver", &Config::Patches::battlelogsummaryhooks,
                                 InstallAaiReportObserver};
const Feature kAaiBack{"AAIBack", &Config::Patches::battlelogsummaryhooks, InstallAaiBack};
const Feature kBattleLogSummary{"BattleLogSummary", &Config::Patches::battlelogsummaryhooks, PrepareAaiOverlay,
                                stfc_aai::Update};
#endif
