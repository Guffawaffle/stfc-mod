#pragma once

#include "patches/sync_transport.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

struct MajelIngestEnvelopeInput {
  SyncConfig::Type sync_type = SyncConfig::Type::Ships;
  nlohmann::json   payload;
  std::string      event_id;
  std::string      source_version;
  std::string      install_id;
  std::string      session_id;
  uint64_t         sequence = 0;
  std::string      observed_at;
};

struct SyncTargetCapabilityInfo {
  std::string              name;
  SyncTargetConfig::Mode   mode = SyncTargetConfig::Mode::Legacy;
  std::vector<std::string> enabled_sync_types;
};

struct ModCapabilitySnapshotInput {
  std::string                           source_version;
  std::string                           platform;
  std::vector<SyncTargetCapabilityInfo> targets;
  std::vector<std::string>              supported_schemas;
};

[[nodiscard]] SyncTlsVerificationDecision DecideSyncTlsVerification(const SyncConfig& config);
[[nodiscard]] ScopelySessionHeaders BuildScopelySessionHeaders(const headers::SessionHeaderSnapshot& snapshot,
                                                              std::string transaction_id);
[[nodiscard]] bool SyncTargetUsesMajelEnvelope(SyncTargetConfig::Mode mode);
[[nodiscard]] bool SyncTargetAcceptsType(const SyncTargetConfig& target_config, SyncConfig::Type type);
[[nodiscard]] std::map<std::string, std::string> BuildSyncTargetHeaders(const SyncTargetConfig& target_config,
                                                                        std::string powered_by);
[[nodiscard]] nlohmann::json BuildModCapabilitySnapshot(const ModCapabilitySnapshotInput& input);
[[nodiscard]] std::optional<nlohmann::json> BuildFleetAssignmentSnapshot(const nlohmann::json& slot_delta);
[[nodiscard]] nlohmann::json BuildMajelIngestEnvelope(const MajelIngestEnvelopeInput& input);
} // namespace http
