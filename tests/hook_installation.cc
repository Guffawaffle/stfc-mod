// Production HookSet over SPUD's public generic-hook seam. These fakes do not
// patch code, suspend threads, change SPUD safety flags or load a managed runtime.
// Unsafe cases run as separate argv modes because the process latch has no reset.
#include "installation.h"
#include <il2cpp/hook.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fixture
{
using Error = spud::detour_error;
using State = il2cpp::HookInstallState;
std::vector<std::string> reports;

void Require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

struct HookState {
  std::string                               name;
  std::shared_ptr<std::vector<std::string>> trace;
  Error                                     install_error       = Error::kOk;
  Error                                     uninstall_error     = Error::kOk;
  bool                                      installed           = false;
  bool                                      mutate_before_error = false;
  bool                                      throw_install       = false;
  bool                                      throw_detach        = false;
  unsigned                                  installs = 0, uninstalls = 0, queries = 0, detaches = 0, destroys = 0;
};

// The required methods are the unmodified spud::installable_hook interface;
// detach is HookSet's normal post-commit ownership operation.
struct FakeHook {
  std::shared_ptr<HookState> state;
  explicit FakeHook(std::shared_ptr<HookState> value)
      : state(std::move(value))
  {
  }
  FakeHook(FakeHook&&) noexcept            = default;
  FakeHook& operator=(FakeHook&&) noexcept = default;
  FakeHook(const FakeHook&)                = delete;
  FakeHook& operator=(const FakeHook&)     = delete;
  ~FakeHook()
  {
    if (state)
      ++state->destroys;
  }
  Error try_install()
  {
    ++state->installs;
    state->trace->push_back(state->name + ".install");
    if (state->throw_install)
      throw std::runtime_error("synthetic install exception");
    if (state->mutate_before_error)
      state->installed = true;
    if (state->install_error != Error::kOk)
      return state->install_error;
    state->installed = true;
    return Error::kOk;
  }
  Error uninstall()
  {
    ++state->uninstalls;
    state->trace->push_back(state->name + ".uninstall");
    if (state->uninstall_error != Error::kOk)
      return state->uninstall_error;
    state->installed = false;
    return Error::kOk;
  }
  bool is_installed()
  {
    ++state->queries;
    return state->installed;
  }
  void detach()
  {
    ++state->detaches;
    state->trace->push_back(state->name + ".detach");
    if (state->throw_detach)
      throw std::runtime_error("synthetic detach exception");
  }
};
static_assert(spud::installable_hook<FakeHook>);

using Trace = std::shared_ptr<std::vector<std::string>>;
std::shared_ptr<HookState> Make(const char* name, const Trace& trace)
{
  auto state   = std::make_shared<HookState>();
  state->name  = name;
  state->trace = trace;
  return state;
}

void RequireCached(il2cpp::HookSet& set, il2cpp::HookInstallResult first, const Trace& trace)
{
  const auto calls        = trace->size();
  const auto report_count = reports.size();
  const auto again        = set.CommitDetailed();
  Require(again.state == first.state && again.error == first.error, "repeat changed the terminal result");
  bool saw_unsafe = false;
  try {
    Require(set.Commit() == first.Installed(), "bool Commit disagrees with the qualified result");
  } catch (const il2cpp::HookInstallError& error) {
    saw_unsafe = true;
    Require(first.Unsafe() && error.Result().Unsafe() && error.Result().state == first.state
                && error.Result().error == first.error,
            "bool Commit lost the unsafe result");
  }
  Require(saw_unsafe == first.Unsafe(), "bool Commit did not throw exactly for unsafe state");
  Require(trace->size() == calls && reports.size() == report_count, "cached commit retried work or reported twice");
  bool missing_rejected = false;
  try {
    set.Missing();
  } catch (const std::logic_error&) {
    missing_rejected = true;
  }
  Require(missing_rejected, "Missing mutated a terminal set");
  bool     stage_rejected = false;
  auto     late_state     = Make("late", trace);
  FakeHook late(late_state);
  try {
    set.Stage(std::move(late));
  } catch (const std::logic_error&) {
    stage_rejected = true;
  }
  Require(stage_rejected && late.state == late_state && trace->size() == calls && reports.size() == report_count,
          "Stage consumed custody, mutated or reported a terminal set");
}

void SafeCases()
{
  Require(!il2cpp::HookStateIndeterminate(), "a fresh test process is already unsafe");
  const il2cpp::HookInstallResult initial;
  Require(initial.state == State::kNotAttempted && initial.error == Error::kOk && !initial.Installed()
              && !initial.Unsafe(),
          "default qualification is not unattempted");
  {
    auto            trace = std::make_shared<std::vector<std::string>>();
    il2cpp::HookSet empty("Empty");
    const auto      result = empty.CommitDetailed();
    Require(result.state == State::kEmpty && !result.Installed() && !result.Unsafe(), "empty set claimed installation");
    RequireCached(empty, result, trace);
  }
  for (unsigned missing = 0; missing < 3; ++missing) {
    auto                                    trace = std::make_shared<std::vector<std::string>>();
    std::vector<std::shared_ptr<HookState>> states;
    il2cpp::HookSet                         set("MissingTarget");
    const auto                              before = reports.size();
    for (unsigned index = 0; index < 3; ++index) {
      if (index == missing)
        set.Missing();
      else {
        states.push_back(Make(index == 0 ? "A" : "B", trace));
        set.Stage(FakeHook(states.back()));
      }
    }
    const auto result = set.CommitDetailed();
    Require(result.state == State::kMissingTarget && !result.Unsafe() && trace->empty(),
            "an Nth missing target allowed an install");
    Require(reports.size() == before + 1, "missing set did not report once");
    for (const auto& state : states)
      Require(!state->installed && state->installs == 0 && state->detaches == 0, "missing set touched a hook");
    RequireCached(set, result, trace);
  }
  {
    auto            trace = std::make_shared<std::vector<std::string>>();
    auto            a = Make("A", trace), b = Make("B", trace), c = Make("C", trace);
    il2cpp::HookSet set("Success");
    auto            retained = set.StageRetained(FakeHook(a));
    set.Stage(FakeHook(b));
    set.Stage(FakeHook(c));
    const auto result = set.CommitDetailed();
    Require(result.state == State::kInstalled && result.Installed() && !result.Unsafe(),
            "complete set was not installed");
    Require(
        *trace == std::vector<std::string>({"A.install", "B.install", "C.install", "A.detach", "B.detach", "C.detach"}),
        "success did not install then detach exactly once in order");
    for (const auto& state : {a, b, c})
      Require(state->installed && state->installs == 1 && state->uninstalls == 0 && state->detaches == 1,
              "successful hook lifecycle changed");
    Require(a->destroys == 0 && retained->state == a, "retained staged handle lost post-commit query lifetime");
    retained.reset();
    Require(a->destroys == 1 && a->uninstalls == 0, "dropping a detached query handle changed installed ownership");
    RequireCached(set, result, trace);
  }
  for (unsigned failing = 0; failing < 3; ++failing) {
    auto                                    trace = std::make_shared<std::vector<std::string>>();
    std::vector<std::shared_ptr<HookState>> states{Make("A", trace), Make("B", trace), Make("C", trace)};
    states[failing]->install_error = Error::kDecodeFailed;
    il2cpp::HookSet set("Rejected");
    for (const auto& state : states)
      set.Stage(FakeHook(state));
    const auto before = reports.size();
    const auto result = set.CommitDetailed();
    Require(result.state == State::kRejected && result.error == Error::kDecodeFailed && !result.Unsafe(),
            "clean refusal lost its qualified error");
    std::vector<std::string> expected;
    for (unsigned index = 0; index <= failing; ++index)
      expected.push_back(states[index]->name + ".install");
    for (unsigned index = failing; index > 0; --index)
      expected.push_back(states[index - 1]->name + ".uninstall");
    Require(*trace == expected, "Nth install failure did not roll back newest first");
    Require(reports.size() == before + 1, "clean refusal did not report once");
    for (const auto& state : states)
      Require(!state->installed && state->detaches == 0, "refused set left an installed or detached new hook");
    RequireCached(set, result, trace);
  }
  {
    auto trace = std::make_shared<std::vector<std::string>>();
    auto a = Make("Existing", trace), b = Make("Refused", trace);
    a->installed     = true;
    b->install_error = Error::kDecodeFailed;
    il2cpp::HookSet set("Preinstalled");
    set.Stage(FakeHook(a));
    set.Stage(FakeHook(b));
    const auto result = set.CommitDetailed();
    Require(result.state == State::kRejected && !result.Unsafe() && a->installed && !b->installed,
            "clean refusal changed a preinstalled member");
    Require(a->uninstalls == 0 && a->detaches == 1 && b->detaches == 0,
            "preinstalled ownership was not preserved before cleanup");
    RequireCached(set, result, trace);
  }
  {
    auto trace          = std::make_shared<std::vector<std::string>>();
    auto existing       = Make("Existing", trace);
    existing->installed = true;
    il2cpp::HookSet set("MissingWithPreinstalled");
    set.Stage(FakeHook(existing));
    set.Missing();
    const auto result = set.CommitDetailed();
    Require(result.state == State::kMissingTarget && !result.Unsafe() && existing->installed,
            "missing target changed a preinstalled member");
    Require(existing->installs == 0 && existing->uninstalls == 0 && existing->detaches == 1,
            "missing target did not preserve preinstalled ownership");
    RequireCached(set, result, trace);
  }
  Require(!il2cpp::HookStateIndeterminate(), "safe cases latched the process unsafe");
}

void UnsafeCase(std::string_view mode)
{
  Require(!il2cpp::HookStateIndeterminate(), "unsafe modes must start in a fresh process");
  InstallGuard installed_before, skipped_before;
  unsigned     guarded_calls = 0;
  Require(installed_before.Run(true,
                               [&] {
                                 ++guarded_calls;
                                 return true;
                               })
              == InstallState::kInstalled,
          "safe prerequisite owner did not install");
  Require(skipped_before.Run(false, [] { return true; }) == InstallState::kSkipped,
          "safe prerequisite owner did not skip");
  auto            historical_trace = std::make_shared<std::vector<std::string>>();
  auto            historical_hook  = Make("Historical", historical_trace);
  il2cpp::HookSet historical("HistoricalSuccess");
  historical.Stage(FakeHook(historical_hook));
  const auto historical_result = historical.CommitDetailed();
  Require(historical_result.Installed() && !historical_result.Unsafe(), "historical prerequisite did not install");
  auto trace = std::make_shared<std::vector<std::string>>();
  auto a = Make("A", trace), b = Make("B", trace), c = Make("C", trace);
  if (mode == "rollback-failed") {
    b->uninstall_error = Error::kProtectionFailed;
    c->install_error   = Error::kDecodeFailed;
  } else if (mode == "install-throws") {
    b->throw_install = true;
  } else if (mode == "mutation-then-error") {
    b->mutate_before_error = true;
    b->install_error       = Error::kDecodeFailed;
  } else if (mode == "protection-failed") {
    b->install_error = Error::kProtectionFailed;
  } else if (mode == "suspension-unavailable") {
    b->install_error = Error::kSuspensionUnavailable;
  } else if (mode == "unknown-error") {
    b->install_error = static_cast<Error>(0x7f001);
  } else if (mode == "detach-throws") {
    a->throw_detach = true;
  } else {
    throw std::runtime_error("unknown unsafe mode");
  }
  {
    il2cpp::HookSet set("Unsafe");
    set.Stage(FakeHook(a));
    set.Stage(FakeHook(b));
    set.Stage(FakeHook(c));
    const auto                before = reports.size();
    InstallGuard              owner;
    il2cpp::HookInstallResult result;
    bool                      propagated   = false;
    InstallState              owner_result = InstallState::kNotAttempted;
    try {
      owner_result = owner.Run(true, [&] {
        if (mode == "detach-throws")
          return set.Commit();
        result = set.CommitDetailed();
        return result.Installed();
      });
    } catch (const il2cpp::HookInstallError& error) {
      propagated   = true;
      result       = error.Result();
      owner_result = owner.State();
    }
    Require(propagated == (mode == "detach-throws"), "unsafe bool Commit did not propagate through its real owner");
    Require(owner_result == InstallState::kIndeterminate && owner.State() == InstallState::kIndeterminate,
            "real installer owner lost the unsafe process result");
    Require(MustAbortInstallation(owner_result, InstallRequirement::kOptional)
                && MustAbortInstallation(owner_result, InstallRequirement::kRequired),
            "unsafe owner was treated as an optional skip");
    Require(result.state == State::kIndeterminate && result.Unsafe() && !result.Installed(),
            "uncertain native state was reported as a clean refusal");
    Require(il2cpp::HookStateIndeterminate(), "unsafe result did not latch the process");
    Require(reports.size() == before + 1, "unsafe result did not report once");
    if (mode == "rollback-failed") {
      Require(result.error == Error::kRollbackFailed && b->installed && !a->installed,
              "rollback failure lost the still-installed member");
      Require(*trace == std::vector<std::string>({"A.install", "B.install", "C.install", "B.uninstall", "A.uninstall"}),
              "rollback failure did not continue undoing earlier members");
    } else if (mode == "mutation-then-error") {
      Require(b->installed && !a->installed, "mutation-before-error was not preserved as uncertain evidence");
    } else if (mode == "protection-failed") {
      Require(result.error == Error::kProtectionFailed, "protection failure lost its underlying result");
    } else if (mode == "suspension-unavailable") {
      Require(result.error == Error::kSuspensionUnavailable, "suspension failure lost its underlying result");
    } else if (mode == "unknown-error") {
      Require(result.error == static_cast<Error>(0x7f001), "future SPUD error was not retained exactly");
    } else if (mode == "install-throws") {
      Require(a->installed, "install exception was incorrectly presented as fully rolled back");
    } else if (mode == "detach-throws") {
      Require(a->installed && b->installed && c->installed && a->detaches == 1,
              "detach exception lost committed state");
    }
    RequireCached(set, result, trace);
    Require(owner.Run(true,
                      [&] {
                        ++guarded_calls;
                        return true;
                      })
                == InstallState::kIndeterminate,
            "unsafe owner retried initialization");
  }
  for (const auto& state : {a, b, c})
    Require(state->destroys == 0, "unsafe owner was destroyed instead of quarantined");
  {
    const auto calls        = historical_trace->size();
    const auto report_count = reports.size();
    const auto observed     = historical.CommitDetailed();
    Require(observed.state == historical_result.state && observed.error == historical_result.error
                && observed.Installed() && !observed.Unsafe(),
            "global unsafe state rewrote the historical detailed result");
    bool refused = false;
    try {
      (void)historical.Commit();
    } catch (const il2cpp::HookInstallError& error) {
      refused = true;
      Require(error.Result().Unsafe() && error.Result().error == Error::kOk,
              "historical bool guard invented a new SPUD error");
    }
    Require(refused, "a cached successful bool Commit bypassed the global unsafe latch");
    Require(historical_trace->size() == calls && historical_hook->installs == 1 && historical_hook->detaches == 1
                && historical_hook->uninstalls == 0 && reports.size() == report_count,
            "historical unsafe guard repeated native work or reports");
  }
  auto blocked = Make("Blocked", trace);
  {
    il2cpp::HookSet set("AfterUnsafe");
    set.Stage(FakeHook(blocked));
    const auto result = set.CommitDetailed();
    Require(result.Unsafe() && result.error == Error::kOk, "latched refusal invented a new SPUD error");
    Require(blocked->installs == 0 && blocked->queries == 0 && blocked->detaches == 0,
            "a new transaction touched native state after the unsafe latch");
    RequireCached(set, result, trace);
  }
  Require(blocked->destroys == 0 && il2cpp::HookStateIndeterminate(), "process unsafe latch or quarantine was reset");
  InstallGuard fresh_owner;
  for (auto* owner : {&installed_before, &skipped_before, &fresh_owner}) {
    const auto state = owner->Run(true, [&] {
      ++guarded_calls;
      return true;
    });
    Require(state == InstallState::kIndeterminate && MustAbortInstallation(state, InstallRequirement::kOptional)
                && MustAbortInstallation(state, InstallRequirement::kRequired),
            "actual process latch did not stop an existing or fresh installer owner");
    Require(!FrameDeliveryReady(state, true, true), "latched owner still claimed frame readiness");
  }
  Require(guarded_calls == 1, "latched process executed another installer attempt");
}
} // namespace fixture

// Link the production hook implementation with fixture diagnostics, without
// importing GameAssembly or the managed metadata implementation.
namespace il2cpp
{
void Report(std::string_view message)
{ fixture::reports.emplace_back(message); }
std::string Method::Name() const
{ throw std::logic_error("HookSet fixture unexpectedly requested managed metadata"); }
} // namespace il2cpp

int main(int argc, char** argv)
{
  try {
    if (argc == 1)
      fixture::SafeCases();
    else if (argc == 2)
      fixture::UnsafeCase(argv[1]);
    else
      throw std::runtime_error("pass zero arguments or one isolated unsafe mode");
    std::cout << "Hook installation qualification passed: " << (argc == 1 ? "safe cases" : argv[1]) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Hook installation qualification failed: " << error.what() << '\n';
    return 1;
  }
}
