#pragma once

// Process-lifetime profile admission policy. Platform argument acquisition, leases,
// native hooks and fatal reporting belong to the host, not this pure sequencing model.

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace profile_bootstrap
{
inline bool IsProfileId(std::string_view value) noexcept
{
  if (value.size() != 32)
    return false;
  for (const char ch : value)
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
      return false;
  return true;
}

// Unknown arguments, including -ccm and its filename, are not profile identity.
// The returned ID is owned; no platform argv storage survives this call.
inline std::optional<std::string> ParseProfileArguments(std::span<const std::string_view> arguments)
{
  std::optional<std::string> selected;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    if (arguments[i] != "-stfc-profile")
      continue;
    if (selected)
      throw std::invalid_argument("Specify -stfc-profile exactly once.");
    if (++i == arguments.size() || !IsProfileId(arguments[i]))
      throw std::invalid_argument("-stfc-profile requires an immutable 32-character lowercase hexadecimal profile ID.");
    selected = arguments[i];
  }
  return selected;
}

// The process may also be a non-game loader, such as the isolated browser guardian.
// Compare the complete ASCII basename without allocating or accepting a prefix.
inline bool IsPrimeExecutable(std::wstring_view path) noexcept
{
  const auto                  slash    = path.find_last_of(L"\\/");
  const auto                  name     = slash == std::wstring_view::npos ? path : path.substr(slash + 1);
  constexpr std::wstring_view expected = L"prime.exe";
  if (name.size() != expected.size())
    return false;
  for (std::size_t i = 0; i < name.size(); ++i) {
    const auto ch = name[i] >= L'A' && name[i] <= L'Z' ? name[i] + (L'a' - L'A') : name[i];
    if (ch != expected[i])
      return false;
  }
  return true;
}

enum class Phase {
  kNotStarted,
  kInitializing,
  kInitialized,
  kPreparing,
  kPrepared,
  kRuntimeReady,
  kInstalling,
  kReady,
  kFailed
};

class ProfileBootstrap
{
public:
  ProfileBootstrap()                                   = default;
  ProfileBootstrap(const ProfileBootstrap&)            = delete;
  ProfileBootstrap& operator=(const ProfileBootstrap&) = delete;
  ProfileBootstrap(ProfileBootstrap&&)                 = delete;
  ProfileBootstrap& operator=(ProfileBootstrap&&)      = delete;

  bool Requested() const noexcept
  { return requested_; }
  bool Ready() const noexcept
  { return phase_ == Phase::kReady; }
  bool Failed() const noexcept
  { return phase_ == Phase::kFailed; }
  Phase State() const noexcept
  { return phase_; }

  // Required for ordinary launches too: the host acquires the installation lease
  // and checks readiness here, before loading GameAssembly. Exceptions are fatal
  // at the host boundary even when no named profile was requested.
  template <typename Action> void Initialize(bool requested, Action&& action)
  {
    CheckCallable();
    if (phase_ != Phase::kNotStarted) {
      if (requested != requested_)
        Fail("The process profile identity cannot change after initialization.");
      return;
    }
    requested_ = requested;
    Run(Phase::kInitializing, Phase::kInitialized, std::forward<Action>(action));
  }

  // Named preparation precedes file/configuration access and original runtime
  // initialization. Ordinary launches advance without opening a profile store.
  template <typename Action> void Prepare(Action&& action)
  {
    CheckCallable();
    if (phase_ == Phase::kPrepared || phase_ == Phase::kRuntimeReady || phase_ == Phase::kReady)
      return;
    if (phase_ != Phase::kInitialized)
      Fail("Profile preparation requires completed initialization.");
    if (requested_)
      Run(Phase::kPreparing, Phase::kPrepared, std::forward<Action>(action));
    else
      phase_ = Phase::kPrepared;
  }

  // The host calls original first. A named failure must terminate before login;
  // an ordinary failure returns false so the host returns without any features.
  bool RuntimeInitialized(bool success)
  {
    CheckCallable();
    if (phase_ != Phase::kPrepared && phase_ != Phase::kRuntimeReady && phase_ != Phase::kReady)
      Fail("Runtime initialization requires completed profile preparation.");
    if (!success) {
      phase_ = Phase::kFailed;
      if (requested_)
        throw std::runtime_error("The game runtime did not initialize.");
      return false;
    }
    if (phase_ == Phase::kPrepared)
      phase_ = Phase::kRuntimeReady;
    return true;
  }

  // Required native preference/browser hooks and the session-ready publication
  // complete before optional feature initialization. Ordinary launches skip them.
  template <typename Action> void Install(Action&& action)
  {
    CheckCallable();
    if (phase_ == Phase::kReady)
      return;
    if (phase_ != Phase::kRuntimeReady)
      Fail("Profile hooks require successful runtime initialization.");
    if (requested_)
      Run(Phase::kInstalling, Phase::kReady, std::forward<Action>(action));
    else
      phase_ = Phase::kReady;
  }

private:
  [[noreturn]] void Fail(const char* message)
  {
    phase_ = Phase::kFailed;
    throw std::logic_error(message);
  }

  void CheckCallable()
  {
    if (phase_ == Phase::kFailed)
      throw std::logic_error("Profile bootstrap has already failed.");
    if (phase_ == Phase::kInitializing || phase_ == Phase::kPreparing || phase_ == Phase::kInstalling)
      Fail("Profile bootstrap cannot be reentered while a phase is running.");
  }

  template <typename Action> void Run(Phase running, Phase complete, Action&& action)
  {
    static_assert(std::is_void_v<std::invoke_result_t<Action>>, "A bootstrap action must complete or throw.");
    phase_ = running;
    try {
      std::forward<Action>(action)();
    } catch (...) {
      phase_ = Phase::kFailed;
      throw;
    }
    // A callback may catch a rejected reentrant request. It must not resurrect
    // the failed process owner by returning normally afterward.
    if (phase_ != running)
      Fail("Profile bootstrap failed during a phase.");
    phase_ = complete;
  }

  Phase phase_     = Phase::kNotStarted;
  bool  requested_ = false;
};
} // namespace profile_bootstrap
