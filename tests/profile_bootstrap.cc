// Pure process admission policy: no catalog, token, game, filesystem or native hook access.
#include "profile_bootstrap.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using profile_bootstrap::Phase;
using profile_bootstrap::ProfileBootstrap;
constexpr std::string_view kId = "0123456789abcdef0123456789abcdef";

void Require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

template <typename Exception, typename Action> void Throws(Action&& action, const char* message)
{
  bool thrown = false;
  try {
    std::forward<Action>(action)();
  } catch (const Exception&) {
    thrown = true;
  }
  Require(thrown, message);
}

auto Parse(std::initializer_list<std::string_view> arguments)
{ return profile_bootstrap::ParseProfileArguments({arguments.begin(), arguments.size()}); }

void Arguments()
{
  Require(!Parse({}) && !Parse({"prime.exe", "-debug", "-ccm", "captain.toml"}),
          "ordinary and filename-selected launches do not become named profiles");
  Require(Parse({"prime.exe", "-stfc-profile", kId}) == kId, "one exact selector resolves its owned ID");
  Require(Parse({"-ccm", "separate.toml", "-stfc-profile", kId, "-trace"}) == kId,
          "filename selection is independent of a named request");
  Require(!Parse({"-ccm", kId}), "a profile-looking filename does not request identity");
  Require(!Parse({"-STFC-PROFILE", kId}), "the selector uses its explicit case-sensitive spelling");
  Require(profile_bootstrap::IsProfileId(std::string(32, '0')), "zero hex digits are valid format, not existence");

  Throws<std::invalid_argument>([] { Parse({"-stfc-profile"}); }, "a bare selector must fail");
  Throws<std::invalid_argument>([] { Parse({"-stfc-profile", ""}); }, "an empty selector value must fail");
  Throws<std::invalid_argument>([] { Parse({"-stfc-profile", kId, "-stfc-profile", kId}); },
                                "duplicate identical selectors must fail");
  Throws<std::invalid_argument>(
      [] { Parse({"-stfc-profile", kId, "-stfc-profile", "fedcba9876543210fedcba9876543210"}); },
      "duplicate different selectors must fail");
  Throws<std::invalid_argument>([] { Parse({"-stfc-profile", kId, "-stfc-profile"}); },
                                "a duplicate bare selector must fail");
  Throws<std::invalid_argument>([] { Parse({"-stfc-profile", "-debug"}); }, "another option is not a profile ID");
  for (const std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{31}, std::size_t{33}, std::size_t{64}}) {
    const std::string value(length, 'a');
    Throws<std::invalid_argument>([&] { Parse({"-stfc-profile", value}); }, "wrong ID length was accepted");
  }
  for (const char invalid : {'A', 'F', 'g', 'G', '-', '/', '\\', '.', ' ', '\0', static_cast<char>(0x80)}) {
    for (std::size_t position = 0; position < kId.size(); ++position) {
      std::string value(kId);
      value[position] = invalid;
      Throws<std::invalid_argument>([&] { Parse({"-stfc-profile", value}); }, "a non-lowercase-hex digit was accepted");
    }
  }
  std::string                           source(kId);
  const std::array<std::string_view, 2> borrowed{"-stfc-profile", source};
  const auto                            owned = profile_bootstrap::ParseProfileArguments(borrowed);
  source.assign(32, 'f');
  Require(owned == kId, "the resolved ID borrows platform argument storage");
}

void ExecutableAdmission()
{
  for (const auto value : {L"prime.exe", L"Prime.EXE", L"PRIME.EXE", L"C:\\Games\\Prime.Exe", L"/game/prime.exe"})
    Require(profile_bootstrap::IsPrimeExecutable(value), "an exact case-insensitive game basename was rejected");
  for (const auto value : {L"", L"prime", L"primefoo.exe", L"prime.exe.bak", L"rundll32.exe",
                           L"C:\\Windows\\System32\\rundll32.exe", L"C:\\prime.exe\\", L" prime.exe", L"prime.exe "})
    Require(!profile_bootstrap::IsPrimeExecutable(value), "a non-game process was admitted");
  Require(!profile_bootstrap::IsPrimeExecutable(std::wstring_view(L"prime.exe\0other", 15)),
          "an embedded zero suffix was accepted");
}

void NamedLifecycle()
{
  ProfileBootstrap         bootstrap;
  std::vector<std::string> trace;
  bootstrap.Initialize(true, [&] {
    trace.emplace_back("installation lease");
    trace.emplace_back("named process admission");
  });
  Require(bootstrap.Requested() && !bootstrap.Ready() && bootstrap.State() == Phase::kInitialized,
          "named initialization published readiness early");
  bootstrap.Prepare([&] {
    trace.emplace_back("session lease");
    trace.emplace_back("preference preparation");
  });
  trace.emplace_back("file and config");
  trace.emplace_back("original runtime");
  Require(bootstrap.RuntimeInitialized(true), "successful runtime was rejected");
  Require(!bootstrap.Ready(), "runtime success preceded required hook installation");
  bootstrap.Install([&] {
    trace.emplace_back("required preference and browser hooks");
    trace.emplace_back("session ready");
  });
  Require(bootstrap.Ready() && !bootstrap.Failed(), "named completion did not become ready");
  trace.emplace_back("optional features");
  const std::vector<std::string> expected{"installation lease",
                                          "named process admission",
                                          "session lease",
                                          "preference preparation",
                                          "file and config",
                                          "original runtime",
                                          "required preference and browser hooks",
                                          "session ready",
                                          "optional features"};
  Require(trace == expected, "named bootstrap changed phase ordering");
  bootstrap.Initialize(true, [] { throw std::runtime_error("initialize repeated"); });
  bootstrap.Prepare([] { throw std::runtime_error("prepare repeated"); });
  Require(bootstrap.RuntimeInitialized(true), "repeat successful observation was rejected");
  bootstrap.Install([] { throw std::runtime_error("install repeated"); });
  Require(bootstrap.Ready() && trace == expected, "completed repeats changed the process owner");
}

void OrdinaryLifecycle()
{
  ProfileBootstrap bootstrap;
  std::uint32_t    leases = 0;
  bootstrap.Initialize(false, [&] { ++leases; });
  bootstrap.Prepare([] { throw std::runtime_error("ordinary launch opened a profile store"); });
  Require(bootstrap.RuntimeInitialized(true), "ordinary runtime success was rejected");
  bootstrap.Install([] { throw std::runtime_error("ordinary launch installed named hooks"); });
  bootstrap.Initialize(false, [&] { ++leases; });
  Require(bootstrap.Ready() && !bootstrap.Requested() && leases == 1,
          "ordinary launch omitted or repeated installation admission");
}

void IdentityCannotChange()
{
  for (const bool initially_requested : {false, true}) {
    ProfileBootstrap bootstrap;
    bootstrap.Initialize(initially_requested, [] {});
    std::uint32_t attempts = 0;
    Throws<std::logic_error>([&] { bootstrap.Initialize(!initially_requested, [&] { ++attempts; }); },
                             "a process changed its requested identity mode");
    Require(bootstrap.Failed() && bootstrap.Requested() == initially_requested && attempts == 0,
            "a changed identity was attempted or not latched failed");
  }
}

void RequiredFailures()
{
  {
    ProfileBootstrap bootstrap;
    Throws<int>([&] { bootstrap.Initialize(false, [] { throw 7; }); }, "an unknown exception did not propagate");
    Require(bootstrap.Failed(), "an unknown initializer failure did not seal the owner");
  }
  // Every initialize failure is required, including an ordinary installation lease.
  for (const bool requested : {false, true}) {
    ProfileBootstrap bootstrap;
    Throws<std::runtime_error>(
        [&] { bootstrap.Initialize(requested, [] { throw std::runtime_error("installation refusal"); }); },
        "installation failure did not propagate");
    Require(bootstrap.Failed() && !bootstrap.Ready(), "installation failure did not seal the owner");
    Throws<std::logic_error>([&] { bootstrap.Initialize(requested, [] {}); }, "failed initialization was retried");
  }
  for (const std::uint32_t failure : {1u, 2u}) {
    ProfileBootstrap bootstrap;
    std::uint32_t    entered   = 0;
    bool             continued = false;
    Throws<std::runtime_error>(
        [&] {
          bootstrap.Initialize(true, [] {});
          bootstrap.Prepare([&] {
            ++entered;
            if (failure == 1)
              throw std::runtime_error("preparation refusal");
          });
          Require(bootstrap.RuntimeInitialized(true), "runtime success refused");
          bootstrap.Install([&] {
            ++entered;
            throw std::runtime_error("hook refusal");
          });
          continued = true;
        },
        "required named failure did not propagate");
    Require(bootstrap.Failed() && !bootstrap.Ready() && !continued && entered == failure,
            "required failure allowed a later phase");
    Throws<std::logic_error>([&] { bootstrap.Install([&] { ++entered; }); }, "failed hooks were retried");
    Require(entered == failure, "a failed owner ran another action");
  }
  for (const bool requested : {false, true}) {
    ProfileBootstrap bootstrap;
    bootstrap.Initialize(requested, [] {});
    bootstrap.Prepare([] {});
    if (requested)
      Throws<std::runtime_error>([&] { bootstrap.RuntimeInitialized(false); }, "named runtime failure continued");
    else
      Require(!bootstrap.RuntimeInitialized(false), "ordinary runtime failure allowed feature initialization");
    Require(bootstrap.Failed() && !bootstrap.Ready(), "runtime failure did not seal the owner");
    Throws<std::logic_error>([&] { bootstrap.Install([] {}); }, "hooks ran after runtime failure");
  }
}

void OutOfOrder()
{
  {
    ProfileBootstrap bootstrap;
    Throws<std::logic_error>([&] { bootstrap.Prepare([] {}); }, "preparation ran before installation admission");
    Require(bootstrap.Failed(), "out-of-order preparation was not terminal");
  }
  {
    ProfileBootstrap bootstrap;
    Throws<std::logic_error>([&] { bootstrap.RuntimeInitialized(true); }, "runtime was accepted before admission");
    Require(bootstrap.Failed(), "out-of-order runtime was not terminal");
  }
  {
    ProfileBootstrap bootstrap;
    bootstrap.Initialize(true, [] {});
    Throws<std::logic_error>([&] { bootstrap.RuntimeInitialized(true); }, "runtime skipped preparation");
    Require(bootstrap.Failed(), "skipped preparation was not terminal");
  }
  {
    ProfileBootstrap bootstrap;
    bootstrap.Initialize(true, [] {});
    bootstrap.Prepare([] {});
    Throws<std::logic_error>([&] { bootstrap.Install([] {}); }, "hooks ran before runtime success");
    Require(bootstrap.Failed(), "early hook installation was not terminal");
  }
}

void Reentrant()
{
  for (const std::uint32_t phase : {0u, 1u, 2u}) {
    ProfileBootstrap bootstrap;
    const auto       reenter = [&] {
      // Even if the action catches its rejected reentry, the outer action must
      // not publish success over the latched failure.
      Throws<std::logic_error>([&] { bootstrap.Initialize(true, [] {}); }, "reentrant initialization was accepted");
    };
    Throws<std::logic_error>(
        [&] {
          bootstrap.Initialize(true, [&] {
            if (phase == 0)
              reenter();
          });
          bootstrap.Prepare([&] {
            if (phase == 1)
              reenter();
          });
          Require(bootstrap.RuntimeInitialized(true), "runtime success refused");
          bootstrap.Install(reenter);
        },
        "a phase resurrected the owner after caught reentry");
    Require(bootstrap.Failed() && !bootstrap.Ready(), "reentrant phase did not remain failed");
  }
}
} // namespace

int main()
{
  try {
    Arguments();
    ExecutableAdmission();
    NamedLifecycle();
    OrdinaryLifecycle();
    IdentityCannotChange();
    RequiredFailures();
    OutOfOrder();
    Reentrant();
    std::cout << "Profile bootstrap policy passed: owned arguments, exact entry, immutable identity, ordered "
                 "ordinary/named phases and terminal failures\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
