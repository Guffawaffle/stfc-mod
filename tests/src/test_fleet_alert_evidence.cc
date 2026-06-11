#include <doctest/doctest.h>

#include "patches/fleet_alert_evidence.h"

#include <nlohmann/json.hpp>

namespace
{
FleetAlertRuntimeContext runtime_context()
{
  return FleetAlertRuntimeContext{
      gameplay_dispatch_context("fleet-slot-arrived-in-system",
                                "FleetArrivalHooks",
                                "Digit.Prime.HUD.FleetStateWidget.SetWidgetData",
                                "fleet-slot-arrived-in-system",
                                "publish-fleet-alert-evidence"),
      "2026-06-11T12:34:56Z",
      1781181296000,
  };
}
} // namespace

TEST_SUITE("fleet_alert_evidence")
{
  TEST_CASE("maps only arrival transition reasons to fleet alert event types")
  {
    CHECK(fleet_alert_event_type_for_transition_reason("fleet-slot-arrived-in-system") == "fleet.arrived_in_system");
    CHECK(fleet_alert_event_type_for_transition_reason("fleet-slot-arrived-at-destination")
          == "fleet.arrived_at_destination");
    CHECK_FALSE(fleet_alert_event_type_for_transition_reason("fleet-slot-combat-ended").has_value());
  }

  TEST_CASE("builds fleet arrival evidence with string-safe identifiers and provenance")
  {
    FleetAlertFleetTransitionEvidence evidence;
    evidence.runtime = runtime_context();
    evidence.alert_event_type = "fleet.arrived_in_system";
    evidence.fleet_id = 12345678901234567890ull;
    evidence.previous_state = 256;
    evidence.current_state = 512;
    evidence.previous_state_name = "Warping";
    evidence.current_state_name = "Impulsing";
    evidence.ship.ship_id = "9876543210987654321";
    evidence.ship.hull_spec_id = "1307832955";
    evidence.ship.display_name = "Squall";
    evidence.ship.hull_name = "USS Enterprise";
    evidence.missing_evidence.push_back("systemId");

    const auto event = build_fleet_transition_alert_evidence_event(evidence);

    CHECK(event["protocolVersion"] == "stfc.sidecar.events.v0");
    CHECK(event["type"] == "fleet.alert_evidence");
    CHECK(event["schemaVersion"] == "stfc.fleet.alert_evidence.v0");
    CHECK(event["eventType"] == "fleet.arrived_in_system");
    CHECK(event["timestamp"] == "2026-06-11T12:34:56Z");
    CHECK(event["capturedAtUnixMs"] == 1781181296000);
    CHECK(event["fleet"]["fleetId"] == "12345678901234567890");
    CHECK(event["fleet"]["state"]["previousName"] == "Warping");
    CHECK(event["fleet"]["state"]["currentName"] == "Impulsing");
    CHECK(event["ship"]["shipId"] == "9876543210987654321");
    CHECK(event["ship"]["hullSpecId"] == "1307832955");
    CHECK(event["dispatch"]["owner"] == "FleetArrivalHooks");
    CHECK(event["dispatch"]["seam"] == "Digit.Prime.HUD.FleetStateWidget.SetWidgetData");
    CHECK(event["missingEvidence"][0] == "systemId");
  }

  TEST_CASE("builds incoming attack evidence without final alert copy")
  {
    FleetAlertIncomingAttackEvidence evidence;
    evidence.runtime = FleetAlertRuntimeContext{
        gameplay_dispatch_context("toast-fleet-queue",
                                  "FleetArrivalHooks",
                                  "Digit.Prime.HUD.ToastFleetObserver.QueueNotifications",
                                  "incoming-attack-materialized",
                                  "publish-fleet-alert-evidence"),
        "2026-06-11T12:35:10Z",
        1781181310000,
    };
    evidence.target_type = 1;
    evidence.target_type_name = "Fleet";
    evidence.target_fleet_id = 4001;
    evidence.resolved_fleet_id = 4001;
    evidence.resolved_fleet_state = 512;
    evidence.resolved_fleet_state_name = "Impulsing";
    evidence.target_ship_display_name = "Squall";
    evidence.attacker_fleet_type = 2;
    evidence.attacker_kind = "Hostile";
    evidence.attacker_identity = "987";
    evidence.missing_evidence.push_back("systemId");

    const auto event = build_incoming_attack_alert_evidence_event(evidence);

    CHECK(event["type"] == "fleet.alert_evidence");
    CHECK(event["eventType"] == "fleet.incoming_attack");
    CHECK(event["target"]["fleetId"] == "4001");
    CHECK(event["target"]["targetTypeName"] == "Fleet");
    CHECK(event["fleet"]["fleetId"] == "4001");
    CHECK(event["fleet"]["shipDisplayName"] == "Squall");
    CHECK(event["attacker"]["kind"] == "Hostile");
    CHECK(event["attacker"]["identity"] == "987");
    CHECK_FALSE(event.contains("title"));
    CHECK_FALSE(event.contains("body"));
  }
}
