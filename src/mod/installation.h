#pragma once

// Process-lifetime installer ownership. This is capability readiness, not a claim that every optional sub-hook
// exists. HookSet owns transaction results; this guard prevents repeating a feature's initialization side effects.

#include <utility>

// Only the process safety observation is needed here; the native binding/transaction headers do not belong in
// this capability-owner model. The definition remains in the single IL2CPP library owner.
namespace il2cpp
{
bool HookStateIndeterminate() noexcept;
}

enum class InstallState { kNotAttempted, kInstalling, kSkipped, kInstalled, kUnavailable, kIndeterminate };
enum class InstallRequirement { kOptional, kRequired };

constexpr const char* InstallStateName(InstallState state)
{
  switch (state) {
    case InstallState::kNotAttempted:
      return "not-attempted";
    case InstallState::kInstalling:
      return "installing";
    case InstallState::kSkipped:
      return "skipped";
    case InstallState::kInstalled:
      return "installed";
    case InstallState::kUnavailable:
      return "unavailable";
    case InstallState::kIndeterminate:
      return "indeterminate";
  }
  return "unknown";
}

constexpr bool MustAbortInstallation(InstallState state, InstallRequirement requirement)
{
  // A required phase may never continue into login in an incomplete/reentrant/skipped state. An uncertain native
  // patch is unsafe even when the capability requesting it was optional.
  return state == InstallState::kIndeterminate
         || (requirement == InstallRequirement::kRequired && state != InstallState::kInstalled);
}

constexpr bool FrameDeliveryReady(InstallState feature, bool registered, bool anchor_ready)
{ return feature == InstallState::kInstalled && registered && anchor_ready; }

constexpr bool FrameDispatchReady(InstallState host, bool anchor_ready)
{ return host == InstallState::kInstalled && anchor_ready; }

class InstallGuard
{
public:
  InstallState State() const noexcept
  { return state_; }

  // Game/bootstrap thread only. A reentrant request observes Installing; a later request observes the terminal
  // result. Neither repeats the attempt. A thrown initializer remains unavailable, or indeterminate if HookSet
  // latched unsafe native state, before the exception is passed to the caller's reporting boundary.
  template <typename Attempt> InstallState Run(bool enabled, Attempt&& attempt)
  {
    if (il2cpp::HookStateIndeterminate())
      return state_ = InstallState::kIndeterminate;
    if (state_ != InstallState::kNotAttempted)
      return state_;
    if (!enabled)
      return state_ = InstallState::kSkipped;
    state_ = InstallState::kInstalling;
    try {
      const bool installed = std::forward<Attempt>(attempt)();
      state_               = il2cpp::HookStateIndeterminate() ? InstallState::kIndeterminate
                             : installed                      ? InstallState::kInstalled
                                                              : InstallState::kUnavailable;
    } catch (...) {
      state_ = il2cpp::HookStateIndeterminate() ? InstallState::kIndeterminate : InstallState::kUnavailable;
      throw;
    }
    return state_;
  }

private:
  InstallState state_ = InstallState::kNotAttempted;
};
