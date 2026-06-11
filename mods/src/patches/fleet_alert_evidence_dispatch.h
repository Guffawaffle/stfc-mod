/**
 * @file fleet_alert_evidence_dispatch.h
 * @brief Sidecar-local publisher for copied fleet alert evidence.
 */
#pragma once

#include "patches/fleet_alert_evidence.h"

void fleet_alert_evidence_publish_fleet_transition(const FleetAlertFleetTransitionEvidence& evidence);
void fleet_alert_evidence_publish_incoming_attack(const FleetAlertIncomingAttackEvidence& evidence);
