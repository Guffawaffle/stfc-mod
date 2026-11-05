// Actual production host adapter, synthetic AAI installation/update functions.
// No game, native patch, worker or evidence decoding is simulated as qualified.
#include "features/features.h"
#include <il2cpp/hook.h>
#include <stfc_aai/overlay.h>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
stfc_aai::Enabled              live = nullptr;
stfc_aai::hooks::InstallReport report;
unsigned                       configurations{}, installs{}, updates{};
bool                           unsafe{}, inject_unsafe{};
void                           Require(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
Config& Config::Get()
{
  static Config config;
  return config;
}
namespace il2cpp
{
bool HookStateIndeterminate() noexcept
{ return unsafe; }
} // namespace il2cpp
namespace stfc_aai
{
void Configure(Enabled value)
{
  live = value;
  ++configurations;
}
hooks::InstallReport InstallHooks()
{
  ++installs;
  if (inject_unsafe) {
    hooks::Attempt contained;
    report.report_observer = contained.Once([]() -> hooks::Status {
      unsafe = true;
      throw il2cpp::HookInstallError({il2cpp::HookInstallState::kIndeterminate, spud::detour_error::kRollbackFailed});
    });
    Require(report.report_observer == hooks::Status::InstallException,
            "fixture did not reach the actual AAI exception-containment boundary");
  }
  return report;
}
void Update()
{ ++updates; }
} // namespace stfc_aai

int main(int argc, char** argv)
{
  try {
    using Status      = stfc_aai::hooks::Status;
    auto&      config = Config::Get();
    const auto mode   = argc > 1 ? std::string_view(argv[1]) : "complete";
    report            = {Status::Installed, Status::Installed};
    if (mode == "observer-missing")
      report.report_observer = Status::MissingMethod;
    if (mode == "back-missing")
      report.back = Status::DetourFailed;
    if (mode == "both-missing")
      report = {Status::MissingClass, Status::MissingClass};
    if (mode == "unsafe-contained") {
      inject_unsafe = true;
    }
    Require(kAaiReportObserver.group == &Config::Patches::battlelogsummaryhooks
                && kAaiBack.group == kAaiReportObserver.group && kBattleLogSummary.group == kAaiReportObserver.group,
            "original restart gate not shared by hook capabilities and frame service");
    if (inject_unsafe) {
      Require(!unsafe, "fixture already unsafe before the production adapter started installing");
      bool stopped = false;
      try {
        (void)kAaiReportObserver.install();
      } catch (const il2cpp::HookInstallError& error) {
        stopped = error.Result().Unsafe();
      }
      Require(stopped, "contained AAI failure lost host's unsafe installation boundary");
    } else {
      Require(kAaiReportObserver.install() == stfc_aai::hooks::Ready(report.report_observer),
              "observer status changed");
      Require(kAaiBack.install() == stfc_aai::hooks::Ready(report.back), "Back status changed");
      Require(kBattleLogSummary.install(), "safe degraded hooks removed bounded fallback frame service");
      Require(kBattleLogSummary.install() && configurations == 1 && installs == 1, "host repeated installation");
      Require(live && live(), "AAI live default was not preserved");
      config.ui.battle_log_summary_preview = false;
      Require(!live(), "AAI enable callback retained stale configuration");
      kBattleLogSummary.frame();
      Require(updates == 1, "disabled live state skipped AAI's teardown callback");
      config.ui.battle_log_summary_preview = true;
      Require(live(), "AAI re-enable requires reinstall");
      Require(!FrameDeliveryReady(InstallState::kInstalled, true, false),
              "registration claimed actual frame readiness");
    }
    std::cout << "Production AAI host adapter passed: " << mode << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
