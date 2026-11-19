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
  assert(IsNewerReleaseTag("v1.1.9.5-guffa.7-dev.12", "", Channel::Dev));
  assert(IsNewerReleaseTag("v1.1.9.5-guffa.7-dev.12", "v1.1.9.5-guffa.7-dev.9", Channel::Dev));
  assert(!IsNewerReleaseTag("v1.1.9.5-guffa.7-dev.12", "v1.1.9.5-guffa.7-dev.12", Channel::Dev));
  assert(!IsNewerReleaseTag("v1.1.9.5-guffa.7-dev.9", "v1.1.9.5-guffa.7-dev.12", Channel::Dev));

  const auto newer = IdentifyRelease("1.1.10.0-guffa.1 · Release");
  assert(newer && CheckIndex(index, *newer).status == CheckStatus::Current);
  assert(!IdentifyRelease("1.1.9.5-guffa.9"));
  assert(!IdentifyRelease("1.1.9.5 · Pre b123.1"));
  assert(!IdentifyRelease("1.1.9.5 · DEV local"));
  assert(!IdentifyRelease("1.1.9.5-guffa.7-dev.9 · Pre"));

  for (const auto bad : {"v1.1.9.5-guffa.010", "v1.1.9.5-guffa.4294967296", "v1.1.9.5-guffa.0",
                         "v1.1.9.5-guffa.10/other", "v1.1.9.5-guffa.10-dev.1"})
    assert(!ParseReleaseTag(bad, Channel::Release));
  assert(CheckIndex(R"({"schema":1,"channels":{"release":"v1.1.9.5-guffa.10"}})", *pre).status == CheckStatus::Failed);
  assert(CheckIndex(R"({"schema":2,"channels":{}})", *release).status == CheckStatus::Failed);
  assert(CheckIndex("not JSON", *dev).status == CheckStatus::Failed);
  assert(CheckIndex(std::string(max_index_bytes + 1, ' '), *dev).status == CheckStatus::Failed);
}
