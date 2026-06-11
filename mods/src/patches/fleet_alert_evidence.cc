#include "patches/fleet_alert_evidence.h"

#include <nlohmann/json.hpp>

namespace
{
using json = nlohmann::json;

constexpr std::string_view kSidecarEventProtocolVersion = "stfc.sidecar.events.v0";
constexpr std::string_view kNativeEventSource           = "stfc-community-mod";

json dispatch_to_json(const GameplayDispatchContext& dispatch)
{
  return json{
      {"source", dispatch.source},
      {"owner", dispatch.owner},
      {"seam", dispatch.seam},
      {"reason", dispatch.reason},
      {"effect", dispatch.effect},
  };
}

json base_event(const FleetAlertRuntimeContext& runtime)
{
  return json{
      {"protocolVersion", kSidecarEventProtocolVersion},
      {"type", kFleetAlertEvidenceEventType},
      {"schemaVersion", kFleetAlertEvidenceSchemaVersion},
      {"timestamp", runtime.timestamp_iso_utc},
      {"source", kNativeEventSource},
      {"capturedAtUnixMs", runtime.observed_at_unix_ms},
      {"observedAtUnixMs", runtime.observed_at_unix_ms},
      {"dispatch", dispatch_to_json(runtime.dispatch)},
  };
}

void put_if_not_empty(json& object, std::string_view key, const std::string& value)
{
  if (!value.empty()) {
    object[std::string(key)] = value;
  }
}

void put_if_present(json& object, std::string_view key, const std::optional<std::string>& value)
{
  if (value.has_value() && !value->empty()) {
    object[std::string(key)] = *value;
  }
}

json ship_to_json(const FleetAlertShipEvidence& ship)
{
  json result = json::object();
  put_if_present(result, "shipId", ship.ship_id);
  put_if_present(result, "hullSpecId", ship.hull_spec_id);
  put_if_not_empty(result, "displayName", ship.display_name);
  put_if_not_empty(result, "hullName", ship.hull_name);
  return result;
}
} // namespace

std::optional<std::string> fleet_alert_event_type_for_transition_reason(std::string_view reason)
{
  if (reason == "fleet-slot-arrived-in-system") {
    return "fleet.arrived_in_system";
  }

  if (reason == "fleet-slot-arrived-at-destination") {
    return "fleet.arrived_at_destination";
  }

  return std::nullopt;
}

nlohmann::json build_fleet_transition_alert_evidence_event(const FleetAlertFleetTransitionEvidence& evidence)
{
  auto event = base_event(evidence.runtime);
  event["eventType"] = evidence.alert_event_type;

  json fleet{
      {"fleetId", std::to_string(evidence.fleet_id)},
      {"state",
       {
           {"previous", evidence.previous_state},
           {"previousName", evidence.previous_state_name},
           {"current", evidence.current_state},
           {"currentName", evidence.current_state_name},
       }},
  };
  if (evidence.slot_index.has_value()) {
    fleet["slotIndex"] = *evidence.slot_index;
  }
  event["fleet"] = std::move(fleet);

  auto ship = ship_to_json(evidence.ship);
  if (!ship.empty()) {
    event["ship"] = std::move(ship);
  }

  if (!evidence.missing_evidence.empty()) {
    event["missingEvidence"] = evidence.missing_evidence;
  }

  return event;
}

nlohmann::json build_incoming_attack_alert_evidence_event(const FleetAlertIncomingAttackEvidence& evidence)
{
  auto event = base_event(evidence.runtime);
  event["eventType"] = "fleet.incoming_attack";
  event["target"] = {
      {"targetType", evidence.target_type},
      {"targetTypeName", evidence.target_type_name},
  };
  if (evidence.target_fleet_id != 0) {
    event["target"]["fleetId"] = std::to_string(evidence.target_fleet_id);
  }

  if (evidence.resolved_fleet_id != 0 || !evidence.target_ship_display_name.empty()) {
    json fleet{
        {"state",
         {
             {"current", evidence.resolved_fleet_state},
             {"currentName", evidence.resolved_fleet_state_name},
         }},
    };
    if (evidence.resolved_fleet_id != 0) {
      fleet["fleetId"] = std::to_string(evidence.resolved_fleet_id);
    }
    put_if_not_empty(fleet, "shipDisplayName", evidence.target_ship_display_name);
    event["fleet"] = std::move(fleet);
  }

  json attacker{
      {"fleetType", evidence.attacker_fleet_type},
      {"kind", evidence.attacker_kind},
  };
  put_if_not_empty(attacker, "identity", evidence.attacker_identity);
  event["attacker"] = std::move(attacker);

  if (!evidence.missing_evidence.empty()) {
    event["missingEvidence"] = evidence.missing_evidence;
  }

  return event;
}
