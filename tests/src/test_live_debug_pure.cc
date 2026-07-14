#include "test_pure_common.h"

#include <atomic>
#include <thread>

// ===========================================================================
// live_debug_pure
// ===========================================================================

TEST_SUITE("live_debug_recent_event_store")
{
  TEST_CASE("snapshot preserves order and trims to capacity")
  {
    LiveDebugRecentEventStore store(2);

    store.append("event-a", nlohmann::json{{"value", 1}}, 100);
    store.append("event-b", nlohmann::json{{"value", 2}}, 200);
    store.append("event-a", nlohmann::json{{"value", 3}}, 300);

    const auto snapshot = store.snapshot();
    CHECK(snapshot.count == 2);
    CHECK(snapshot.capacity == 2);
    CHECK(snapshot.returnedCount == 2);
    CHECK(snapshot.matchedCount == 2);
    CHECK(snapshot.firstSeq == 2);
    CHECK(snapshot.lastSeq == 3);
    CHECK(snapshot.evictedCount == 1);
    REQUIRE(snapshot.events.size() == 2);
    CHECK(snapshot.events[0]["seq"] == 2);
    CHECK(snapshot.events[0]["kind"] == "event-b");
    CHECK(snapshot.events[1]["seq"] == 3);
    CHECK(snapshot.events[1]["kind"] == "event-a");
    CHECK(snapshot.kindCounts["event-a"] == 1);
    CHECK(snapshot.kindCounts["event-b"] == 1);
    CHECK(snapshot.bufferKindCounts["event-a"] == 1);
    CHECK(snapshot.bufferKindCounts["event-b"] == 1);
  }

  TEST_CASE("clear removes events but preserves monotonic sequence")
  {
    LiveDebugRecentEventStore store(4);

    store.append("event-a", nlohmann::json::object(), 100);
    store.append("event-b", nlohmann::json::object(), 200);

    CHECK(store.clear() == 2);

    store.append("event-c", nlohmann::json::object(), 300);

    const auto snapshot = store.snapshot();
    CHECK(snapshot.count == 1);
    CHECK(snapshot.clearCount == 1);
    REQUIRE(snapshot.events.size() == 1);
    CHECK(snapshot.events[0]["seq"] == 3);
    CHECK(snapshot.events[0]["kind"] == "event-c");
  }

  TEST_CASE("snapshot query supports afterSeq kind limit and summary metadata")
  {
    LiveDebugRecentEventStore store(3);

    store.append("event-a", nlohmann::json{{"value", 1}}, 100);
    store.append("event-b", nlohmann::json{{"value", 2}}, 200);
    store.append("event-a", nlohmann::json{{"value", 3}}, 300);
    store.append("event-a", nlohmann::json{{"value", 4}}, 400);

    LiveDebugRecentEventStoreQuery query;
    query.afterSeq       = 0;
    query.kind           = "event-a";
    query.limit          = 1;
    query.includeDetails = false;

    const auto snapshot = store.snapshot(query);
    CHECK(snapshot.count == 3);
    CHECK(snapshot.matchedCount == 2);
    CHECK(snapshot.returnedCount == 1);
    CHECK(snapshot.queryScanCount == 2);
    CHECK(snapshot.queryTextScanCount == 0);
    CHECK(snapshot.queryGap == true);
    CHECK(snapshot.queryUsedKindIndex == true);
    CHECK(snapshot.missingCountBeforeFirstReturned == 1);
    REQUIRE(snapshot.events.size() == 1);
    CHECK(snapshot.events[0]["seq"] == 4);
    CHECK(snapshot.events[0]["kind"] == "event-a");
    CHECK_FALSE(snapshot.events[0].contains("details"));
    CHECK(snapshot.kindCounts["event-a"] == 1);
    CHECK(snapshot.bufferKindCounts["event-a"] == 2);
    CHECK(snapshot.bufferKindCounts["event-b"] == 1);
  }

  TEST_CASE("snapshot query supports multi-kind and server-side text matching")
  {
    LiveDebugRecentEventStore store(6);

    store.append("toast-notification-observed", nlohmann::json{{"message", "Alliance help requested"}}, 100);
    store.append("fleet-slot-state-changed", nlohmann::json{{"state", "Mining"}}, 200);
    store.append("top-canvas-changed", nlohmann::json{{"activeChildName", "AllianceBanner"}}, 300);
    store.append("toast-notification-observed", nlohmann::json{{"message", "Warp complete"}}, 400);

    LiveDebugRecentEventStoreQuery wildcard_query;
    wildcard_query.kinds          = {"toast-notification-observed", "top-canvas-changed"};
    wildcard_query.match          = "*alliance*";
    wildcard_query.includeDetails = false;

    const auto wildcard_snapshot = store.snapshot(wildcard_query);
    CHECK(wildcard_snapshot.count == 4);
    CHECK(wildcard_snapshot.matchedCount == 2);
    CHECK(wildcard_snapshot.returnedCount == 2);
    CHECK(wildcard_snapshot.queryScanCount == 3);
    CHECK(wildcard_snapshot.queryTextScanCount == 3);
    CHECK(wildcard_snapshot.queryUsedKindIndex == true);
    REQUIRE(wildcard_snapshot.events.size() == 2);
    CHECK(wildcard_snapshot.events[0]["kind"] == "toast-notification-observed");
    CHECK(wildcard_snapshot.events[1]["kind"] == "top-canvas-changed");
    CHECK_FALSE(wildcard_snapshot.events[0].contains("details"));
    CHECK(wildcard_snapshot.kindCounts["toast-notification-observed"] == 1);
    CHECK(wildcard_snapshot.kindCounts["top-canvas-changed"] == 1);
    CHECK(wildcard_snapshot.bufferKindCounts["fleet-slot-state-changed"] == 1);

    LiveDebugRecentEventStoreQuery exact_query;
    exact_query.match = "TOAST-NOTIFICATION-OBSERVED";
    exact_query.exact = true;

    const auto exact_snapshot = store.snapshot(exact_query);
    CHECK(exact_snapshot.matchedCount == 2);
    CHECK(exact_snapshot.queryScanCount == 4);
    CHECK(exact_snapshot.queryTextScanCount == 2);
    CHECK(exact_snapshot.queryUsedKindIndex == false);
    REQUIRE(exact_snapshot.events.size() == 2);
    CHECK(exact_snapshot.events[0]["seq"] == 1);
    CHECK(exact_snapshot.events[1]["seq"] == 4);
  }

  TEST_CASE("snapshot can run while append and clear mutate the store")
  {
    LiveDebugRecentEventStore store(64);
    std::atomic_bool          writers_done     = false;
    std::atomic_bool          invariant_failed = false;

    std::thread writer_a([&store] {
      for (int i = 0; i < 250; ++i) {
        store.append("event-a", nlohmann::json{{"payload", i}}, i);
      }
    });

    std::thread writer_b([&store] {
      for (int i = 0; i < 250; ++i) {
        store.append("event-b", nlohmann::json{{"payload", i}}, i);
      }
    });

    std::thread clearer([&store] {
      for (int i = 0; i < 8; ++i) {
        std::this_thread::yield();
        store.clear();
      }
    });

    std::thread reader([&store, &writers_done, &invariant_failed] {
      LiveDebugRecentEventStoreQuery query;
      query.kinds          = {"event-a", "event-b"};
      query.match          = "payload";
      query.limit          = 16;
      query.includeDetails = false;

      while (!writers_done.load()) {
        const auto snapshot = store.snapshot(query);
        if (snapshot.count > snapshot.capacity || snapshot.returnedCount != snapshot.events.size()
            || snapshot.returnedCount > query.limit) {
          invariant_failed.store(true);
        }
      }
    });

    writer_a.join();
    writer_b.join();
    clearer.join();
    writers_done.store(true);
    reader.join();

    const auto snapshot = store.snapshot();
    CHECK(snapshot.count <= snapshot.capacity);
    CHECK_FALSE(invariant_failed.load());
  }

  TEST_CASE("recent-events request parser merges filters and summary flags")
  {
    const nlohmann::json request = {
        {"afterSeq", 41},
        {"last", 3},
        {"kinds", nlohmann::json::array({"toast-notification-observed"})},
        {"kind", "top-canvas-changed"},
        {"match", "alliance"},
        {"exact", true},
        {"summary", true},
    };

    const auto query = live_debug_recent_events_query_from_request(request);
    CHECK(query.afterSeq == 41);
    CHECK(query.limit == 3);
    CHECK(query.kind == "");
    REQUIRE(query.kinds.size() == 2);
    CHECK(query.kinds[0] == "toast-notification-observed");
    CHECK(query.kinds[1] == "top-canvas-changed");
    CHECK(query.match == "alliance");
    CHECK(query.exact == true);
    CHECK(query.includeDetails == false);
  }

  TEST_CASE("recent-events request parser prefers includeDetails over summary and limit over last")
  {
    const nlohmann::json request = {
        {"limit", 7}, {"last", 2}, {"includeDetails", true}, {"summary", true}, {"kind", "fleet-slot-state-changed"},
    };

    const auto query = live_debug_recent_events_query_from_request(request);
    CHECK(query.limit == 7);
    CHECK(query.kind == "fleet-slot-state-changed");
    CHECK(query.includeDetails == true);
  }

  TEST_CASE("recent-events result builder preserves metadata and nulls empty seq values")
  {
    LiveDebugRecentEventStoreSnapshot snapshot;
    snapshot.count                           = 4;
    snapshot.returnedCount                   = 2;
    snapshot.matchedCount                    = 3;
    snapshot.capacity                        = 256;
    snapshot.nextSeq                         = 12;
    snapshot.evictedCount                    = 5;
    snapshot.clearCount                      = 1;
    snapshot.queryScanCount                  = 9;
    snapshot.queryTextScanCount              = 4;
    snapshot.queryGap                        = true;
    snapshot.queryUsedKindIndex              = true;
    snapshot.missingCountBeforeFirstReturned = 2;
    snapshot.kindCounts                      = nlohmann::json{{"toast-notification-observed", 2}};
    snapshot.bufferKindCounts                = nlohmann::json{{"toast-notification-observed", 3}};
    snapshot.events = nlohmann::json::array({nlohmann::json{{"seq", 10}}, nlohmann::json{{"seq", 11}}});

    const auto result = live_debug_recent_events_result(snapshot);
    CHECK(result["count"] == 4);
    CHECK(result["returnedCount"] == 2);
    CHECK(result["matchedCount"] == 3);
    CHECK(result["capacity"] == 256);
    CHECK(result["firstSeq"].is_null());
    CHECK(result["lastSeq"].is_null());
    CHECK(result["nextSeq"] == 12);
    CHECK(result["evictedCount"] == 5);
    CHECK(result["clearCount"] == 1);
    CHECK(result["queryScanCount"] == 9);
    CHECK(result["queryTextScanCount"] == 4);
    CHECK(result["queryGap"] == true);
    CHECK(result["queryUsedKindIndex"] == true);
    CHECK(result["missingCountBeforeFirstReturned"] == 2);
    CHECK(result["kindCounts"]["toast-notification-observed"] == 2);
    CHECK(result["bufferKindCounts"]["toast-notification-observed"] == 3);
    REQUIRE(result["events"].size() == 2);
  }
}


TEST_SUITE("live_debug_ui_serializers")
{
  TEST_CASE("top canvas serializer preserves visible metadata")
  {
    TopCanvasObservation observation;
    observation.found            = true;
    observation.pointer          = "0x1234";
    observation.className        = "GalaxyScreen";
    observation.classNamespace   = "Scopely.UI";
    observation.name             = "GalaxyTopCanvas";
    observation.visible          = true;
    observation.enabled          = true;
    observation.internalVisible  = false;
    observation.activeChildNames = {"ArmadaButton", "WarpHud"};

    const auto result = top_canvas_observation_to_json(observation);

    CHECK(result["found"] == true);
    CHECK(result["pointer"] == "0x1234");
    CHECK(result["className"] == "GalaxyScreen");
    CHECK(result["activeChildNames"].size() == 2);
    CHECK(result["visibleOnlyHint"] == true);
  }

  TEST_CASE("station warning serializer labels target type")
  {
    StationWarningObservation observation;
    observation.tracked                = true;
    observation.pointer                = "0x777";
    observation.hasContext             = true;
    observation.targetType             = 3;
    observation.targetFleetId          = 42;
    observation.targetUserId           = "player-1";
    observation.quickScanTargetFleetId = 99;
    observation.quickScanTargetId      = "scan-9";

    const auto result = station_warning_observation_to_json(observation);

    CHECK(result["tracked"] == true);
    CHECK(result["targetType"] == 3);
    CHECK(result["targetTypeName"] == "Station");
    CHECK(result["targetFleetId"] == 42);
    CHECK(result["quickScanTargetId"] == "scan-9");
  }

  TEST_CASE("navigation interaction serializer emits readable context names")
  {
    NavigationInteractionObservation observation;
    observation.tracked      = true;
    observation.trackedCount = 1;

    NavigationInteractionObservation::Entry entry;
    entry.pointer               = "0xabc";
    entry.hasContext            = true;
    entry.contextDataState      = 1;
    entry.inputInteractionType  = 14;
    entry.userId                = "enemy-7";
    entry.isMarauder            = true;
    entry.threatLevel           = 0;
    entry.validNavigationInput  = true;
    entry.showSetCourseArm      = true;
    entry.locationTranslationId = 123456;
    entry.poiPointer            = "0xpoi";
    observation.entries.push_back(entry);

    const auto result = navigation_interaction_observation_to_json(observation);

    CHECK(result["tracked"] == true);
    CHECK(result["trackedCount"] == 1);
    REQUIRE(result["entries"].size() == 1);
    CHECK(result["entries"][0]["contextDataStateName"] == "Verifying");
    CHECK(result["entries"][0]["inputInteractionTypeName"] == "TapArmadaLocation");
    CHECK(result["entries"][0]["threatLevelName"] == "VeryHard");
    CHECK(result["entries"][0]["poiPointer"] == "0xpoi");
  }
}


TEST_SUITE("live_debug_fleet_serializers")
{
  TEST_CASE("fleet state names keep expected labels")
  {
    CHECK(fleet_state_name_from_value(-1) == doctest::String("None"));
    CHECK(fleet_state_name_from_value(2) == doctest::String("Docked"));
    CHECK(fleet_state_name_from_value(18) == doctest::String("CanReplaceOfficers"));
    CHECK(fleet_state_name_from_value(2048) == doctest::String("AutoHunting"));
    CHECK(fleet_state_name_from_value(2552) == doctest::String("CannotMove"));
    CHECK(fleet_state_name_from_value(2947) == doctest::String("CanManage"));
    CHECK(fleet_state_name_from_value(3589) == doctest::String("CanBeTargetedByAbility"));
    CHECK(fleet_state_name_from_value(3591) == doctest::String("CanEngage"));
    CHECK(fleet_state_name_from_value(4096) == doctest::String("Outposting"));
    CHECK(fleet_state_name_from_value(5637) == doctest::String("CanRecall"));
    CHECK(fleet_state_name_from_value(8133) == doctest::String("Deployed"));
    CHECK(fleet_state_name_from_value(8135) == doctest::String("CanLocate"));
    CHECK(fleet_state_name_from_value(999999) == doctest::String("Unmapped"));
  }

  TEST_CASE("fleet observation serializer includes tracked fleet details")
  {
    FleetObservation observation;
    observation.tracked              = true;
    observation.pointer              = "0xfleetbar";
    observation.selectedIndex        = 3;
    observation.hasController        = true;
    observation.hasFleet             = true;
    observation.fleetId              = 44;
    observation.currentState         = 2;
    observation.previousState        = 1;
    observation.cargoFillPercent     = 37;
    observation.cargoFillBasisPoints = 3700;
    observation.hullName             = "Mayflower";
    observation.shipIdentityProbeId  = "2679690622826529803";

    const auto result = fleet_observation_to_json(observation);

    CHECK(result["tracked"] == true);
    CHECK(result["pointer"] == "0xfleetbar");
    CHECK(result["fleet"]["id"] == 44);
    CHECK(result["fleet"]["currentStateName"] == "Docked");
    CHECK(result["fleet"]["previousStateName"] == "IdleInSpace");
    CHECK(result["fleet"]["cargoFillBasisPoints"] == 3700);
    CHECK(result["fleet"]["hullName"] == "Mayflower");
    CHECK(result["fleet"]["shipIdentityProbe"]["shipId"] == "2679690622826529803");
    CHECK(result["fleet"]["shipIdentityProbe"]["source"] == "FleetPlayerData.Ship.ID");
  }

  TEST_CASE("fleet slot serializer preserves slot order and readable states")
  {
    std::array<FleetSlotObservation, kFleetIndexMax> observations{};
    observations[0].slotIndex            = 0;
    observations[1].slotIndex            = 1;
    observations[1].selected             = true;
    observations[1].present              = true;
    observations[1].fleetId              = 9001;
    observations[1].currentState         = 256;
    observations[1].previousState        = 128;
    observations[1].cargoFillPercent     = 82;
    observations[1].cargoFillBasisPoints = 8200;
    observations[1].hullName             = "Enterprise";
    observations[1].shipIdentityProbeId  = "2679690622826529803";

    const auto result = fleet_slots_to_json(observations);

    REQUIRE(result.size() == kFleetIndexMax);
    CHECK(result[0]["slotIndex"] == 0);
    CHECK(result[1]["selected"] == true);
    CHECK(result[1]["fleetId"] == 9001);
    CHECK(result[1]["currentStateName"] == "Warping");
    CHECK(result[1]["previousStateName"] == "WarpCharging");
    CHECK(result[1]["hullName"] == "Enterprise");
    CHECK(result[1]["shipIdentityProbe"]["shipId"] == "2679690622826529803");
    CHECK(result[1]["shipIdentityProbe"]["source"] == "FleetPlayerData.Ship.ID");
  }
}


TEST_SUITE("live_debug_viewer_serializers")
{
  TEST_CASE("occupied state names stay readable")
  {
    CHECK(occupied_state_name_from_value(0) == doctest::String("NotOccupied"));
    CHECK(occupied_state_name_from_value(1) == doctest::String("LocalPlayerOccupied"));
    CHECK(occupied_state_name_from_value(2) == doctest::String("OtherPlayerOccupied"));
    CHECK(occupied_state_name_from_value(99) == doctest::String("Unknown"));
  }

  TEST_CASE("target viewer serializer emits tracked pointers and nulls")
  {
    TargetViewerObservation observation;
    observation.preScanTargetTracked        = true;
    observation.preScanTargetPointer        = "0xpre";
    observation.preScanStationTargetTracked = false;
    observation.celestialViewerTracked      = true;
    observation.celestialViewerPointer      = "0xcelestial";

    const auto result = target_viewer_observation_to_json(observation);

    CHECK(result["preScanTargetTracked"] == true);
    CHECK(result["preScanTarget"]["pointer"] == "0xpre");
    CHECK(result["preScanStationTargetTracked"] == false);
    CHECK(result["preScanStationTarget"].is_null());
    CHECK(result["celestialViewer"]["pointer"] == "0xcelestial");
  }

  TEST_CASE("mine viewer serializer includes timer and occupied state metadata")
  {
    MineViewerObservation observation;
    observation.miningViewerTracked      = true;
    observation.miningPointer            = "0xmine";
    observation.enabled                  = true;
    observation.isActiveAndEnabled       = true;
    observation.isInfoShown              = true;
    observation.hasParent                = true;
    observation.parentIsShowing          = false;
    observation.occupiedState            = 2;
    observation.hasScanEngageButtons     = true;
    observation.hasTimer                 = true;
    observation.timerState               = 4;
    observation.timerType                = 8;
    observation.timerRemainingSeconds    = 75;
    observation.timerRemainingBucket     = 60;
    observation.starNodeViewerTracked    = true;
    observation.starNodePointer          = "0xstar";
    observation.starNodeEnabled          = false;
    observation.starNodeActiveAndEnabled = true;

    const auto result = mine_viewer_observation_to_json(observation);

    CHECK(result["miningViewerTracked"] == true);
    CHECK(result["miningViewer"]["pointer"] == "0xmine");
    CHECK(result["miningViewer"]["occupiedStateName"] == "OtherPlayerOccupied");
    CHECK(result["miningViewer"]["timer"]["remainingSeconds"] == 75);
    CHECK(result["miningViewer"]["timer"]["remainingSecondsBucket"] == 60);
    CHECK(result["starNodeViewer"]["pointer"] == "0xstar");
    CHECK(result["starNodeViewer"]["isActiveAndEnabled"] == true);
  }
}
