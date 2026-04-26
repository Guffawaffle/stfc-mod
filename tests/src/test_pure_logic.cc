#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bounded_ttl_cache.h"
#include "patches/live_debug_event_store.h"
#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_ui_serializers.h"
#include "patches/live_debug_viewer_serializers.h"
#include "patches/notification_queue.h"
#include "patches/notification_text.h"
#include "str_utils_pure.h"
#include "testable_functions.h"

#include <chrono>
#include <utility>

// ===========================================================================
// str_utils_pure.h
// ===========================================================================

TEST_SUITE("str_utils")
{
  TEST_CASE("StripLeadingAsciiWhitespace")
  {
    CHECK(StripLeadingAsciiWhitespace("  hello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("\thello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("hello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("") == "");
    CHECK(StripLeadingAsciiWhitespace("   ") == "");
  }

  TEST_CASE("StripTrailingAsciiWhitespace")
  {
    CHECK(StripTrailingAsciiWhitespace("hello  ") == "hello");
    CHECK(StripTrailingAsciiWhitespace("hello\t") == "hello");
    CHECK(StripTrailingAsciiWhitespace("hello") == "hello");
    CHECK(StripTrailingAsciiWhitespace("") == "");
    CHECK(StripTrailingAsciiWhitespace("   ") == "");
  }

  TEST_CASE("StripAsciiWhitespace")
  {
    CHECK(StripAsciiWhitespace("  hello  ") == "hello");
    CHECK(StripAsciiWhitespace("  hello world  ") == "hello world");
    CHECK(StripAsciiWhitespace("hello") == "hello");
    CHECK(StripAsciiWhitespace("") == "");
  }

  TEST_CASE("AsciiStrToUpper")
  {
    CHECK(AsciiStrToUpper("hello") == "HELLO");
    CHECK(AsciiStrToUpper("Hello World") == "HELLO WORLD");
    CHECK(AsciiStrToUpper("ALREADY") == "ALREADY");
    CHECK(AsciiStrToUpper("123abc") == "123ABC");
    CHECK(AsciiStrToUpper("") == "");
  }

  TEST_CASE("StrSplit")
  {
    SUBCASE("basic split")
    {
      auto result = StrSplit("a,b,c", ',');
      REQUIRE(result.size() == 3);
      CHECK(result[0] == "a");
      CHECK(result[1] == "b");
      CHECK(result[2] == "c");
    }

    SUBCASE("strips trailing whitespace on last element")
    {
      auto result = StrSplit("a,b,c  ", ',');
      REQUIRE(result.size() == 3);
      CHECK(result[2] == "c");
    }

    SUBCASE("empty segments skipped")
    {
      auto result = StrSplit("a,,b", ',');
      REQUIRE(result.size() == 2);
      CHECK(result[0] == "a");
      CHECK(result[1] == "b");
    }

    SUBCASE("single element")
    {
      auto result = StrSplit("hello", ',');
      REQUIRE(result.size() == 1);
      CHECK(result[0] == "hello");
    }

    SUBCASE("empty string")
    {
      auto result = StrSplit("", ',');
      CHECK(result.empty());
    }

    SUBCASE("pipe delimiter")
    {
      auto result = StrSplit("SPACE|MOUSE1", '|');
      REQUIRE(result.size() == 2);
      CHECK(result[0] == "SPACE");
      CHECK(result[1] == "MOUSE1");
    }
  }
}

// ===========================================================================
// hotkey decisions
// ===========================================================================

TEST_SUITE("hotkey_decisions")
{
  TEST_CASE("Scopely shortcut initialization runs for Scopely mode or fallthrough")
  {
    CHECK_FALSE(should_call_original_initialize_actions(false, false));
    CHECK(should_call_original_initialize_actions(false, true));
    CHECK(should_call_original_initialize_actions(true, false));
    CHECK(should_call_original_initialize_actions(true, true));
  }

  TEST_CASE("per-frame fallthrough can allow original ScreenManager update")
  {
    CHECK_FALSE(should_call_original_screen_update(false, false));
    CHECK(should_call_original_screen_update(false, true));
    CHECK(should_call_original_screen_update(true, false));
    CHECK(should_call_original_screen_update(true, true));
  }

  TEST_CASE("Escape exit suppression only blocks Escape-triggered exit outside the double-tap window")
  {
    CHECK_FALSE(should_suppress_escape_exit(false, true, 500, -1));
    CHECK_FALSE(should_suppress_escape_exit(true, false, 500, -1));

    CHECK(should_suppress_escape_exit(true, true, 0, -1));
    CHECK(should_suppress_escape_exit(true, true, 500, -1));
    CHECK(should_suppress_escape_exit(true, true, 500, 750));

    CHECK_FALSE(should_suppress_escape_exit(true, true, 500, 500));
    CHECK_FALSE(should_suppress_escape_exit(true, true, 500, 250));
  }
}

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
    query.afterSeq = 0;
    query.kind = "event-a";
    query.limit = 1;
    query.includeDetails = false;

    const auto snapshot = store.snapshot(query);
    CHECK(snapshot.count == 3);
    CHECK(snapshot.matchedCount == 2);
    CHECK(snapshot.returnedCount == 1);
    CHECK(snapshot.queryGap == true);
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
    wildcard_query.kinds = {"toast-notification-observed", "top-canvas-changed"};
    wildcard_query.match = "*alliance*";
    wildcard_query.includeDetails = false;

    const auto wildcard_snapshot = store.snapshot(wildcard_query);
    CHECK(wildcard_snapshot.count == 4);
    CHECK(wildcard_snapshot.matchedCount == 2);
    CHECK(wildcard_snapshot.returnedCount == 2);
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
    REQUIRE(exact_snapshot.events.size() == 2);
    CHECK(exact_snapshot.events[0]["seq"] == 1);
    CHECK(exact_snapshot.events[1]["seq"] == 4);
  }
}

TEST_SUITE("live_debug_ui_serializers")
{
  TEST_CASE("top canvas serializer preserves visible metadata")
  {
    TopCanvasObservation observation;
    observation.found = true;
    observation.pointer = "0x1234";
    observation.className = "GalaxyScreen";
    observation.classNamespace = "Scopely.UI";
    observation.name = "GalaxyTopCanvas";
    observation.visible = true;
    observation.enabled = true;
    observation.internalVisible = false;
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
    observation.tracked = true;
    observation.pointer = "0x777";
    observation.hasContext = true;
    observation.targetType = 3;
    observation.targetFleetId = 42;
    observation.targetUserId = "player-1";
    observation.quickScanTargetFleetId = 99;
    observation.quickScanTargetId = "scan-9";

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
    observation.tracked = true;
    observation.trackedCount = 1;

    NavigationInteractionObservation::Entry entry;
    entry.pointer = "0xabc";
    entry.hasContext = true;
    entry.contextDataState = 1;
    entry.inputInteractionType = 14;
    entry.userId = "enemy-7";
    entry.isMarauder = true;
    entry.threatLevel = 0;
    entry.validNavigationInput = true;
    entry.showSetCourseArm = true;
    entry.locationTranslationId = 123456;
    entry.poiPointer = "0xpoi";
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
    CHECK(fleet_state_name_from_value(1541) == doctest::String("CanRecall"));
    CHECK(fleet_state_name_from_value(999999) == doctest::String("Unmapped"));
  }

  TEST_CASE("fleet observation serializer includes tracked fleet details")
  {
    FleetObservation observation;
    observation.tracked = true;
    observation.pointer = "0xfleetbar";
    observation.selectedIndex = 3;
    observation.hasController = true;
    observation.hasFleet = true;
    observation.fleetId = 44;
    observation.currentState = 2;
    observation.previousState = 1;
    observation.cargoFillPercent = 37;
    observation.cargoFillBasisPoints = 3700;
    observation.hullName = "Mayflower";

    const auto result = fleet_observation_to_json(observation);

    CHECK(result["tracked"] == true);
    CHECK(result["pointer"] == "0xfleetbar");
    CHECK(result["fleet"]["id"] == 44);
    CHECK(result["fleet"]["currentStateName"] == "Docked");
    CHECK(result["fleet"]["previousStateName"] == "IdleInSpace");
    CHECK(result["fleet"]["cargoFillBasisPoints"] == 3700);
    CHECK(result["fleet"]["hullName"] == "Mayflower");
  }

  TEST_CASE("fleet slot serializer preserves slot order and readable states")
  {
    std::array<FleetSlotObservation, kFleetIndexMax> observations{};
    observations[0].slotIndex = 0;
    observations[1].slotIndex = 1;
    observations[1].selected = true;
    observations[1].present = true;
    observations[1].fleetId = 9001;
    observations[1].currentState = 256;
    observations[1].previousState = 128;
    observations[1].cargoFillPercent = 82;
    observations[1].cargoFillBasisPoints = 8200;
    observations[1].hullName = "Enterprise";

    const auto result = fleet_slots_to_json(observations);

    REQUIRE(result.size() == kFleetIndexMax);
    CHECK(result[0]["slotIndex"] == 0);
    CHECK(result[1]["selected"] == true);
    CHECK(result[1]["fleetId"] == 9001);
    CHECK(result[1]["currentStateName"] == "Warping");
    CHECK(result[1]["previousStateName"] == "WarpCharging");
    CHECK(result[1]["hullName"] == "Enterprise");
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
    observation.preScanTargetTracked = true;
    observation.preScanTargetPointer = "0xpre";
    observation.preScanStationTargetTracked = false;
    observation.celestialViewerTracked = true;
    observation.celestialViewerPointer = "0xcelestial";

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
    observation.miningViewerTracked = true;
    observation.miningPointer = "0xmine";
    observation.enabled = true;
    observation.isActiveAndEnabled = true;
    observation.isInfoShown = true;
    observation.hasParent = true;
    observation.parentIsShowing = false;
    observation.occupiedState = 2;
    observation.hasScanEngageButtons = true;
    observation.hasTimer = true;
    observation.timerState = 4;
    observation.timerType = 8;
    observation.timerRemainingSeconds = 75;
    observation.timerRemainingBucket = 60;
    observation.starNodeViewerTracked = true;
    observation.starNodePointer = "0xstar";
    observation.starNodeEnabled = false;
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

TEST_SUITE("hotkey_disable_shortcut_alias")
{
  TEST_CASE("canonical key wins")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_canonical = true;
    input.canonical = "CTRL-D";
    input.default_value = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.key == "set_hotkeys_disable");
    CHECK(decision.source_key == "set_hotkeys_disable");
    CHECK(decision.value == "CTRL-D");
    CHECK_FALSE(decision.used_deprecated_alias);
    CHECK_FALSE(decision.has_conflicting_alias);
  }

  TEST_CASE("deprecated typo remains compatible")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_deprecated_typo = true;
    input.deprecated_typo = "CTRL-T";
    input.default_value = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.key == "set_hotkeys_disable");
    CHECK(decision.source_key == "set_hotkeys_disble");
    CHECK(decision.value == "CTRL-T");
    CHECK(decision.used_deprecated_alias);
    CHECK(decision.saw_deprecated_alias);
    CHECK_FALSE(decision.has_conflicting_alias);
  }

  TEST_CASE("conflicting canonical and deprecated values use canonical")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_canonical = true;
    input.canonical = "CTRL-D";
    input.has_deprecated_typo = true;
    input.deprecated_typo = "CTRL-T";
    input.default_value = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.source_key == "set_hotkeys_disable");
    CHECK(decision.value == "CTRL-D");
    CHECK_FALSE(decision.used_deprecated_alias);
    CHECK(decision.saw_deprecated_alias);
    CHECK(decision.has_conflicting_alias);
  }

  TEST_CASE("legacy disabled spelling is accepted as an alias")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_legacy_disabled = true;
    input.legacy_disabled = "CTRL-L";
    input.default_value = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.source_key == "set_hotkeys_disabled");
    CHECK(decision.value == "CTRL-L");
    CHECK(decision.used_deprecated_alias);
  }
}

// ===========================================================================
// bounded TTL deduper
// ===========================================================================

TEST_SUITE("bounded_ttl_deduper")
{
  using TestDeduper = BoundedTtlDeduper<std::string>;

  TEST_CASE("suppresses repeated key inside TTL and emits at boundary")
  {
    auto at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("toast-1", at_ms(0), ttl).emitted);

    const auto duplicate = deduper.should_emit("toast-1", at_ms(99), ttl);
    CHECK_FALSE(duplicate.emitted);
    CHECK(duplicate.suppressed_by_window);

    const auto boundary = deduper.should_emit("toast-1", at_ms(100), ttl);
    CHECK(boundary.emitted);
    CHECK_FALSE(boundary.suppressed_by_window);
  }

  TEST_CASE("key replacement refreshes timestamp after expiry")
  {
    auto at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("incoming", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("incoming", at_ms(100), ttl).emitted);
    CHECK_FALSE(deduper.should_emit("incoming", at_ms(199), ttl).emitted);
    CHECK(deduper.should_emit("incoming", at_ms(200), ttl).emitted);
    CHECK(deduper.size() == 1);
  }

  TEST_CASE("prune removes expired entries without removing live entries")
  {
    auto at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("old", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("live", at_ms(50), ttl).emitted);

    CHECK(deduper.prune(at_ms(100)) == 1);
    CHECK_FALSE(deduper.contains("old"));
    CHECK(deduper.contains("live"));
  }

  TEST_CASE("max-entry eviction removes the oldest cached key")
  {
    auto at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(2);
    const auto ttl = std::chrono::seconds(10);

    CHECK(deduper.should_emit("first", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("second", at_ms(1), ttl).emitted);

    const auto result = deduper.should_emit("third", at_ms(2), ttl);
    CHECK(result.emitted);
    CHECK(result.evicted_oldest);
    CHECK(result.cache_size == 2);
    CHECK_FALSE(deduper.contains("first"));
    CHECK(deduper.contains("second"));
    CHECK(deduper.contains("third"));
  }
}

// ===========================================================================
// notification text and queue helpers
// ===========================================================================

TEST_SUITE("notification_text")
{
  TEST_CASE("normalizes line feeds for Windows notification XML")
  {
    CHECK(notification_normalize_body("alpha\nbeta\r\ngamma\rdone") == "alpha\r\nbeta\r\ngamma\rdone");
    CHECK(notification_normalize_body(nullptr).empty());
    CHECK(notification_normalize_body("").empty());
  }

  TEST_CASE("flattens whitespace for queue summaries")
  {
    CHECK(notification_flatten_text("  alpha\n\tbeta  gamma \r\n") == "alpha beta gamma");
  }

  TEST_CASE("escapes control whitespace for logs")
  {
    CHECK(notification_escape_text_for_log("a\r\nb\tc") == "a\\r\\nb\\tc");
  }

  TEST_CASE("strips Unity rich text tags")
  {
    CHECK(notification_strip_unity_rich_text("<color=#fff>Alpha</color> <b>Beta</b>") == "Alpha Beta");
  }

  TEST_CASE("chooses parsed body before localized fallbacks")
  {
    CHECK(notification_choose_body("parsed", "formatted", "raw") == "parsed");
    CHECK(notification_choose_body("", "formatted", "raw") == "formatted");
    CHECK(notification_choose_body("", "", "<b>raw</b>") == "raw");
    CHECK(notification_choose_body("", "", "", "fallback") == "fallback");
  }
}

TEST_SUITE("notification_queue")
{
  TEST_CASE("collapses same-title batches into a counted summary")
  {
    std::vector<NotificationQueueRequest> batch{
      {.source = "toast", .title = "Fleet Battle", .body = "one"},
      {.source = "toast", .title = "Fleet Battle", .body = "two"},
      {.source = "toast", .title = "Fleet Battle", .body = "three"},
    };

    auto collapsed = notification_queue_collapse_batch(std::move(batch), 2);
    CHECK(collapsed.title == "Fleet Battle (3)");
    CHECK(collapsed.body == "one\ntwo\n+1 more");
  }

  TEST_CASE("collapses mixed-title batches with title prefixes")
  {
    std::vector<NotificationQueueRequest> batch{
      {.source = "toast", .title = "Victory!", .body = "alpha"},
      {.source = "direct", .title = "Fleet Docked", .body = "beta"},
    };

    auto collapsed = notification_queue_collapse_batch(std::move(batch), 4);
    CHECK(collapsed.title == "2 Notifications");
    CHECK(collapsed.body == "Victory!: alpha\nFleet Docked: beta");
  }

  TEST_CASE("builds deterministic batch preview with limit")
  {
    std::vector<NotificationQueueRequest> batch{
      {.source = "toast", .title = "Victory!"},
      {.source = "direct", .title = ""},
      {.source = "toast", .title = "Defeat"},
    };

    CHECK(notification_queue_batch_preview(batch, 2) == "toast:Victory!, direct:(untitled), +1 more");
  }
}

// ===========================================================================
// incoming attack policy
// ===========================================================================

TEST_SUITE("incoming_attack_policy")
{
  TEST_CASE("attacker fleet type maps to player hostile or unknown")
  {
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(1) == IncomingAttackPolicyAttackerKind::Player);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(2) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(3) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(4) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(6) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(0) == IncomingAttackPolicyAttackerKind::Unknown);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(5) == IncomingAttackPolicyAttackerKind::Unknown);
  }

  TEST_CASE("titles and fleet copy specialize by attacker kind")
  {
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Hostile))
          == "Incoming Hostile Attack");
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Player))
          == "Incoming Player Attack");
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Unknown))
          == "Incoming Attack!");

    CHECK(incoming_attack_policy_fleet_body("K'VORT", IncomingAttackPolicyAttackerKind::Hostile)
          == "Your K'VORT is being chased.");
    CHECK(incoming_attack_policy_fleet_body("K'VORT", IncomingAttackPolicyAttackerKind::Player)
          == "Your K'VORT is under attack by another player.");
    CHECK(incoming_attack_policy_fleet_body("", IncomingAttackPolicyAttackerKind::Unknown)
          == "Your fleet is under attack.");
  }

  TEST_CASE("station copy specializes by attacker kind")
  {
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Hostile)
          == "Your station is under attack by a hostile.");
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Player)
          == "Your station is under attack by another player.");
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Unknown)
          == "Your station is under attack.");
  }

  TEST_CASE("dedupe key captures station fleet and global target identity")
  {
    const auto station = incoming_attack_policy_dedupe_key(123, 3, IncomingAttackPolicyAttackerKind::Player, "attacker");
    CHECK(station.target_kind == IncomingAttackPolicyTargetKind::Station);
    CHECK(station.target_id == 0);
    CHECK(station.attacker_identity == "attacker");

    const auto fleet = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Hostile, "");
    CHECK(fleet.target_kind == IncomingAttackPolicyTargetKind::Fleet);
    CHECK(fleet.target_id == 123);

    const auto global = incoming_attack_policy_dedupe_key(0, 0, IncomingAttackPolicyAttackerKind::Unknown, "");
    CHECK(global.target_kind == IncomingAttackPolicyTargetKind::Global);
    CHECK(global.target_id == 0);
  }

  TEST_CASE("target type names remain stable")
  {
    CHECK(std::string(incoming_attack_policy_target_type_name(0)) == "None");
    CHECK(std::string(incoming_attack_policy_target_type_name(1)) == "Fleet");
    CHECK(std::string(incoming_attack_policy_target_type_name(2)) == "DockingPoint");
    CHECK(std::string(incoming_attack_policy_target_type_name(3)) == "Station");
    CHECK(std::string(incoming_attack_policy_target_type_name(99)) == "Unknown");
  }

  TEST_CASE("incoming attack toast states are consumed separately from visible notification policy")
  {
    CHECK(incoming_attack_policy_consumes_toast_state(5));
    CHECK(incoming_attack_policy_consumes_toast_state(6));
    CHECK_FALSE(incoming_attack_policy_consumes_toast_state(17));
    CHECK_FALSE(incoming_attack_policy_consumes_toast_state(10));
  }

  TEST_CASE("generic dedupe suppresses inside TTL and emits at window boundary")
  {
    IncomingAttackPolicyDeduper deduper;
    const auto key = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Hostile, "");

    auto first = deduper.should_emit(key, 100);
    CHECK(first.emitted);
    CHECK_FALSE(first.suppressed_by_window);

    auto duplicate = deduper.should_emit(key, 109);
    CHECK_FALSE(duplicate.emitted);
    CHECK(duplicate.suppressed_by_window);

    auto boundary = deduper.should_emit(key, 110);
    CHECK(boundary.emitted);
    CHECK_FALSE(boundary.suppressed_by_window);
  }

  TEST_CASE("identified dedupe uses longer TTL")
  {
    IncomingAttackPolicyDeduper deduper;
    const auto key = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Player, "player-1");
    CHECK(incoming_attack_policy_dedupe_window_seconds(key) == 120);

    CHECK(deduper.should_emit(key, 100).emitted);
    CHECK_FALSE(deduper.should_emit(key, 219).emitted);
    CHECK(deduper.should_emit(key, 220).emitted);
  }

  TEST_CASE("dedupe evicts oldest entry when max size is exceeded")
  {
    IncomingAttackPolicyDeduper deduper(2);
    const auto first = incoming_attack_policy_dedupe_key(1, 1, IncomingAttackPolicyAttackerKind::Hostile, "a");
    const auto second = incoming_attack_policy_dedupe_key(2, 1, IncomingAttackPolicyAttackerKind::Hostile, "b");
    const auto third = incoming_attack_policy_dedupe_key(3, 1, IncomingAttackPolicyAttackerKind::Hostile, "c");

    CHECK(deduper.should_emit(first, 100).emitted);
    CHECK(deduper.should_emit(second, 101).emitted);
    const auto result = deduper.should_emit(third, 102);
    CHECK(result.emitted);
    CHECK(result.evicted_oldest);
    CHECK(result.cache_size == 2);
    CHECK_FALSE(deduper.contains(first));
    CHECK(deduper.contains(second));
    CHECK(deduper.contains(third));
  }
}

// ===========================================================================
// toast_state_title
// ===========================================================================

TEST_SUITE("toast_state_title")
{
  TEST_CASE("known states return correct titles")
  {
    CHECK(std::string(toast_state_title(10)) == "Victory!");
    CHECK(std::string(toast_state_title(11)) == "Defeat");
    CHECK(std::string(toast_state_title(37)) == "Partial Victory");
    CHECK(std::string(toast_state_title(9)) == "Station Victory!");
    CHECK(std::string(toast_state_title(12)) == "Station Defeat");
    CHECK(std::string(toast_state_title(8)) == "Station Under Attack!");
    CHECK(std::string(toast_state_title(5)) == "Incoming Attack!");
    CHECK(std::string(toast_state_title(7)) == "Fleet Battle");
    CHECK(std::string(toast_state_title(18)) == "Armada Victory!");
    CHECK(std::string(toast_state_title(19)) == "Armada Defeated");
    CHECK(std::string(toast_state_title(15)) == "Armada Created");
    CHECK(std::string(toast_state_title(16)) == "Armada Canceled");
    CHECK(std::string(toast_state_title(28)) == "Achievement");
    CHECK(std::string(toast_state_title(29)) == "Assault Victory!");
    CHECK(std::string(toast_state_title(30)) == "Assault Defeat");
    CHECK(std::string(toast_state_title(40)) == "Fleet Preset Applied");
  }

  TEST_CASE("unknown state returns nullptr")
  {
    CHECK(toast_state_title(999) == nullptr);
    CHECK(toast_state_title(-1) == nullptr);
    CHECK(toast_state_title(13) == nullptr); // gap in enum (13 is unused)
  }
}

TEST_SUITE("toast_state_uses_battle_summary")
{
  TEST_CASE("battle-result toasts use battle-summary parsing")
  {
    CHECK(toast_state_uses_battle_summary(10));
    CHECK(toast_state_uses_battle_summary(11));
    CHECK(toast_state_uses_battle_summary(8));
    CHECK(toast_state_uses_battle_summary(18));
    CHECK(toast_state_uses_battle_summary(29));
  }

  TEST_CASE("incoming-attack toasts do not use battle-summary parsing")
  {
    CHECK_FALSE(toast_state_uses_battle_summary(5));
    CHECK_FALSE(toast_state_uses_battle_summary(6));
    CHECK_FALSE(toast_state_uses_battle_summary(17));
    CHECK_FALSE(toast_state_uses_battle_summary(0));
  }
}

// ===========================================================================
// strip_unity_rich_text
// ===========================================================================

TEST_SUITE("strip_unity_rich_text")
{
  TEST_CASE("removes color tags")
  {
    CHECK(strip_unity_rich_text("<color=#FF0000>Red Text</color>") == "Red Text");
  }

  TEST_CASE("removes bold/italic tags")
  {
    CHECK(strip_unity_rich_text("<b>Bold</b> and <i>Italic</i>") == "Bold and Italic");
  }

  TEST_CASE("removes size tags")
  {
    CHECK(strip_unity_rich_text("<size=20>Big</size>") == "Big");
  }

  TEST_CASE("handles nested tags")
  {
    CHECK(strip_unity_rich_text("<color=#FFF><b>Hello</b></color>") == "Hello");
  }

  TEST_CASE("preserves plain text")
  {
    CHECK(strip_unity_rich_text("Hello World") == "Hello World");
  }

  TEST_CASE("empty string")
  {
    CHECK(strip_unity_rich_text("") == "");
  }

  TEST_CASE("unclosed angle bracket kept")
  {
    CHECK(strip_unity_rich_text("5 < 10 but no closing") == "5 < 10 but no closing");
  }
}

// ===========================================================================
// parse_hull_key
// ===========================================================================

TEST_SUITE("parse_hull_key")
{
  TEST_CASE("full hull key with LIVE suffix")
  {
    CHECK(parse_hull_key("Hull_L30_Destroyer_Klingon_LIVE") == "Lv.30 Destroyer Klingon");
  }

  TEST_CASE("hull key without LIVE suffix")
  {
    CHECK(parse_hull_key("Hull_L45_Battleship_Federation") == "Lv.45 Battleship Federation");
  }

  TEST_CASE("hull key without level prefix")
  {
    CHECK(parse_hull_key("Hull_Jellyfish_LIVE") == "Jellyfish");
  }

  TEST_CASE("minimal key")
  {
    CHECK(parse_hull_key("Hull_L1_Scout") == "Lv.1 Scout");
  }

  TEST_CASE("empty string")
  {
    CHECK(parse_hull_key("") == "");
  }

  TEST_CASE("no Hull_ prefix")
  {
    // Still processes underscores and level
    CHECK(parse_hull_key("L10_Frigate") == "Lv.10 Frigate");
  }
}

// ===========================================================================
// fleet notification formatting
// ===========================================================================

TEST_SUITE("fleet_notification_formatting")
{
  TEST_CASE("duration formatting keeps short ETA readable")
  {
    CHECK(format_duration_short(0) == "");
    CHECK(format_duration_short(59) == "59s");
    CHECK(format_duration_short(96) == "1m 36s");
    CHECK(format_duration_short(3600) == "1h");
    CHECK(format_duration_short(3660) == "1h 1m");
  }

  TEST_CASE("cargo formatting clamps and rounds percentage")
  {
    CHECK(format_cargo_fill_text(-1.0f) == "");
    CHECK(format_cargo_fill_text(0.0f) == "Current Cargo: 0%");
    CHECK(format_cargo_fill_text(0.126f) == "Current Cargo: 13%");
    CHECK(format_cargo_fill_text(1.4f) == "Current Cargo: 100%");
  }

  TEST_CASE("started mining title and body use stacked layout")
  {
    CHECK(format_started_mining_title("K'VORT", "Parsteel") == "K'VORT started mining Parsteel");
    CHECK(format_started_mining_body("1m 36s", "Current Cargo: 0%")
          == "ETA 1m 36s\nCurrent Cargo: 0%");
  }

  TEST_CASE("started mining title and body omit optional details cleanly")
  {
    CHECK(format_started_mining_title("K'VORT", "") == "K'VORT started mining");
    CHECK(format_started_mining_body("", "") == "");
    CHECK(format_started_mining_title("", "Parsteel") == "Fleet started mining Parsteel");
    CHECK(format_started_mining_body("", "Current Cargo: 55%") == "Current Cargo: 55%");
  }

  TEST_CASE("node depleted body keeps resource and cargo context")
  {
    CHECK(format_node_depleted_body("K'VORT", "Parsteel", "Current Cargo: 100%")
          == "K'VORT depleted its Parsteel node. Current Cargo: 100%.");
    CHECK(format_node_depleted_body("?", "", "") == "Fleet depleted its node.");
  }
}

// ===========================================================================
// BattleSummaryPreview::format_body
// ===========================================================================

TEST_SUITE("BattleSummaryPreview")
{
  TEST_CASE("full battle with both sides")
  {
    BattleSummaryPreview d{"Player1", "Enemy1", "Lv.30 Destroyer", "Lv.25 Frigate"};
    CHECK(d.format_body() == "Player1 (Lv.30 Destroyer) vs Enemy1 (Lv.25 Frigate)");
  }

  TEST_CASE("names only, no ships")
  {
    BattleSummaryPreview d{"Player1", "Enemy1", "", ""};
    CHECK(d.format_body() == "Player1 vs Enemy1");
  }

  TEST_CASE("player only")
  {
    BattleSummaryPreview d{"Player1", "", "Lv.30 Destroyer", ""};
    CHECK(d.format_body() == "Player1 (Lv.30 Destroyer)");
  }

  TEST_CASE("enemy only")
  {
    BattleSummaryPreview d{"", "Enemy1", "", "Lv.25 Frigate"};
    CHECK(d.format_body() == "Enemy1 (Lv.25 Frigate)");
  }

  TEST_CASE("all empty")
  {
    BattleSummaryPreview d{"", "", "", ""};
    CHECK(d.format_body() == "");
  }

  TEST_CASE("name with ship on one side, name only on other")
  {
    BattleSummaryPreview d{"Player1", "Enemy1", "Lv.30 Destroyer", ""};
    CHECK(d.format_body() == "Player1 (Lv.30 Destroyer) vs Enemy1");
  }
}
