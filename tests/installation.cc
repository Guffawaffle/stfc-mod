// Installer ownership and critical-versus-optional policy, without a game or real code patch.
#include "installation.h"

#include <iostream>
#include <stdexcept>

// This fixture exercises the process owner above HookSet, not the native patch engine. HookSet's separate fixture
// qualifies the real monotonic latch. No test resets or overrides production latch state.
namespace il2cpp
{
bool HookStateIndeterminate() noexcept
{ return false; }
} // namespace il2cpp

namespace
{
void Require(bool ok, const char* what)
{
  if (!ok)
    throw std::runtime_error(what);
}

void Policy()
{
  for (const auto state : {InstallState::kNotAttempted, InstallState::kInstalling, InstallState::kSkipped,
                           InstallState::kUnavailable, InstallState::kIndeterminate})
    Require(MustAbortInstallation(state, InstallRequirement::kRequired), "required incomplete phase must abort");
  Require(!MustAbortInstallation(InstallState::kInstalled, InstallRequirement::kRequired),
          "required success continues");
  Require(!MustAbortInstallation(InstallState::kUnavailable, InstallRequirement::kOptional),
          "optional refusal continues");
  Require(MustAbortInstallation(InstallState::kIndeterminate, InstallRequirement::kOptional), "unsafe optional aborts");
  Require(!FrameDeliveryReady(InstallState::kInstalled, true, false),
          "installed feature and registered callback do not imply a ready native anchor");
  Require(!FrameDeliveryReady(InstallState::kUnavailable, true, true),
          "unavailable feature cannot claim frame readiness");
  Require(!FrameDeliveryReady(InstallState::kInstalled, false, true),
          "missing registration prevents delivery readiness");
  Require(FrameDeliveryReady(InstallState::kInstalled, true, true), "frame readiness requires all three observations");
}

void Ownership()
{
  int          calls = 0;
  InstallGuard installed;
  Require(installed.Run(true,
                        [&] {
                          ++calls;
                          Require(installed.Run(true,
                                                [&] {
                                                  ++calls;
                                                  return true;
                                                })
                                      == InstallState::kInstalling,
                                  "reentrant request observes pending work");
                          return true;
                        })
              == InstallState::kInstalled,
          "owner succeeds");
  Require(installed.Run(true,
                        [&] {
                          ++calls;
                          return false;
                        })
                  == InstallState::kInstalled
              && calls == 1,
          "successful owner never repeats or loses result");

  InstallGuard skipped;
  Require(skipped.Run(false,
                      [&] {
                        ++calls;
                        return true;
                      })
              == InstallState::kSkipped,
          "disabled owner skipped");
  Require(skipped.Run(true,
                      [&] {
                        ++calls;
                        return true;
                      })
                  == InstallState::kSkipped
              && calls == 1,
          "restart-only disabled state does not later install");

  InstallGuard unavailable;
  Require(unavailable.Run(true, [] { return false; }) == InstallState::kUnavailable, "refusal retained");
  Require(unavailable.Run(true,
                          [&] {
                            ++calls;
                            return true;
                          })
                  == InstallState::kUnavailable
              && calls == 1,
          "refusal is terminal");

  InstallGuard thrown;
  bool         caught = false;
  try {
    thrown.Run(true, []() -> bool { throw std::runtime_error("fixture initialization"); });
    Require(false, "exception swallowed");
  } catch (const std::runtime_error&) {
    caught = true;
    Require(thrown.State() == InstallState::kUnavailable, "exception seals owner before propagation");
  }
  Require(caught, "initializer exception must propagate");
  Require(thrown.Run(true,
                     [&] {
                       ++calls;
                       return true;
                     })
                  == InstallState::kUnavailable
              && calls == 1,
          "throwing initializer is not retried");

  // A critical failure prevents the continuation; an independent optional failure does not.
  for (const auto requirement : {InstallRequirement::kOptional, InstallRequirement::kRequired}) {
    InstallGuard phase;
    int          continuation = 0;
    const auto   state        = phase.Run(true, [] { return false; });
    if (!MustAbortInstallation(state, requirement))
      ++continuation;
    Require(continuation == (requirement == InstallRequirement::kOptional ? 1 : 0), "critical continuation gate");
  }
}
} // namespace

int main()
{
  Policy();
  Ownership();
  std::cout << "Installer ownership and failure-policy tests passed\n";
}
