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

std::string schema_for_sync_type(const SyncConfig::Type type, const nlohmann::json& payload)
{
  if (payload.is_object()) {
    if (const auto found = payload.find("schemaVersion"); found != payload.end() && found->is_string()) {
      return found->get<std::string>();
    }
    if (const auto found = payload.find("schema"); found != payload.end() && found->is_string()) {
      return found->get<std::string>();
    }
  }

  switch (type) {
    case SyncConfig::Type::Battles:
    case SyncConfig::Type::BattlelogsRealtime:
      return "stfc.battle.summary.v1";
    case SyncConfig::Type::FleetRuntime:
      return "stfc.fleet.runtime_snapshot.v1";
    case SyncConfig::Type::ModCapabilities:
      return "stfc.mod.capability_snapshot.v1";
    default:
      return "stfc.sync.delta_batch.v1";
  }
}

nlohmann::json payload_for_sync_type(const SyncConfig::Type type, const nlohmann::json& payload)
{
  if (schema_for_sync_type(type, payload) != "stfc.sync.delta_batch.v1") {
    return payload;
  }

  return nlohmann::json{{"syncType", to_string(type)}, {"items", payload}};
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

bool SyncTargetUsesMajelEnvelope(const SyncTargetConfig::Mode mode)
{ return mode == SyncTargetConfig::Mode::Majel || mode == SyncTargetConfig::Mode::SidecarBroker; }

bool SyncTargetAcceptsType(const SyncTargetConfig& target_config, const SyncConfig::Type type)
{
  if (type == SyncConfig::Type::ModCapabilities) {
    return SyncTargetUsesMajelEnvelope(target_config.mode);
  }

  for (const auto& option : SyncOptions) {
    if (option.type == type) {
      return target_config.*option.option;
    }
  }

  return false;
}

std::map<std::string, std::string> BuildSyncTargetHeaders(const SyncTargetConfig& target_config,
                                                          std::string powered_by)
{
  std::map<std::string, std::string> headers{
      {"Content-Type", "application/json"},
      {"X-Powered-By", std::move(powered_by)},
  };

  if (target_config.mode == SyncTargetConfig::Mode::Majel) {
    headers.emplace("Authorization", "Bearer " + target_config.token);
  } else {
    headers.emplace("stfc-sync-token", target_config.token);
  }

  return headers;
}

nlohmann::json BuildModCapabilitySnapshot(const ModCapabilitySnapshotInput& input)
{
  auto targets = nlohmann::json::array();
  for (const auto& target : input.targets) {
    targets.push_back({
        {"name", target.name},
        {"mode", to_string(target.mode)},
        {"enabledSyncTypes", target.enabled_sync_types},
    });
  }

  return nlohmann::json{
      {"schemaVersion", "stfc.mod.capability_snapshot.v1"},
      {"modVersion", input.source_version},
      {"platform", input.platform},
      {"targets", std::move(targets)},
      {"supportedSchemas", input.supported_schemas},
      {"privacy",
       {
           {"tokenRedacted", true},
           {"containsEndpointUrls", false},
           {"containsCallbacks", false},
           {"readOnly", true},
       }},
  };
}

nlohmann::json BuildMajelIngestEnvelope(const MajelIngestEnvelopeInput& input)
{
  const auto schema = schema_for_sync_type(input.sync_type, input.payload);

  return nlohmann::json{
      {"protocolVersion", "majel.ingest.v1"},
      {"eventId", input.event_id},
      {"source", "stfc-community-mod"},
      {"sourceVersion", input.source_version},
      {"installId", input.install_id},
      {"sessionId", input.session_id},
      {"sequence", input.sequence},
      {"observedAt", input.observed_at},
      {"schema", schema},
      {"classification", "cloud_private"},
      {"payload", payload_for_sync_type(input.sync_type, input.payload)},
  };
}
} // namespace http
