#include "patches/fleet_alert_evidence_dispatch.h"

#include "patches/sidecar_local_ingest.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace
{
using json = nlohmann::json;

constexpr std::string_view kFleetAlertEvidenceBoundary =
    "game thread copied fleet alert evidence before async sidecar publish";

SidecarLocalDispatchContext sidecar_context_for(const FleetAlertRuntimeContext& runtime)
{
  return sidecar_local_dispatch_context(runtime.dispatch,
                                        "stfc.fleet.alert_evidence.v0",
                                        "copied-gameplay-evidence",
                                        "sidecar-local.fleet-alert-evidence",
                                        kFleetAlertEvidenceBoundary);
}

void enqueue_fleet_alert_evidence(const json& event, const FleetAlertRuntimeContext& runtime)
{
  if (!sidecar_local_ingest::FleetAlertEvidenceEnabled()) {
    return;
  }

  auto events = json::array();
  events.push_back(event);
  if (!sidecar_local_ingest::EnqueueFleetAlertEvidence(events, sidecar_context_for(runtime))) {
    spdlog::debug("[FleetAlertEvidence] source={} owner={} seam={} reason={} effect={} enqueue=skipped",
                  runtime.dispatch.source,
                  runtime.dispatch.owner,
                  runtime.dispatch.seam,
                  runtime.dispatch.reason,
                  runtime.dispatch.effect);
  }
}
} // namespace

void fleet_alert_evidence_publish_fleet_transition(const FleetAlertFleetTransitionEvidence& evidence)
{ enqueue_fleet_alert_evidence(build_fleet_transition_alert_evidence_event(evidence), evidence.runtime); }

void fleet_alert_evidence_publish_incoming_attack(const FleetAlertIncomingAttackEvidence& evidence)
{ enqueue_fleet_alert_evidence(build_incoming_attack_alert_evidence_event(evidence), evidence.runtime); }
