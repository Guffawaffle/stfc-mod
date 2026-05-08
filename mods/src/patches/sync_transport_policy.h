#pragma once

#include "patches/sync_transport.h"

#include <string>

namespace http
{
struct SyncTlsVerificationDecision {
  bool disable_verification = false;
  bool warn_verify_ssl_ignored = false;
  bool emit_unsafe_tls_error = false;
};

struct ScopelySessionHeaders {
  std::string transaction_id;
  std::string auth_session_id;
  std::string prime_version;
  std::string instance_id;
  std::string unity_version;
};

[[nodiscard]] SyncTlsVerificationDecision DecideSyncTlsVerification(const SyncConfig& config);
[[nodiscard]] ScopelySessionHeaders BuildScopelySessionHeaders(const headers::SessionHeaderSnapshot& snapshot,
                                                              std::string transaction_id);
} // namespace http