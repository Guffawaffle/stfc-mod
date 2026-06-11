#include <doctest/doctest.h>

#include "patches/fleet_runtime_diagnostics.h"

TEST_SUITE("fleet_runtime_diagnostics")
{
  TEST_CASE("suppressed unchanged increments counters and preserves latest trigger")
  {
    fleet_runtime_diagnostics_reset();

    const auto dispatch = gameplay_dispatch_context(
        "deployment-set-course-response-event",
        "DeploymentRuntimeObservers",
        "Digit.PrimeServer.Events.DeploymentEvents.TriggerSetCourseResponseEvent",
        "deployment-set-course-response-event",
        "defer-fleet-runtime-snapshot");
    fleet_runtime_diagnostics_trigger(dispatch);
    fleet_runtime_diagnostics_capture_attempt(dispatch, 4);
    fleet_runtime_diagnostics_suppressed_unchanged(dispatch, 4);

    const auto snapshot = fleet_runtime_diagnostics_snapshot();
    CHECK(snapshot.triggerCount == 1);
    CHECK(snapshot.captureAttemptCount == 1);
    CHECK(snapshot.suppressedUnchangedCount == 1);
    CHECK(snapshot.suppressedNonMeaningfulCount == 0);
    CHECK(snapshot.latestTriggerSource == "deployment-set-course-response-event");
    CHECK(snapshot.latestTriggerOwner == "DeploymentRuntimeObservers");
    CHECK(snapshot.latestTriggerSeam == "Digit.PrimeServer.Events.DeploymentEvents.TriggerSetCourseResponseEvent");
    CHECK(snapshot.latestTriggerReason == "deployment-set-course-response-event");
    CHECK(snapshot.latestTriggerEffect == "defer-fleet-runtime-snapshot");
    CHECK(snapshot.latestTriggerAtMs > 0);
  }

  TEST_CASE("trace captures safe slot counts and redacted status summary")
  {
    fleet_runtime_diagnostics_reset();

    FleetObservation fleet;
    fleet.tracked = true;

    std::array<FleetSlotObservation, kFleetIndexMax> slots{};
    for (int index = 0; index < kFleetIndexMax; ++index) {
      slots[static_cast<size_t>(index)].slotIndex = index;
    }
    slots[0].present = true;
    slots[0].currentState = 2;
    slots[1].present = true;
    slots[1].currentState = 64;

    const auto dispatch = gameplay_dispatch_context(
        "deployment-battle-end-event",
        "DeploymentRuntimeObservers",
        "Digit.PrimeServer.Events.DeploymentEvents.TriggerBattleEndEvent",
        "deployment-battle-end-event",
        "defer-fleet-runtime-snapshot");
    const auto trace = fleet_runtime_diagnostics_make_trace(dispatch, fleet, slots, 123456, 7);

    CHECK(trace.localSequence == 1);
    CHECK(trace.observedAtMs == 123456);
    CHECK(trace.captureDurationMs == 7);
    CHECK(trace.slotCount == 10);
    CHECK(trace.presentShipCount == 2);
    CHECK(trace.source == "deployment-battle-end-event");
    CHECK(trace.owner == "DeploymentRuntimeObservers");
    CHECK(trace.seam == "Digit.PrimeServer.Events.DeploymentEvents.TriggerBattleEndEvent");
    CHECK(trace.reason == "deployment-battle-end-event");
    CHECK(trace.effect == "defer-fleet-runtime-snapshot");
    CHECK(trace.statusSummary == "battling:1,docked:1,empty:8");

    const auto snapshot = fleet_runtime_diagnostics_snapshot();
    CHECK(snapshot.latestLocalSequence == 1);
    CHECK(snapshot.latestObservedAtMs == 123456);
  }

  TEST_CASE("queued and dropped diagnostics update queue and post counters")
  {
    fleet_runtime_diagnostics_reset();

    FleetRuntimeTraceContext trace;
    trace.localSequence = 9;
    trace.observedAtMs = 222;
    trace.source = "deployment-course-start-event";
    trace.owner = "DeploymentRuntimeObservers";
    trace.seam = "Digit.PrimeServer.Events.DeploymentEvents.TriggerCourseStartEvent";
    trace.reason = "deployment-course-start-event";
    trace.effect = "defer-fleet-runtime-snapshot";
    trace.slotCount = 10;
    trace.presentShipCount = 3;
    trace.statusSummary = "docked:2,empty:7,mining:1";

    fleet_runtime_diagnostics_scheduler_queue(trace, true, 1);
    fleet_runtime_diagnostics_target_queue(trace, "cloud-majel", "majel", false, 256, 5,
                                           "target-queue-full");
    fleet_runtime_diagnostics_post_result(trace, "cloud-majel", "majel", true, 202, "", 35);
    fleet_runtime_diagnostics_post_result(trace, "cloud-majel", "majel", false, 0, "transport", 0);

    const auto snapshot = fleet_runtime_diagnostics_snapshot();
    CHECK(snapshot.schedulerQueueAcceptedCount == 1);
    CHECK(snapshot.schedulerQueueDroppedCount == 0);
    CHECK(snapshot.targetQueueAcceptedCount == 0);
    CHECK(snapshot.targetQueueDroppedCount == 1);
    CHECK(snapshot.postSuccessCount == 1);
    CHECK(snapshot.postFailureCount == 1);
  }
}
