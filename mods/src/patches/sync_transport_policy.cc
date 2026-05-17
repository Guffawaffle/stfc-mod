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
    case SyncConfig::Type::FleetAssignments:
      return "stfc.fleet.assignment_snapshot.v1";
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
  if (type == SyncConfig::Type::FleetAssignments) {
    return SyncTargetUsesMajelEnvelope(target_config.mode) && target_config.slots;
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

static nlohmann::json json_id_to_string(const nlohmann::json& value)
{
  if (value.is_null() || value.is_discarded()) {
    return nullptr;
  }
  if (value.is_string()) {
    return value;
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<uint64_t>());
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<int64_t>());
  }

  return nullptr;
}

static nlohmann::json officer_ids_to_strings(const nlohmann::json& value)
{
  auto officer_ids = nlohmann::json::array();
  if (!value.is_array()) {
    return officer_ids;
  }

  for (const auto& officer_id : value) {
    if (auto normalized = json_id_to_string(officer_id); !normalized.is_null()) {
      officer_ids.push_back(std::move(normalized));
    }
  }

  return officer_ids;
}

std::optional<nlohmann::json> BuildFleetAssignmentSnapshot(const nlohmann::json& slot_delta)
{
  if (!slot_delta.is_object()) {
    return std::nullopt;
  }

  const auto params = slot_delta.find("params");
  if (params == slot_delta.end() || !params->is_object()) {
    return std::nullopt;
  }

  const auto setup = params->find("setup");
  if (setup == params->end() || !setup->is_array()) {
    return std::nullopt;
  }

  auto assignments = nlohmann::json::array();
  for (const auto& item : *setup) {
    if (!item.is_object()) {
      continue;
    }

    assignments.push_back({
        {"drydockId", json_id_to_string(item.value("drydock_id", nlohmann::json(nullptr)))},
        {"shipId", json_id_to_string(item.value("ship_id", nlohmann::json(nullptr)))},
        {"officerIds", officer_ids_to_strings(item.value("officer_ids", nlohmann::json::array()))},
    });
  }

  return nlohmann::json{
      {"schemaVersion", "stfc.fleet.assignment_snapshot.v1"},
      {"sourceSlotId", json_id_to_string(slot_delta.value("sid", nlohmann::json(nullptr)))},
      {"sourceSlotSpecId", slot_delta.value("spec_id", nlohmann::json(nullptr))},
      {"presetName", params->value("name", "")},
      {"presetOrder", params->value("order", nlohmann::json(nullptr))},
      {"assignments", std::move(assignments)},
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
