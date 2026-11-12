#include "build_identity.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace
{
void Require(bool result, const char* message)
{
  if (!result) {
    std::fprintf(stderr, "Build identity fixture failed: %s\n", message);
    std::exit(1);
  }
}
} // namespace

int main()
{
  using namespace build_identity;
  constexpr std::string_view base   = "1.1.9.5";
  constexpr std::string_view source = "0123456789abcdef0123456789abcdef01234567";
  constexpr std::array       valid{"1.1.9.5-guffa.6 · Release", "1.1.9.5-guffa.6 · Pre",
                                   "1.1.9.5 · Pre b137.2",      "1.1.9.5 · DEV b137.2",
                                   "1.1.9.5 · DEV local",       "1.1.9.5-guffa.9223372036854775807 · Pre"};
  for (const auto* value : valid)
    Require(ValidDisplayVersion(value, base), "declared Release/Pre/DEV labels are accepted");
  constexpr std::array invalid{"",
                               "1.1.9.0-guffa.6 · Release",
                               "1.1.9.50-guffa.6 · Release",
                               "1.1.9.5 · Release",
                               "1.1.9.5 · Pre local",
                               "1.1.9.5-guffa.0 · Release",
                               "1.1.9.5-guffa.06 · Release",
                               "1.1.9.5-guffa.6 · DEV",
                               "1.1.9.5-guffa.6 · Release\n",
                               "1.1.9.5-guffa.6 · Release ",
                               "1.1.9.5-guffa.6 · release",
                               "1.1.9.5 · DEV b0.1",
                               "1.1.9.5 · DEV b1.0",
                               "1.1.9.5 · DEV b01.1",
                               "1.1.9.5 · DEV b1.1.1",
                               "1.1.9.5 · Pre b-1.1",
                               "1.1.9.5 · DEV b9223372036854775808.1",
                               "1.1.9.5-guffa.6 · <b>Release</b>",
                               "<color=red>1.1.9.5 · DEV local</color>",
                               "1.1.9.5 · DEV local\r\nspoof",
                               "1.1.9.5 · DEV local|Release"};
  for (const auto* value : invalid)
    Require(!ValidDisplayVersion(value, base), "wrong base, markup, controls and invalid numeric/channel grammar fail");
  Require(!ValidDisplayVersion("1.1.9.5 · DEV local", "1.1.9"), "a malformed compiled base cannot qualify metadata");
  Require(!ValidDisplayVersion(std::string(161, '1'), base), "oversized labels are refused");
  Require(!ValidDisplayVersion(std::string("1.1.9.5 · DEV local\0suffix", 28), base), "embedded NUL is not accepted");

  Require(SourceMatches(source, "01234567"), "full launcher source matches compiled short hash");
  Require(SourceMatches(source, "0123456789ABCDEF0123456789ABCDEF01234567"), "full hashes compare without hex case");
  Require(!SourceMatches(source, "fedcba98"), "wrong loaded source refuses a stale launcher label");
  Require(!SourceMatches(source, ""), "unknown compiled source does not authorize an external label");
  Require(!SourceMatches("01234567", "01234567"), "an abbreviated handoff source is insufficient");
  Require(!SourceMatches("0123456789abcdef0123456789abcdef0123456z", "01234567"),
          "the entire handoff source must be valid hex");
  Require(!SourceMatches(source, "0123456z"), "invalid compiled hashes cannot qualify a handoff");
  Require(HandoffVersion(base, valid[0], source, "01234567") == valid[0], "matching Mac handoff preserves release");
  Require(HandoffVersion(base, valid[2], source, "01234567") == valid[2],
          "matching unnamed Pre handoff preserves CI label");
  Require(HandoffVersion(base, valid[0], source, "fedcba98") == base,
          "wrong-source release handoff leaves its channel unknown");
  Require(HandoffVersion(base, {}, {}, {}) == "1.1.9.5 · DEV local", "missing metadata produces an honest local label");
  Require(HandoffVersion(base, {}, {}, "01234567") == base, "a known CI hash without a launcher has unknown channel");
  Require(HandoffVersion(base, valid[0], {}, {}) == base, "a supplied unbound label cannot claim a DEV channel");
  Require(HandoffVersion(base, {}, {}, {}, {}, true) == base,
          "empty or malformed supplied metadata is distinct from absence");

  Require(DescriptionVersion(base, "STFC Community Mod | 1.1.9.5-guffa.6 · Release", SignatureState::Present)
              == valid[0],
          "strict Windows signer-description prefix and release label are retained");
  Require(DescriptionVersion(base, "STFC Community Mod | 1.1.9.5 · Pre b137.2", SignatureState::Present) == valid[2],
          "signed unnamed Pre displays its CI build identity");
  Require(DescriptionVersion(base, "Other App | 1.1.9.5-guffa.6 · Release", SignatureState::Present) == base,
          "unrelated signer descriptions do not become product metadata");
  Require(DescriptionVersion(base, "STFC Community Mod | 1.1.9.0-guffa.6 · Release", SignatureState::Present) == base,
          "a legacy or mismatched description cannot replace the compiled core version");
  Require(DescriptionVersion(base, {}, SignatureState::Absent, "github:42576047391:2:windows")
              == "1.1.9.5 · DEV b42576047391.2",
          "unsigned CI payloads retain a useful development build fallback");
  Require(DescriptionVersion(base, {}, SignatureState::Present, "github:42576047391:2:windows") == base,
          "a present signature without a known label is not misclassified as DEV");
  Require(DescriptionVersion(base, {}, SignatureState::Unknown, "github:42576047391:2:windows") == base,
          "unobservable signature metadata is not confused with absent metadata");
  for (const auto invocation : {"github:windows", "github::windows", "github:0:1:windows", "github:1:0:windows",
                                "github:1:1:windows\n", "ax-zig-20261111-local"})
    Require(FallbackVersion(base, invocation) == "1.1.9.5 · DEV local", "unknown/malformed invocations stay local");

  std::puts("Build display identity: strict label grammar, source binding, signer description and fallbacks passed");
}
