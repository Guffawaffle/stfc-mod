#include "test_pure_common.h"

TEST_SUITE("config_redaction")
{
  TEST_CASE("runtime secret redaction never preserves token material")
  {
    const auto redacted = config_redaction::redact_secret_for_runtime_snapshot("super-secret-sync-token");
    CHECK(redacted == "<redacted>");
    CHECK(redacted.find("super") == std::string::npos);
    CHECK(redacted.find("token") == std::string::npos);
  }

  TEST_CASE("empty runtime secret remains diagnosable")
  { CHECK(config_redaction::redact_secret_for_runtime_snapshot("") == "<empty>"); }

  TEST_CASE("log token mask preserves shape without exposing full token")
  {
    const auto masked = config_redaction::mask_token_for_log("abcdefghi-sensitive-jklmnopqr");
    CHECK(masked.starts_with("abcdefghi"));
    CHECK(masked.ends_with("-jklmnopqr"));
    CHECK(masked.find("sensitive") == std::string::npos);
  }

  TEST_CASE("proxy userinfo masking preserves endpoint")
  {
    CHECK(config_redaction::mask_proxy_userinfo("socks5://user:pass@example.test:1080")
          == "socks5://****:****@example.test:1080");
    CHECK(config_redaction::mask_proxy_userinfo("http://user%40domain:pass@example.test/proxy")
          == "http://*************:****@example.test/proxy");
    CHECK(config_redaction::mask_proxy_userinfo("https://example.test/path?email=a@b.test")
          == "https://example.test/path?email=a@b.test");
  }
}
