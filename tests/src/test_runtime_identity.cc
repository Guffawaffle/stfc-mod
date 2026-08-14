#include <ostream>

#include <doctest/doctest.h>

#include "runtime_identity.h"

#include <string>

TEST_CASE("canonical runtime identity is unmistakably downstream")
{
  const auto& identity = runtime_identity::Current();
  const auto  support  = runtime_identity::SupportIdentity(identity);

  CHECK(identity.distribution_id == "guffawaffle.stfc-community-mod");
  CHECK(identity.display_name == "Guffawaffle STFC Mod");
  CHECK(identity.unofficial_label == "Unofficial downstream build");
  CHECK(support.find("distribution=guffawaffle.stfc-community-mod") != std::string::npos);
  CHECK(support.find("source=") != std::string::npos);
  CHECK(support.find("upstream=netniV/stfc-mod@v1.1.4#") != std::string::npos);
}

TEST_CASE("test support identity includes target expiry and support boundary")
{
  const runtime_identity::Record identity{
      .distribution_id    = "guffawaffle.stfc-community-mod",
      .display_name       = "Guffawaffle STFC Mod",
      .downstream_version = "2.1.0-test.1",
      .unofficial_label   = "Unofficial downstream build",
      .build_class        = "test",
      .build_class_label  = "Unofficial test build",
      .source_state_id    = "git:0123456789abcdef0123456789abcdef01234567",
      .base_commit        = "0123456789abcdef0123456789abcdef01234567",
      .upstream_base      = "netniV/stfc-mod@v1.1.4#d912611fa1eca49fc54f363bdf8377dfebf8def0",
      .test_target        = "netniV/stfc-mod#203+#205",
      .test_expiry        = "superseded when both PRs merge or close",
      .support_boundary   = "Guffawaffle test channel only",
      .reproducible       = true,
  };

  const auto support = runtime_identity::SupportIdentity(identity);
  CHECK(support.find("class=test") != std::string::npos);
  CHECK(support.find("target=netniV/stfc-mod#203+#205") != std::string::npos);
  CHECK(support.find("expires=superseded when both PRs merge or close") != std::string::npos);
  CHECK(support.find("support=Guffawaffle test channel only") != std::string::npos);
}

TEST_CASE("short runtime label remains explicit without full provenance")
{
  const auto label = runtime_identity::ShortRuntimeLabel();
  CHECK(label.find("Guffawaffle STFC Mod") != std::string::npos);
  CHECK(label.find("Unofficial downstream build") != std::string::npos);
}
