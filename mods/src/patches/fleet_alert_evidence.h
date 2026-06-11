/**
 * @file fleet_alert_evidence.h
 * @brief Pure builders for sidecar fleet alert evidence events.
 */
#pragma once

#include "patches/gameplay_dispatch_context.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

inline constexpr std::string_view kFleetAlertEvidenceEventType     = "fleet.alert_evidence";
inline constexpr std::string_view kFleetAlertEvidenceSchemaVersion = "stfc.fleet.alert_evidence.v0";

struct FleetAlertRuntimeContext {
  GameplayDispatchContext dispatch;
  std::string             timestamp_iso_utc;
  int64_t                 observed_at_unix_ms = 0;
};

struct FleetAlertShipEvidence {
  std::optional<std::string> ship_id;
  std::optional<std::string> hull_spec_id;
  std::string                display_name;
  std::string                hull_name;
};

struct FleetAlertFleetTransitionEvidence {
  FleetAlertRuntimeContext runtime;
  std::string              alert_event_type;
  uint64_t                 fleet_id = 0;
  std::optional<int>       slot_index;
  int                      previous_state = 0;
  int                      current_state = 0;
  std::string              previous_state_name;
  std::string              current_state_name;
  FleetAlertShipEvidence   ship;
  std::vector<std::string> missing_evidence;
};

struct FleetAlertIncomingAttackEvidence {
  FleetAlertRuntimeContext runtime;
  int                      target_type = -1;
  std::string              target_type_name;
  uint64_t                 target_fleet_id = 0;
  uint64_t                 resolved_fleet_id = 0;
  int                      resolved_fleet_state = 0;
  std::string              resolved_fleet_state_name;
  std::string              target_ship_display_name;
  int                      attacker_fleet_type = 0;
  std::string              attacker_kind;
  std::string              attacker_identity;
  std::vector<std::string> missing_evidence;
};

[[nodiscard]] std::optional<std::string> fleet_alert_event_type_for_transition_reason(std::string_view reason);

[[nodiscard]] nlohmann::json
build_fleet_transition_alert_evidence_event(const FleetAlertFleetTransitionEvidence& evidence);

[[nodiscard]] nlohmann::json
build_incoming_attack_alert_evidence_event(const FleetAlertIncomingAttackEvidence& evidence);
