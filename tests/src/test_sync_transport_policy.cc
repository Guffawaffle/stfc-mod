#include <doctest/doctest.h>

#include "patches/sync_transport_policy.h"

TEST_SUITE("sync_transport_policy")
{
  TEST_CASE("verify_ssl stays enabled unless the explicit unsafe override is also set")
  {
    SyncConfig config;

    auto decision = http::DecideSyncTlsVerification(config);
    CHECK_FALSE(decision.disable_verification);
    CHECK_FALSE(decision.warn_verify_ssl_ignored);
    CHECK_FALSE(decision.emit_unsafe_tls_error);

    config.verify_ssl = false;
    decision = http::DecideSyncTlsVerification(config);
    CHECK_FALSE(decision.disable_verification);
    CHECK(decision.warn_verify_ssl_ignored);
    CHECK_FALSE(decision.emit_unsafe_tls_error);

    config.allow_unsafe_tls_without_certificate_validation = true;
    decision = http::DecideSyncTlsVerification(config);
    CHECK(decision.disable_verification);
    CHECK_FALSE(decision.warn_verify_ssl_ignored);
    CHECK(decision.emit_unsafe_tls_error);
  }

  TEST_CASE("scopely session headers are rebuilt from the latest snapshot")
  {
    http::headers::SessionHeaderSnapshot first_snapshot{
        .gameServerUrl = "https://example.invalid",
        .instanceSessionId = "session-one",
        .instanceId = 7,
        .unityVersion = "6000.0.52f1",
        .primeVersion = "1.000.1",
    };

    const auto first_headers = http::BuildScopelySessionHeaders(first_snapshot, "txn-1");
    CHECK(first_headers.transaction_id == "txn-1");
    CHECK(first_headers.auth_session_id == "session-one");
    CHECK(first_headers.prime_version == "1.000.1");
    CHECK(first_headers.instance_id == "007");
    CHECK(first_headers.unity_version == "6000.0.52f1");

    http::headers::SessionHeaderSnapshot second_snapshot = first_snapshot;
    second_snapshot.instanceSessionId = "session-two";
    second_snapshot.instanceId = 42;
    second_snapshot.primeVersion = "1.000.2";
    second_snapshot.unityVersion = "6000.0.60f1";

    const auto second_headers = http::BuildScopelySessionHeaders(second_snapshot, "txn-2");
    CHECK(second_headers.transaction_id == "txn-2");
    CHECK(second_headers.auth_session_id == "session-two");
    CHECK(second_headers.prime_version == "1.000.2");
    CHECK(second_headers.instance_id == "042");
    CHECK(second_headers.unity_version == "6000.0.60f1");
  }
}