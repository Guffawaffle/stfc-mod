#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bounded_ttl_cache.h"
#include "patches/async_work_queue.h"
#include "patches/battle_log_decoder.h"
#include "patches/live_debug_event_store.h"
#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_recent_event_requests.h"
#include "patches/live_debug_ui_serializers.h"
#include "patches/live_debug_viewer_serializers.h"
#include "patches/notification_queue.h"
#include "patches/notification_text.h"
#include "patches/object_tracker_core.h"
#include "str_utils_pure.h"
#include "testable_functions.h"

#include <chrono>
#include <utility>

// ===========================================================================
// battle_log_decoder
// ===========================================================================

TEST_SUITE("battle_log_decoder")
{
  TEST_CASE("decode_probe_entry maps segment ids to ships and components")
  {
    auto probe = nlohmann::json::object();
    probe["journal_id"] = 12345;
    probe["names"] = nlohmann::json{{"player-1", {{"name", "Guff"}}}};

    auto journal = nlohmann::json::object();
    journal["id"] = 12345;
    journal["battle_type"] = 8;
    journal["battle_time"] = "2026-04-26T23:04:17";
    journal["initiator_id"] = "player-1";
    journal["target_id"] = "mar_45";
    journal["initiator_wins"] = true;
    journal["battle_log"] = nlohmann::json::array({-96, -90, -88, 111, -86, 10, 20, 0.5, -85, 222, -98, 900, 1, -99, -89,
                                                    -90, -88, 0, -87, -84, -83, -89, -97});
    journal["resources_transferred"] = {{"2431852293", 263028}};
    journal["chest_drop"] = {{"loot_roll_key", "MAR_45_Armada_Car_Uncommon"},
                 {"chests_gained",
                  nlohmann::json::array({{{"count", 1},
                               {"params", {{"ref_id", 129255444149608},
                                    {"chest_name", "MAR_45_Armada_Car_Uncommon"}}}}})}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", 1},
        {"ship_ids", nlohmann::json::array({111})},
        {"hull_ids", nlohmann::json::array({77})},
        {"offense_rating", 1000.0},
        {"ship_components", {{"111", nlohmann::json::array({900, 901})}}},
    };
    journal["target_fleet_data"]["deployed_fleet"] = {
        {"uid", "mar_45"},
        {"fleet_id", 2},
        {"type", 2},
        {"ship_ids", nlohmann::json::array({0})},
        {"hull_ids", nlohmann::json::array({3066099110})},
        {"ship_levels", {{"0", 45}}},
        {"ship_components", {{"0", nlohmann::json::array({800})}}},
    };
    probe["journal"] = journal;

    battle_log_decoder::DecodeOptions options;
    options.include_segments = true;

    const auto decoded = battle_log_decoder::decode_probe_entry(probe, options);
    REQUIRE(decoded.value("ok", false));
    CHECK(decoded["journal_id"] == 12345);
    CHECK(decoded["battle_type"] == 8);
    CHECK(decoded["participant_count"] == 2);
    CHECK(decoded["ship_count"] == 2);
    CHECK(decoded["component_count"] == 3);
    CHECK(decoded["signature"]["first_token"] == -96);
    CHECK(decoded["signature"]["last_token"] == -97);
    CHECK(decoded["signature"]["segment_count"] == 2);
    REQUIRE(decoded["segments"].size() == 3);
    CHECK(decoded["segments"][0]["ship_ids"] == nlohmann::json::array({111}));
    CHECK(decoded["segments"][0]["component_refs"][0]["component_id"] == 900);
    CHECK(decoded["segments"][0]["component_refs"][0]["ship_id"] == 111);
    CHECK(decoded["segments"][1]["ship_ids"] == nlohmann::json::array({0}));
    CHECK(decoded["segments"][2]["terminated"] == false);
    CHECK(decoded["participants"][1]["participant_kind"] == "hostile");
    CHECK(decoded["participants"][1]["display_name"] == "Central Command Station");
    CHECK(decoded["participants"][1]["display_name_source"] == "derived_hostile_key");
    CHECK(decoded["participants"][1]["ship_level"] == 45);

    const auto report = battle_log_decoder::build_sidecar_battle_report_event(journal, decoded, 12345, 111);
    CHECK(report["protocolVersion"] == "stfc.sidecar.events.v0");
    CHECK(report["type"] == "battle.report");
    CHECK(report["schemaVersion"] == "stfc.sidecar.battle-report.v0");
    CHECK(report["journalId"] == "12345");
    CHECK(report["capturedAtUnixMs"] == 111);
    CHECK(report["report"]["summary"]["outcome"] == "initiator_victory");
    CHECK(report["report"]["parity"]["sections"]["battleEvents"] == "decoded_segments");
    CHECK(report["report"]["rewards"].size() == 2);
    CHECK(report["report"]["events"].size() == 3);
  }

  TEST_CASE("build_sidecar_battle_capture_event emits string tokens and lossless journal integers")
  {
    auto journal = nlohmann::json::object();
    journal["id"] = 2709118446356718841LL;
    journal["battle_type"] = 8;
    journal["battle_time"] = "2026-04-26T23:04:17";
    journal["initiator_id"] = "player-1";
    journal["target_id"] = "mar_45";
    journal["initiator_wins"] = true;
    journal["system_id"] = 2682660367670527124LL;
    journal["battle_log"] = nlohmann::json::array({-96, 2682660367670527124LL, -97});

    const auto names = nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_id", 2682660367670527124LL}}}};
    const auto capture = battle_log_decoder::build_sidecar_battle_capture_event(journal, names, 0, 222);

    CHECK(capture["protocolVersion"] == "stfc.sidecar.events.v0");
    CHECK(capture["type"] == "battle.capture");
    CHECK(capture["schemaVersion"] == "stfc.battle.capture.v1");
    CHECK(capture["journalId"] == "2709118446356718841");
    CHECK(capture["capturedAtUnixMs"] == 222);
    CHECK(capture["capture"]["sourceKind"] == "scopely.journal.battle");
    CHECK(capture["capture"]["battleLog"]["encoding"] == "string_tokens.v1");
    CHECK(capture["capture"]["battleLog"]["tokenCount"] == 3);
    CHECK(capture["capture"]["battleLog"]["tokens"][1] == "2682660367670527124");
    CHECK(capture["capture"]["names"]["player-1"]["alliance_id"] == "2682660367670527124");
    CHECK(capture["capture"]["journal"]["encoding"] == "lossless_integer_strings.v1");
    CHECK(capture["capture"]["journal"]["data"]["id"] == "2709118446356718841");
    CHECK(capture["capture"]["journal"]["data"]["system_id"] == "2682660367670527124");
    CHECK_FALSE(capture["capture"]["journal"]["data"].contains("battle_log"));
  }

  TEST_CASE("hostile display names ignore retrieving placeholders and derive from reward keys")
  {
    const auto names = nlohmann::json{{"mar_42", {{"name", "Retrieving..."}}}};
    auto       journal = nlohmann::json{{"id", 222},
                                        {"battle_type", 2},
                                        {"target_id", "mar_42"},
                                        {"battle_log", nlohmann::json::array({-96, -89, -97})},
                                        {"chest_drop", {{"loot_roll_key", "MAR_42_Battleship_ElAurian_Core"}}}};
    journal["target_fleet_data"]["deployed_fleet"] = {
        {"uid", "mar_42"},
        {"fleet_id", 7},
        {"type", 2},
        {"ship_ids", nlohmann::json::array({0})},
        {"hull_ids", nlohmann::json::array({1686254040})},
        {"ship_levels", {{"0", 42}}},
    };

    const auto decoded = battle_log_decoder::decode_journal(journal, names);

    REQUIRE(decoded.value("ok", false));
    REQUIRE(decoded["participants"].size() == 1);
    CHECK(decoded["participants"][0]["name"] == "");
    CHECK(decoded["participants"][0]["participant_kind"] == "hostile");
    CHECK(decoded["participants"][0]["display_name"] == "Lv.42 Battleship ElAurian Core");
    CHECK(decoded["participants"][0]["display_name_source"] == "derived_hostile_key");
  }

  TEST_CASE("battle type filter can skip journals before decode")
  {
    const auto journal = nlohmann::json{{"battle_type", 5}, {"battle_log", nlohmann::json::array({-96, -97})}};

    battle_log_decoder::DecodeOptions options;
    options.battle_type_filter = {2, 8};

    CHECK_FALSE(battle_log_decoder::journal_matches_options(journal, options));
    const auto decoded = battle_log_decoder::decode_journal(journal, nlohmann::json::object(), options);
    CHECK_FALSE(decoded.value("ok", true));
  }

  TEST_CASE("decode_journal derives rounds sub-rounds and attack rows from record markers")
  {
    const auto names = nlohmann::json{{"player-1", {{"name", "Guff"}}}};
    auto       journal = nlohmann::json{{"id", 333},
                                        {"battle_type", 8},
                                        {"battle_time", "2026-04-27T01:23:45"},
                                        {"initiator_id", "player-1"},
                                        {"target_id", "mar_45"},
                                        {"initiator_wins", true},
                                        {"battle_log",
                                         nlohmann::json::array({-96,
                                                                -90,
                                                                -88,
                                                                111,
                                                                -86,
                                                                10,
                                                                20,
                                                                0.5,
                                                                -85,
                                                                -83,
                                                                0,
                                                                -98,
                                                                800,
                                                                111,
                                                                1.0,
                                                                0.0,
                                                                1,
                                                                1,
                                                                1878,
                                                                83380359.0,
                                                                7514,
                                                                30103950.0,
                                                                7636.33728,
                                                                5405,
                                                                0.0,
                                                                0.0,
                                                                -99,
                                                                -89,
                                                                -97,
                                                                -96,
                                                                111,
                                                                -98,
                                                                900,
                                                                0,
                                                                1.0,
                                                                0.0,
                                                                1,
                                                                0,
                                                                2501,
                                                                80608653.0,
                                                                10002,
                                                                19017128.0,
                                                                10166.36544,
                                                                7195,
                                                                0.3728,
                                                                0.0,
                                                                -93,
                                                                111,
                                                                -91,
                                                                1449938138,
                                                                2481912459,
                                                                0.02,
                                                                -92,
                                                                -94,
                                                                -99,
                                                                111,
                                                                -98,
                                                                901,
                                                                -95,
                                                                1.0,
                                                                -99,
                                                                -89,
                                                                -97})},
                                        {"chest_drop", {{"loot_roll_key", "MAR_45_Armada_Car_Uncommon"}}}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", 1},
        {"ship_ids", nlohmann::json::array({111})},
        {"hull_ids", nlohmann::json::array({77})},
        {"ship_components", {{"111", nlohmann::json::array({900, 901})}}},
    };
    journal["target_fleet_data"]["deployed_fleet"] = {
        {"uid", "mar_45"},
        {"fleet_id", 2},
        {"type", 2},
        {"ship_ids", nlohmann::json::array({0})},
        {"hull_ids", nlohmann::json::array({3066099110})},
        {"ship_levels", {{"0", 45}}},
        {"ship_components", {{"0", nlohmann::json::array({800})}}},
    };

    battle_log_decoder::DecodeOptions options;
    options.include_segments = true;

    const auto decoded = battle_log_decoder::decode_journal(journal, names, options);
    REQUIRE(decoded.value("ok", false));
    REQUIRE(decoded["segments"].is_array());
    REQUIRE(decoded["rounds"].is_array());
    REQUIRE(decoded["attack_rows"].is_array());
    REQUIRE(decoded["segments"].size() == 3);
    REQUIRE(decoded["rounds"].size() == 2);
    REQUIRE(decoded["attack_rows"].size() == 2);

    CHECK(decoded["segments"][0]["round"] == 1);
    CHECK(decoded["segments"][0]["subRound"] == 1);
    CHECK(decoded["segments"][0]["summary"]["attackCount"] == 1);
    CHECK(decoded["segments"][0]["summary"]["criticalCount"] == 1);
    CHECK(decoded["segments"][1]["round"] == 2);
    CHECK(decoded["segments"][1]["subRound"] == 1);
    CHECK(decoded["segments"][1]["summary"]["attackCount"] == 1);
    CHECK(decoded["segments"][1]["summary"]["componentScalarCount"] == 1);
    CHECK(decoded["segments"][2]["recordCount"] == 0);

    CHECK(decoded["attack_rows"][0]["attackerShipId"] == 0);
    CHECK(decoded["attack_rows"][0]["targetShipId"] == 111);
    CHECK(decoded["attack_rows"][0]["critical"] == true);
    CHECK(decoded["attack_rows"][0]["damage"]["hull"] == 1878);
    CHECK(decoded["attack_rows"][1]["attackerShipId"] == 111);
    CHECK(decoded["attack_rows"][1]["targetShipId"] == 0);
    CHECK(decoded["attack_rows"][1]["triggeredEffectCount"] == 1);
    CHECK(decoded["attack_rows"][1]["triggeredEffects"][0]["value"] == doctest::Approx(0.02));

    const auto report = battle_log_decoder::build_sidecar_battle_report_event(journal, decoded, 333, 222);
    CHECK(report["report"]["summary"]["roundCount"] == 2);
    CHECK(report["report"]["summary"]["attackRowCount"] == 2);
    CHECK(report["report"]["decode"]["status"] == "decoded_segments_with_attack_rows");
    CHECK(report["report"]["parity"]["sections"]["battleEvents"] == "partial");
    CHECK(report["report"]["rounds"][1]["summary"]["componentScalarCount"] == 1);
    CHECK(report["report"]["attackRows"][1]["round"] == 2);
    CHECK(report["report"]["attackRows"][1]["subRound"] == 1);
  }
}

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

  TEST_CASE("startup router gates hotkey toggles and Scopely fallthrough")
  {
    CHECK(hotkey_router_startup_action(true, false, false, true) == HotkeyRouterStartupAction::DisableHotkeys);
    CHECK(hotkey_router_startup_action(false, true, false, false) == HotkeyRouterStartupAction::EnableHotkeys);
    CHECK(hotkey_router_startup_action(false, false, true, true) == HotkeyRouterStartupAction::AllowOriginal);
    CHECK(hotkey_router_startup_action(false, false, false, false) == HotkeyRouterStartupAction::SuppressOriginal);
    CHECK(hotkey_router_startup_action(false, false, false, true) == HotkeyRouterStartupAction::Continue);
  }

  TEST_CASE("ship selection returns the first active fleet hotkey")
  {
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{}) == -1);
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{false, false, true, false, false, false, false, false}) == 2);
    CHECK(hotkey_router_ship_select_request(std::array<bool, 8>{true, false, true, false, false, false, false, false}) == 0);
  }

  TEST_CASE("escape clears focused chat or input without falling through")
  {
    CHECK(hotkey_router_should_clear_input_focus(true, true, false));
    CHECK(hotkey_router_should_clear_input_focus(true, false, true));
    CHECK_FALSE(hotkey_router_should_clear_input_focus(true, false, false));
    CHECK_FALSE(hotkey_router_should_clear_input_focus(false, true, true));
  }

  TEST_CASE("queue toggle is only a gameplay-surface action")
  {
    CHECK(hotkey_router_should_toggle_queue(false, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(true, false, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(false, true, true));
    CHECK_FALSE(hotkey_router_should_toggle_queue(false, false, false));
  }

  TEST_CASE("dispatch decisions preserve explicit action fallthrough")
  {
    CHECK(hotkey_router_dispatch_action(false, false, false) == HotkeyRouterDispatchAction::Continue);
    CHECK(hotkey_router_dispatch_action(true, true, false) == HotkeyRouterDispatchAction::SuppressOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, true) == HotkeyRouterDispatchAction::AllowOriginal);
    CHECK(hotkey_router_dispatch_action(true, false, false) == HotkeyRouterDispatchAction::Continue);
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
        {"limit", 7},
        {"last", 2},
        {"includeDetails", true},
        {"summary", true},
        {"kind", "fleet-slot-state-changed"},
    };

    const auto query = live_debug_recent_events_query_from_request(request);
    CHECK(query.limit == 7);
    CHECK(query.kind == "fleet-slot-state-changed");
    CHECK(query.includeDetails == true);
  }

  TEST_CASE("recent-events result builder preserves metadata and nulls empty seq values")
  {
    LiveDebugRecentEventStoreSnapshot snapshot;
    snapshot.count = 4;
    snapshot.returnedCount = 2;
    snapshot.matchedCount = 3;
    snapshot.capacity = 256;
    snapshot.nextSeq = 12;
    snapshot.evictedCount = 5;
    snapshot.clearCount = 1;
    snapshot.queryGap = true;
    snapshot.missingCountBeforeFirstReturned = 2;
    snapshot.kindCounts = nlohmann::json{{"toast-notification-observed", 2}};
    snapshot.bufferKindCounts = nlohmann::json{{"toast-notification-observed", 3}};
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
    CHECK(result["queryGap"] == true);
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

TEST_SUITE("async_work_queue")
{
  TEST_CASE("tracks enqueue drain and depth diagnostics")
  {
    AsyncWorkQueue<int> queue;

    CHECK(queue.enqueue(10));
    CHECK(queue.enqueue(20));

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 2);
    CHECK(diagnostics.enqueued == 2);
    CHECK(diagnostics.dequeued == 0);

    auto batch = queue.drain();
    REQUIRE(batch.size() == 2);
    CHECK(batch[0] == 10);
    CHECK(batch[1] == 20);

    diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 0);
    CHECK(diagnostics.dequeued == 2);
  }

  TEST_CASE("rejects new work after shutdown while preserving queued work")
  {
    AsyncWorkQueue<std::string> queue;

    CHECK(queue.enqueue("before"));
    queue.request_shutdown();

    CHECK_FALSE(queue.enqueue("after"));
    CHECK(queue.shutdown_requested());

    auto batch = queue.drain();
    REQUIRE(batch.size() == 1);
    CHECK(batch[0] == "before");
  }

  TEST_CASE("try_pop updates dequeue diagnostics")
  {
    AsyncWorkQueue<int> queue;
    int value = 0;

    CHECK_FALSE(queue.try_pop(value));
    CHECK(queue.enqueue(42));
    CHECK(queue.try_pop(value));
    CHECK(value == 42);

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 0);
    CHECK(diagnostics.dequeued == 1);
  }

  TEST_CASE("tracks worker activity and errors")
  {
    AsyncWorkQueue<int> queue;

    queue.set_worker_active(true);
    queue.record_worker_error();
    queue.record_worker_error();

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.worker_active);
    CHECK(diagnostics.worker_errors == 2);

    queue.set_worker_active(false);
    CHECK_FALSE(queue.diagnostics().worker_active);
  }
}

TEST_SUITE("object_tracker_core")
{
  TEST_CASE("tracks objects by class with oldest-to-newest ordering")
  {
    ObjectTrackerCore<int, int> tracker;

    tracker.add(1, 100);
    tracker.add(1, 200);
    tracker.add(2, 300);

    CHECK(tracker.class_count() == 2);
    CHECK(tracker.object_count() == 3);
    CHECK(tracker.latest_for_class(1) == 200);
    CHECK(tracker.latest_for_class(3) == 0);

    auto objects = tracker.objects_for_class(1);
    REQUIRE(objects.size() == 2);
    CHECK(objects[0] == 100);
    CHECK(objects[1] == 200);
  }

  TEST_CASE("removes a single object from a class bucket")
  {
    ObjectTrackerCore<int, int> tracker;
    tracker.add(1, 100);
    tracker.add(1, 200);

    CHECK(tracker.remove(1, 100));
    CHECK_FALSE(tracker.remove(1, 999));

    auto objects = tracker.objects_for_class(1);
    REQUIRE(objects.size() == 1);
    CHECK(objects[0] == 200);
    CHECK(tracker.latest_for_class(1) == 200);
  }

  TEST_CASE("removes an object from every class bucket")
  {
    ObjectTrackerCore<int, int> tracker;
    tracker.add(1, 100);
    tracker.add(2, 100);
    tracker.add(2, 200);

    CHECK(tracker.remove_object_from_all(100) == 2);
    CHECK(tracker.objects_for_class(1).empty());

    auto remaining = tracker.objects_for_class(2);
    REQUIRE(remaining.size() == 1);
    CHECK(remaining[0] == 200);
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

  TEST_CASE("fleet transition decisions classify arrivals and mining")
  {
    auto arrivedInSystem = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Warping),
        static_cast<int>(FleetBarTransitionState::Impulsing),
        true,
        false,
        false,
        false,
        false,
        "K'VORT",
    });
    CHECK(arrivedInSystem.kind == FleetBarTransitionNotificationKind::ArrivedInSystem);
    CHECK(arrivedInSystem.title == "Fleet Arrived");
    CHECK(arrivedInSystem.body == "Your K'VORT has arrived in-system");

    auto arrivedAtDestination = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Impulsing),
        static_cast<int>(FleetBarTransitionState::IdleInSpace),
        false,
        true,
        false,
        false,
        false,
        "K'VORT",
    });
    CHECK(arrivedAtDestination.kind == FleetBarTransitionNotificationKind::ArrivedAtDestination);
    CHECK(arrivedAtDestination.body == "Your K'VORT has arrived at its destination");

    auto startedMining = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::IdleInSpace),
        static_cast<int>(FleetBarTransitionState::Mining),
        false,
        false,
        true,
        false,
        false,
        "K'VORT",
        "Parsteel",
        "1m 36s",
        "Current Cargo: 13%",
    });
    CHECK(startedMining.kind == FleetBarTransitionNotificationKind::StartedMining);
    CHECK(startedMining.title == "K'VORT started mining Parsteel");
    CHECK(startedMining.body == "ETA 1m 36s\nCurrent Cargo: 13%");
    CHECK(startedMining.clear_mining_eta);
  }

  TEST_CASE("fleet transition decisions suppress ambiguous docking")
  {
    auto ambiguousDocked = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::CanManage),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(ambiguousDocked.kind == FleetBarTransitionNotificationKind::None);
    CHECK(ambiguousDocked.suppressed_ambiguous_docked);

    auto dockedFromSpace = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Impulsing),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(dockedFromSpace.kind == FleetBarTransitionNotificationKind::Docked);
    CHECK(dockedFromSpace.title == "Fleet Docked");
    CHECK(dockedFromSpace.body == "Your K'VORT docked");

    auto repairComplete = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Repairing),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(repairComplete.kind == FleetBarTransitionNotificationKind::RepairComplete);
    CHECK(repairComplete.title == "Repair Complete");
    CHECK(repairComplete.body == "Your K'VORT finished repairs");
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
