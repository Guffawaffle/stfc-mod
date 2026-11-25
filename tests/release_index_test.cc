#include "release_index.h"

#include <cassert>

int main()
{
  using namespace mod_updates;
  constexpr auto index   = R"({"schema":1,"channels":{
    "release":"v1.1.9.5-guffa.10",
    "pre":"v1.1.9.5-guffa.8",
    "dev":"v1.1.9.5-guffa.7-dev.12"
  }})";
  const auto     release = IdentifyRelease("1.1.9.5-guffa.9 · Release");
  const auto     pre     = IdentifyRelease("1.1.9.5-guffa.8 · Pre");
  const auto     dev     = IdentifyRelease("1.1.9.5-guffa.7-dev.9 · DEV");
  assert(release && pre && dev);
  assert(CheckIndex(index, *release).tag == "v1.1.9.5-guffa.10");
  assert(CheckIndex(index, *pre).status == CheckStatus::Current);
  assert(CheckIndex(index, *dev).tag == "v1.1.9.5-guffa.7-dev.12");
  assert(IsIgnoredUpdate(CheckIndex(index, *dev), "v1.1.9.5-guffa.7-dev.12"));
  assert(!IsIgnoredUpdate(CheckIndex(index, *dev), "v1.1.9.5-guffa.7-dev.11"));
  assert(!IsIgnoredUpdate(CheckIndex(index, *release), "v1.1.9.5-guffa.7-dev.12"));
  assert(!IsIgnoredUpdate(CheckIndex(index, *pre), ""));
  assert(RestoreCheck("v1.1.9.5-guffa.7-dev.12", *dev).tag == "v1.1.9.5-guffa.7-dev.12");
  assert(RestoreCheck("v1.1.9.5-guffa.7-dev.9", *dev).status == CheckStatus::Current);
  assert(RestoreCheck("v1.1.9.5-guffa.7-dev.8", *dev).status == CheckStatus::Current);
  assert(RestoreCheck("", *dev).status == CheckStatus::Idle);
  assert(RestoreCheck("v1.1.9.5-guffa.10", *dev).status == CheckStatus::Idle);

  const auto newer = IdentifyRelease("1.1.10.0-guffa.1 · Release");
  assert(newer && CheckIndex(index, *newer).status == CheckStatus::Current);
  assert(RestoreCheck(CheckIndex(index, *newer).tag, *release).status == CheckStatus::Available);
  assert(!IdentifyRelease("1.1.9.5-guffa.9"));
  assert(!IdentifyRelease("1.1.9.5 · Pre b123.1"));
  assert(!IdentifyRelease("1.1.9.5 · DEV local"));
  assert(!IdentifyRelease("1.1.9.5-guffa.7-dev.9 · Pre"));

  for (const auto bad : {"v1.1.9.5-guffa.010", "v1.1.9.5-guffa.4294967296", "v1.1.9.5-guffa.0",
                         "v1.1.9.5-guffa.10/other", "v1.1.9.5-guffa.10-dev.1"})
    assert(!ParseReleaseTag(bad, Channel::Release));

  constexpr auto independent_index  = R"({"schema":1,"channels":{
    "release":"v2.2.0", "pre":"v2.2.0-rc.2", "dev":"v2.2.0-dev.10"
  }})";
  const auto     transition_release = IdentifyRelease("1.1.9.5-guffa.12 · Release");
  const auto     transition_pre     = IdentifyRelease("1.1.9.5-guffa.12 · Pre");
  const auto     transition_dev     = IdentifyRelease("1.1.9.5-guffa.12-dev.99 · DEV");
  assert(transition_release && transition_pre && transition_dev);
  for (const auto installed : {*transition_release, *transition_pre, *transition_dev})
    assert(CheckIndex(independent_index, installed).status == CheckStatus::Available);
  assert(RestoreCheck("v2.2.0-rc.2", *transition_pre).status == CheckStatus::Available);
  assert(IsIgnoredUpdate(CheckIndex(independent_index, *transition_pre), "v2.2.0-rc.2"));

  const auto independent_release = IdentifyRelease("2.2.0 · Release");
  const auto independent_pre     = IdentifyRelease("2.2.0-rc.2 · Pre");
  const auto independent_dev     = IdentifyRelease("2.2.0-dev.10 · DEV");
  assert(independent_release && independent_pre && independent_dev);
  for (const auto installed : {*independent_release, *independent_pre, *independent_dev}) {
    assert(CheckIndex(independent_index, installed).status == CheckStatus::Current);
    assert(CheckIndex(index, installed).status == CheckStatus::Current);
  }
  assert(RestoreCheck("v2.2.0-rc.10", *independent_pre).status == CheckStatus::Available);
  assert(RestoreCheck("v2.2.0-rc.1", *independent_pre).status == CheckStatus::Current);
  assert(RestoreCheck("v2.10.0", *independent_release).status == CheckStatus::Available);
  assert(RestoreCheck("v2.1.99", *independent_release).status == CheckStatus::Current);
  assert(RestoreCheck("v2.2.1-dev.1", *independent_dev).status == CheckStatus::Available);
  assert(*ParseReleaseTag("v2.2.0-dev.99", Channel::Dev) < *ParseReleaseTag("v2.2.0-rc.1", Channel::Pre));
  assert(*ParseReleaseTag("v2.2.0-rc.99", Channel::Pre) < *ParseReleaseTag("v2.2.0", Channel::Release));
  assert(*ParseReleaseTag("v2.2.0", Channel::Release) < *ParseReleaseTag("v2.2.1-dev.1", Channel::Dev));
  assert(*ParseReleaseTag("v1.99.99.99-guffa.4294967295", Channel::Release) < independent_release->version);
  assert(*ParseReleaseTag("v1.1.9.5-guffa.10-dev.1", Channel::Dev)
         > *ParseReleaseTag("v1.1.9.5-guffa.9-dev.99", Channel::Dev));
  assert(ParseReleaseTag("v2.4294967295.0", Channel::Release));
  assert(ParseReleaseTag("v2.2.0-rc.4294967295", Channel::Pre));

  for (const auto bad : {"2.2.0 · Pre", "2.2.0-rc.2 · Release", "2.2.0-dev.1 · Pre", "2.2.0-rc.1 · DEV",
                         "2.2.0-ci.45005222362.1 · CI", "2.2.0 · CI b45005222362.1", "2.2.0 · DEV local"})
    assert(!IdentifyRelease(bad));
  for (const auto channel : {Channel::Release, Channel::Pre, Channel::Dev})
    for (const auto bad :
         {"v1.1.9", "v2.2.0.0", "v2.2.0-beta.1", "v2.2.0-ci.45005222362.1", "2.2.0", "V2.2.0", "v02.2.0", "v2.02.0",
          "v2.2.00", "v2.2.4294967296", "v2.2.0+build.1", "v2.2.0/other", "v2.2.0\n"})
      assert(!ParseReleaseTag(bad, channel));
  for (const auto bad : {"v2.2.0-rc.0", "v2.2.0-rc.01", "v2.2.0-rc.4294967296", "v2.2.0-rc.1.1", "v2.2.0-rc.1/other",
                         "v2.2.0-dev.1", "v2.2.0"})
    assert(!ParseReleaseTag(bad, Channel::Pre));
  for (const auto bad : {"v2.2.0-dev.0", "v2.2.0-dev.01", "v2.2.0-dev.4294967296", "v2.2.0-dev.1.1",
                         "v2.2.0-dev.1/other", "v2.2.0-rc.1", "v2.2.0"})
    assert(!ParseReleaseTag(bad, Channel::Dev));
  assert(!ParseReleaseTag("v2.2.0-rc.1", Channel::Release));
  assert(!ParseReleaseTag("v2.2.0-dev.1", Channel::Release));
  assert(CheckIndex(R"({"schema":1,"channels":{"pre":"v2.2.0"}})", *transition_pre).status == CheckStatus::Failed);
  assert(RestoreCheck("v2.2.0", *transition_pre).status == CheckStatus::Idle);
  assert(CheckIndex(R"({"schema":1,"channels":{"release":"v1.1.9.5-guffa.10"}})", *pre).status == CheckStatus::Failed);
  assert(CheckIndex(R"({"schema":2,"channels":{}})", *release).status == CheckStatus::Failed);
  assert(CheckIndex("not JSON", *dev).status == CheckStatus::Failed);
  assert(CheckIndex(std::string(max_index_bytes + 1, ' '), *dev).status == CheckStatus::Failed);
}
