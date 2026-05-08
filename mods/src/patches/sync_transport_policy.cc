#include "patches/sync_transport_policy.h"

#include <utility>

namespace
{
std::string format_instance_id(const int32_t instance_id)
{
  auto value = std::to_string(instance_id);
  if (value.starts_with('-') || value.size() >= 3) {
    return value;
  }

  return std::string(3 - value.size(), '0') + value;
}
} // namespace

namespace http
{
SyncTlsVerificationDecision DecideSyncTlsVerification(const SyncConfig& config)
{
  if (config.verify_ssl) {
    return {};
  }

  if (!config.allow_unsafe_tls_without_certificate_validation) {
    return {.disable_verification = false, .warn_verify_ssl_ignored = true, .emit_unsafe_tls_error = false};
  }

  return {.disable_verification = true, .warn_verify_ssl_ignored = false, .emit_unsafe_tls_error = true};
}

ScopelySessionHeaders BuildScopelySessionHeaders(const headers::SessionHeaderSnapshot& snapshot,
                                                 std::string transaction_id)
{
  return {
      std::move(transaction_id),
      snapshot.instanceSessionId,
      snapshot.primeVersion,
      format_instance_id(snapshot.instanceId),
      snapshot.unityVersion,
  };
}
} // namespace http