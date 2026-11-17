#include "build_identity.h"
#include "version.h"

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
  for (const auto* value :
       {"1.1.9.5-guffa.6 · DEV local", "1.1.9.5-guffa.6 · DEV b137.2", "1.1.9.5-guffa.6 · Pre b137.2",
        "1.1.9.5-guffa.6 · CI local", "1.1.9.5-guffa.6 · CI b137.2"})
    Require(ValidDisplayVersion(value, base), "fork lineage is retained independently from the build channel");
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
                               "1.1.9.5-guffa.6 · CI",
                               "1.1.9.5-guffa.6 · CI b0.1",
                               "1.1.9.5-guffa.6 · CI b1.0",
                               "1.1.9.5-guffa.6 · CI local\n",
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
  Require(HandoffVersion(base, {}, {}, {}) == "1.1.9.5 · CI local", "an unnamed core build has a CI local label");
  Require(HandoffVersion(base, {}, {}, "01234567") == base, "a known CI hash without a launcher has unknown channel");
  Require(HandoffVersion(base, valid[0], {}, {}) == base, "a supplied unbound label cannot claim a DEV channel");
  Require(HandoffVersion(base, {}, {}, {}, {}, true) == base,
          "empty or malformed supplied metadata is distinct from absence");
  Require(HandoffVersion(base, {}, {}, {}, {}, false, VER_FORK_VERSION_STR)
              == std::string(VER_FORK_VERSION_STR) + " · CI local",
          "the compiled fork version remains visible in a local Mac core build");
  Require(HandoffVersion(base, {}, {}, {}, {}, false, VER_FORK_VERSION_STR, true)
              == std::string(VER_FORK_VERSION_STR) + " · DEV local",
          "only an explicitly selected development payload receives the DEV fallback");
  Require(HandoffVersion(base, "1.1.9.5-guffa.6 · DEV b137.2", source, "01234567", {}, true, VER_FORK_VERSION_STR, true)
              == "1.1.9.5-guffa.6 · DEV b137.2",
          "a source-matched Mac handoff retains both the fork revision and development build");
  Require(HandoffVersion(base, "1.1.9.5-guffa.6 · CI b137.2", source, "01234567", {}, true, VER_FORK_VERSION_STR)
              == "1.1.9.5-guffa.6 · CI b137.2",
          "a source-matched raw PR payload retains CI rather than claiming the development package");
  Require(HandoffVersion(base, "1.1.9.5-guffa.6 · DEV b137.2", source, "01234567", {}, true, VER_FORK_VERSION_STR)
              == VER_FORK_VERSION_STR,
          "a matching source commit cannot turn a core payload into a development package");
  Require(HandoffVersion(base, "1.1.9.5-guffa.6 · CI b137.2", source, "01234567", {}, true, VER_FORK_VERSION_STR, true)
              == VER_FORK_VERSION_STR,
          "a development package refuses stale CI metadata from the same source");

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
              == "1.1.9.5 · CI b42576047391.2",
          "unsigned CI payloads retain a useful CI build fallback");
  Require(DescriptionVersion(base, {}, SignatureState::Present, "github:42576047391:2:windows") == base,
          "a present signature without a known label is not misclassified as DEV");
  Require(DescriptionVersion(base, {}, SignatureState::Unknown, "github:42576047391:2:windows") == base,
          "unobservable signature metadata is not confused with absent metadata");
  Require(DescriptionVersion(base, {}, SignatureState::Absent, "github:42576047391:2:windows", VER_FORK_VERSION_STR)
              == std::string(VER_FORK_VERSION_STR) + " · CI b42576047391.2",
          "raw Windows CI retains the compiled fork revision alongside its unique CI build");
  Require(DescriptionVersion(base, {}, SignatureState::Absent, {}, VER_FORK_VERSION_STR)
              == std::string(VER_FORK_VERSION_STR) + " · CI local",
          "the in-game unsigned local fallback uses the compiled fork version");
  Require(
      DescriptionVersion(base, {}, SignatureState::Absent, "github:42576047391:2:windows", VER_FORK_VERSION_STR, true)
          == std::string(VER_FORK_VERSION_STR) + " · DEV b42576047391.2",
      "the selected development package retains its distinct unsigned DEV identity");
  Require(DescriptionVersion(base, "STFC Community Mod | 1.1.9.5-guffa.6 · DEV local", SignatureState::Present, {},
                             VER_FORK_VERSION_STR)
              == VER_FORK_VERSION_STR,
          "a core module cannot gain a DEV identity from mismatched descriptive metadata");
  Require(DescriptionVersion(base, {}, SignatureState::Unknown, {}, VER_FORK_VERSION_STR) == VER_FORK_VERSION_STR,
          "unknown signing metadata retains known fork lineage without claiming a channel");
  const std::string dev_tag   = "v" + std::string(VER_FORK_VERSION_STR) + "-dev.1";
  const std::string dev_label = std::string(VER_FORK_VERSION_STR) + "-dev.1 · DEV";
  Require(ValidDisplayVersion(dev_label, base), "a named DEV label retains its package sequence");
  Require(HandoffVersion(base, dev_label, source, "01234567", {}, true, VER_FORK_VERSION_STR, true) == dev_label,
          "the exact source-bound named DEV label reaches the selected Mac development payload");
  Require(HandoffVersion(base, dev_label, source, "01234567", {}, true, VER_FORK_VERSION_STR) == VER_FORK_VERSION_STR,
          "a named DEV label cannot relabel a core payload");
  Require(DescriptionVersion(base, {}, SignatureState::Absent, {}, VER_FORK_VERSION_STR, true, dev_tag) == dev_label,
          "the Windows compiled DEV tag supplies its complete unsigned display version");
  for (const auto* invalid_dev : {"1.1.9.5-guffa.6-dev.01 · DEV", "1.1.9.5-guffa.6-dev.0 · DEV",
                                  "1.1.9.5-guffa.6-dev.1 · Release", "1.1.9.5-guffa.6-dev.1 · DEV b1.1"})
    Require(!ValidDisplayVersion(invalid_dev, base), "named DEV tags reject misleading or malformed labels");
  const std::string mismatched_dev_tag =
      "v" + std::string(VER_FILE_VERSION_STR) + "-guffa." + std::to_string(1ULL + VERSION_FORK_REVISION) + "-dev.1";
  Require(FallbackVersion(VER_FORK_VERSION_STR, {}, true, mismatched_dev_tag)
              == std::string(VER_FORK_VERSION_STR) + " · DEV local",
          "a mismatched DEV tag cannot replace the compiled lineage");
  for (const auto invocation : {"github:windows", "github::windows", "github:0:1:windows", "github:1:0:windows",
                                "github:1:1:windows\n", "ax-zig-20261111-local"})
    Require(FallbackVersion(base, invocation) == "1.1.9.5 · CI local", "unknown/malformed invocations stay local");

  std::puts("Build display identity: strict label grammar, source binding, signer description and fallbacks passed");
}
