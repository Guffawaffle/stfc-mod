#include "test_pure_common.h"

// ===========================================================================
// battle_log_decoder
// ===========================================================================

TEST_SUITE("battle_log_decoder")
{
  TEST_CASE("decode_probe_entry maps segment ids to ships and components")
  {
    auto probe          = nlohmann::json::object();
    probe["journal_id"] = 12345;
    probe["names"]      = nlohmann::json{{"player-1", {{"name", "Guff"}}}};

    auto journal              = nlohmann::json::object();
    journal["id"]             = 12345;
    journal["battle_type"]    = 8;
    journal["battle_time"]    = "2026-04-26T23:04:17";
    journal["initiator_id"]   = "player-1";
    journal["target_id"]      = "mar_45";
    journal["initiator_wins"] = true;
    journal["battle_log"]     = nlohmann::json::array(
        {-96, -90, -88, 111, -86, 10, 20, 0.5, -85, 222, -98, 900, 1, -99, -89, -90, -88, 0, -87, -84, -83, -89, -97});
    journal["resources_transferred"] = {{"2431852293", 263028}};
    journal["chest_drop"]            = {
        {"loot_roll_key", "MAR_45_Armada_Car_Uncommon"},
        {"chests_gained",
         nlohmann::json::array(
             {{{"count", 1},
               {"params", {{"ref_id", 129255444149608}, {"chest_name", "MAR_45_Armada_Car_Uncommon"}}}}})}};
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
    auto journal              = nlohmann::json::object();
    journal["id"]             = 2709118446356718841LL;
    journal["battle_type"]    = 8;
    journal["battle_time"]    = "2026-04-26T23:04:17";
    journal["initiator_id"]   = "player-1";
    journal["target_id"]      = "mar_45";
    journal["initiator_wins"] = true;
    journal["system_id"]      = 2682660367670527124LL;
    journal["battle_log"]     = nlohmann::json::array({-96, 2682660367670527124LL, -97});

    const auto names   = nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_id", 2682660367670527124LL}}}};
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

  TEST_CASE("sidecar battle event sequence is capture-only until enrichment is enabled")
  {
    const auto names = nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_name", "House of Test"}}},
                                      {"mar_45", {{"name", "Target"}}}};
    auto journal = nlohmann::json{{"id", 12345},
                                  {"battle_type", 8},
                                  {"battle_time", "2026-04-26T23:04:17"},
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
                                                          222,
                                                          -98,
                                                          900,
                                                          1,
                                                          -99,
                                                          -89,
                                                          -90,
                                                          -88,
                                                          0,
                                                          -87,
                                                          -84,
                                                          -83,
                                                          -89,
                                                          -97})}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", 1},
        {"ship_ids", nlohmann::json::array({111})},
        {"hull_ids", nlohmann::json::array({77})},
        {"ship_components", {{"111", nlohmann::json::array({900})}}},
    };
    journal["target_fleet_data"]["deployed_fleet"] = {
        {"uid", "mar_45"},
        {"fleet_id", 2},
        {"type", 2},
        {"ship_ids", nlohmann::json::array({0})},
        {"hull_ids", nlohmann::json::array({3066099110})},
        {"ship_components", {{"0", nlohmann::json::array({800})}}},
    };

    battle_log_decoder::DecodeOptions options;
    options.include_segments = true;
    const auto decoded = battle_log_decoder::decode_journal(journal, names, options, 12345);
    REQUIRE(decoded.value("ok", false));

    const auto capture_only = battle_log_decoder::build_sidecar_battle_event_sequence(
        journal, names, decoded, battle_log_decoder::CatalogResolver{}, false, 12345, 111);
    REQUIRE(capture_only.size() == 1);
    CHECK(capture_only[0]["type"] == "battle.capture");

    const auto enriched = battle_log_decoder::build_sidecar_battle_event_sequence(
        journal, names, decoded, battle_log_decoder::CatalogResolver{}, true, 12345, 111);
    REQUIRE(enriched.size() == 4);
    CHECK(enriched[0]["type"] == "battle.capture");
    CHECK(enriched[1]["type"] == "battle.report");
    CHECK(enriched[2]["type"] == "catalog.snapshot");
    CHECK(enriched[3]["type"] == "battle.analytics");
  }

  TEST_CASE("battle report preserves exact ship and fleet ids alongside numeric compatibility fields")
  {
    constexpr int64_t kPlayerFleetId = 2644013931949275600LL;
    constexpr int64_t kPlayerShipId = 2682548280591992155LL;
    constexpr int64_t kTargetFleetId = 2644013931932498400LL;
    constexpr int64_t kTargetShipId = 2679690622826529803LL;

    const auto names = nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_name", "House of Test"}, {"alliance_tag", "HOT"}}},
                                      {"mar_45", {{"name", "Target"}}}};
    auto journal = nlohmann::json{{"id", 333},
                                  {"battle_type", 8},
                                  {"battle_time", "2026-05-24T22:46:59"},
                                  {"initiator_id", "player-1"},
                                  {"target_id", "mar_45"},
                                  {"initiator_wins", true},
                                  {"battle_log",
                                   nlohmann::json::array({-96,
                                                          kPlayerShipId,
                                                          -98,
                                                          900,
                                                          kTargetShipId,
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
                                                          -97})}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", kPlayerFleetId},
        {"ship_ids", nlohmann::json::array({kPlayerShipId})},
        {"hull_ids", nlohmann::json::array({77})},
        {"ship_components", {{std::to_string(kPlayerShipId), nlohmann::json::array({900})}}},
    };
    journal["target_fleet_data"]["deployed_fleet"] = {
        {"uid", "mar_45"},
        {"fleet_id", kTargetFleetId},
        {"type", 2},
        {"ship_ids", nlohmann::json::array({kTargetShipId})},
        {"hull_ids", nlohmann::json::array({3066099110})},
        {"ship_components", {{std::to_string(kTargetShipId), nlohmann::json::array({800})}}},
    };

    battle_log_decoder::DecodeOptions options;
    options.include_segments = true;

    const auto decoded = battle_log_decoder::decode_journal(journal, names, options);
    REQUIRE(decoded.value("ok", false));
    REQUIRE(decoded["participants"].is_array());
    CHECK(decoded["participants"][0]["fleet_id"] == kPlayerFleetId);
    CHECK(decoded["participants"][0]["fleet_id_exact"] == std::to_string(kPlayerFleetId));
    CHECK(decoded["participants"][0]["ship_ids"] == nlohmann::json::array({kPlayerShipId}));
    CHECK(decoded["participants"][0]["ship_ids_exact"] == nlohmann::json::array({std::to_string(kPlayerShipId)}));

    const auto report = battle_log_decoder::build_sidecar_battle_report_event(journal, decoded, 333, 222);
    REQUIRE(report["report"]["fleets"].is_array());
    REQUIRE(report["report"]["attackRows"].is_array());
    REQUIRE(report["report"]["events"].is_array());
    CHECK(report["report"]["fleets"][0]["fleet_id"] == kPlayerFleetId);
    CHECK(report["report"]["fleets"][0]["fleet_id_exact"] == std::to_string(kPlayerFleetId));
    CHECK(report["report"]["fleets"][0]["ship_ids"] == nlohmann::json::array({kPlayerShipId}));
    CHECK(report["report"]["fleets"][0]["ship_ids_exact"] == nlohmann::json::array({std::to_string(kPlayerShipId)}));
    CHECK(report["report"]["events"][0]["ship_ids_exact"]
          == nlohmann::json::array({std::to_string(kPlayerShipId), std::to_string(kTargetShipId)}));
    CHECK(report["report"]["attackRows"][0]["attackerShipId"] == kPlayerShipId);
    CHECK(report["report"]["attackRows"][0]["attackerShipIdExact"] == std::to_string(kPlayerShipId));
    CHECK(report["report"]["attackRows"][0]["targetShipId"] == kTargetShipId);
    CHECK(report["report"]["attackRows"][0]["targetShipIdExact"] == std::to_string(kTargetShipId));
    CHECK(report["report"]["attackRows"][0]["attacker"]["shipId"] == kPlayerShipId);
    CHECK(report["report"]["attackRows"][0]["attacker"]["shipIdExact"] == std::to_string(kPlayerShipId));
    CHECK(report["report"]["attackRows"][0]["attacker"]["fleetId"] == kPlayerFleetId);
    CHECK(report["report"]["attackRows"][0]["attacker"]["fleetIdExact"] == std::to_string(kPlayerFleetId));
    CHECK(report["report"]["attackRows"][0]["target"]["shipId"] == kTargetShipId);
    CHECK(report["report"]["attackRows"][0]["target"]["shipIdExact"] == std::to_string(kTargetShipId));
    CHECK(report["report"]["attackRows"][0]["target"]["fleetId"] == kTargetFleetId);
    CHECK(report["report"]["attackRows"][0]["target"]["fleetIdExact"] == std::to_string(kTargetFleetId));

    const auto analytics = battle_log_decoder::build_sidecar_battle_analytics_event(journal, decoded, 333, 222);
    REQUIRE(analytics["analytics"]["attackRows"].is_array());
    CHECK(analytics["analytics"]["attackRows"][0]["attackerShipId"] == kPlayerShipId);
    CHECK(analytics["analytics"]["attackRows"][0]["attackerShipIdExact"] == std::to_string(kPlayerShipId));
    CHECK(analytics["analytics"]["attackRows"][0]["targetShipId"] == kTargetShipId);
    CHECK(analytics["analytics"]["attackRows"][0]["targetShipIdExact"] == std::to_string(kTargetShipId));
  }

  TEST_CASE("build_sidecar_battle_capture_event caps lossless integer recursion")
  {
    auto nested = nlohmann::json{{"leaf", 42}};
    for (int depth = 0; depth < 160; ++depth) {
      nested = nlohmann::json{{"next", std::move(nested)}};
    }

    auto journal          = nlohmann::json::object();
    journal["id"]         = 123;
    journal["battle_log"] = nlohmann::json::array({-96, -97});
    journal["nested"]     = std::move(nested);

    CHECK_NOTHROW(
        (void)battle_log_decoder::build_sidecar_battle_capture_event(journal, nlohmann::json::object(), 0, 0));

    const auto capture =
        battle_log_decoder::build_sidecar_battle_capture_event(journal, nlohmann::json::object(), 0, 0);
    CHECK(capture.dump().find("lossless_integer_json_max_depth") != std::string::npos);
  }

  TEST_CASE("hostile display names ignore retrieving placeholders and derive from reward keys")
  {
    const auto names   = nlohmann::json{{"mar_42", {{"name", "Retrieving..."}}}};
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
    const auto names =
        nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_name", "House of Test"}, {"alliance_tag", "HOT"}}}};
    auto journal = nlohmann::json{
        {"id", 333},
        {"battle_type", 8},
        {"battle_time", "2026-04-27T01:23:45"},
        {"initiator_id", "player-1"},
        {"target_id", "mar_45"},
        {"initiator_wins", true},
        {"battle_log",
         nlohmann::json::array({-96,  -90,        -88,  111,        -86,        10,         20,          0.5,  -85,
                                -83,  0,          -98,  800,        111,        1.0,        0.0,         1,    1,
                                1878, 83380359.0, 7514, 30103950.0, 7636.33728, 5405,       0.0,         0.0,  -99,
                                -89,  -97,        -96,  111,        -98,        900,        0,           1.0,  0.0,
                                1,    0,          2501, 80608653.0, 10002,      19017128.0, 10166.36544, 7195, 0.3728,
                                0.0,  -93,        111,  -91,        1449938138, 2481912459, 0.02,        -92,  -94,
                                -99,  111,        -98,  901,        -95,        1.0,        -99,         -89,  -97})},
        {"chest_drop", {{"loot_roll_key", "MAR_45_Armada_Car_Uncommon"}}}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", 1},
        {"ship_ids", nlohmann::json::array({111})},
        {"hull_ids", nlohmann::json::array({77})},
        {"ship_components", {{"111", nlohmann::json::array({900, 901})}}},
    };
    journal["initiator_fleet_data"]["bridge_officers"] = nlohmann::json::array({{{"id", 10}, {"level", 10}, {"rank", 2}}});
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
    CHECK_FALSE(decoded["attack_rows"][0].contains("runtimeAbilityRowCandidates"));
    CHECK(decoded["attack_rows"][1]["attackerShipId"] == 111);
    CHECK(decoded["attack_rows"][1]["attacker"]["allianceName"] == "House of Test");
    CHECK(decoded["attack_rows"][1]["targetShipId"] == 0);
    CHECK(decoded["attack_rows"][1]["triggeredEffectCount"] == 1);
    CHECK(decoded["attack_rows"][1]["triggeredEffects"][0]["value"] == doctest::Approx(0.02));

    const auto report = battle_log_decoder::build_sidecar_battle_report_event(journal, decoded, 333, 222);
    CHECK(report["report"]["summary"]["roundCount"] == 2);
    CHECK(report["report"]["summary"]["attackRowCount"] == 2);
    CHECK(report["report"]["decode"]["status"] == "decoded_segments_with_attack_rows");
    CHECK(report["report"]["parity"]["sections"]["battleEvents"] == "partial");
    REQUIRE(report["report"]["csvParity"]["rows"].size() == 2);
    CHECK(report["report"]["csvParity"]["rows"][0]["type"] == "Attack");
    CHECK(report["report"]["csvParity"]["rows"][0]["criticalHit"] == "YES");
    CHECK(report["report"]["csvParity"]["rows"][0]["hullDamage"] == "1878");
    CHECK(report["report"]["csvParity"]["rows"][0]["totalDamage"] != "--");
    CHECK(report["report"]["csvParity"]["rows"][1]["attackerName"] == "Guff");
    CHECK(report["report"]["csvParity"]["rows"][1]["attackerAlliance"] == "House of Test");
    CHECK(report["report"]["rounds"][1]["summary"]["componentScalarCount"] == 1);
    CHECK(report["report"]["attackRows"][1]["round"] == 2);
    CHECK(report["report"]["attackRows"][1]["subRound"] == 1);

    const auto analytics = battle_log_decoder::build_sidecar_battle_analytics_event(journal, decoded, 333, 222);
    CHECK(analytics["protocolVersion"] == "stfc.sidecar.events.v0");
    CHECK(analytics["type"] == "battle.analytics");
    CHECK(analytics["schemaVersion"] == "stfc.battle.analytics.v0");
    CHECK(analytics["journalId"] == "333");
    CHECK(analytics["capturedAtUnixMs"] == 222);
    REQUIRE(analytics["analytics"]["csvParity"]["rows"].size() == 2);
    CHECK(analytics["analytics"]["csvParity"]["coverage"]["attackRecordCount"] == 2);
    CHECK(analytics["analytics"]["csvParity"]["coverage"]["catalogResolved"] == false);

    auto discovery_options = options;
    discovery_options.include_runtime_ability_candidates = true;
    const auto discovery_decoded = battle_log_decoder::decode_journal(journal, names, discovery_options);
    REQUIRE(discovery_decoded.contains("runtime_ability_row_candidates"));
    REQUIRE(discovery_decoded["runtime_ability_row_candidates"].is_array());
    REQUIRE(discovery_decoded["runtime_ability_row_candidates"].size() == 2);
    const auto& candidate = discovery_decoded["runtime_ability_row_candidates"][0];
    CHECK(candidate.value("schema", std::string{}) == "stfc.runtime_ability_row_candidate.v0");
    CHECK(candidate.value("phase", std::string{}) == "round_start");
    CHECK(candidate.value("ownerShipId", std::string{}) == "111");
    CHECK(candidate["ownerShip"]["shipId"] == 111);
    CHECK(candidate.value("sourceCategory", std::string{}) == "officer");
    CHECK(candidate.value("sourceId", std::string{}) == "10");
    CHECK(candidate["sourceName"].is_null());
    CHECK(candidate["abilityId"].is_null());
    CHECK(candidate["abilityName"].is_null());
    CHECK(candidate.value("effectId", std::string{}) == "20");
    CHECK(candidate["effectName"].is_null());
    CHECK(candidate.value("value", 0.0) == doctest::Approx(0.5));
    CHECK(candidate.value("marker", 0) == -86);
    CHECK(candidate.value("markerKind", std::string{}) == "source_value");
    CHECK(candidate["rawMarkers"].dump() == "[-86]");
    CHECK(candidate["tokenRange"].value("segmentIndex", -1) == 0);
    CHECK(candidate["tokenRange"].value("recordIndex", -1) == 0);
    CHECK(candidate["tokenRange"].value("recordStart", -1) == 4);
    CHECK(candidate["tokenRange"].value("recordEnd", -1) == 7);
    CHECK(candidate["tokenRange"].value("payloadStart", -1) == 10);
    REQUIRE(candidate["candidateIntegerTokens"].is_array());
    REQUIRE(candidate["candidateIntegerTokens"].size() == 2);
    CHECK(candidate["candidateIntegerTokens"][0].value("exact", std::string{}) == "10");
    CHECK(candidate["candidateIntegerTokens"][1].value("exact", std::string{}) == "20");
    const auto& triggered_candidate = discovery_decoded["runtime_ability_row_candidates"][1];
    CHECK(triggered_candidate.value("schema", std::string{}) == "stfc.runtime_ability_row_candidate.v0");
    CHECK(triggered_candidate.value("phase", std::string{}) == "post_attack");
    CHECK(triggered_candidate.value("ownerShipId", std::string{}) == "111");
    CHECK(triggered_candidate.value("targetShipId", std::string{}) == "111");
    CHECK(triggered_candidate.value("sourceCategory", std::string{}) == "unknown");
    CHECK(triggered_candidate.value("sourceId", std::string{}) == "1449938138");
    CHECK(triggered_candidate.value("effectId", std::string{}) == "2481912459");
    CHECK(triggered_candidate.value("triggered", false));
    CHECK(triggered_candidate.value("occurrenceCount", 0) == 1);
    CHECK(triggered_candidate.value("value", 0.0) == doctest::Approx(0.02));
    CHECK(triggered_candidate.value("marker", 0) == -91);
    CHECK(triggered_candidate.value("markerKind", std::string{}) == "triggered_effect_value");
    CHECK(triggered_candidate.value("confidence", std::string{}) == "experimental_triggered_effect_marker");

    const auto discovery_report = battle_log_decoder::build_sidecar_battle_report_event(journal, discovery_decoded, 333, 222);
    REQUIRE(discovery_report["report"].contains("experimental"));
    CHECK(discovery_report["report"]["experimental"]["runtimeAbilityRowCandidates"].size() == 2);

    const auto discovery_analytics =
        battle_log_decoder::build_sidecar_battle_analytics_event(journal, discovery_decoded, 333, 222);
    REQUIRE(discovery_analytics["analytics"].contains("experimental"));
    CHECK(discovery_analytics["analytics"]["experimental"]["runtimeAbilityRowCandidates"].size() == 2);
    CHECK(discovery_analytics["analytics"]["csvParity"]["coverage"]["abilityRowCount"] == 0);
  }

  TEST_CASE("build_sidecar_catalog_snapshot_event collects observed IDs and resolves players + alliances")
  {
    const auto names = nlohmann::json{
        {"player-1",
         {{"name", "Guff"}, {"alliance_id", 9001}, {"alliance_name", "House of Test"}, {"alliance_tag", "HOT"}}}};
    auto journal = nlohmann::json{{"id", 444},
                                  {"battle_type", 8},
                                  {"battle_time", "2026-04-27T01:23:45"},
                                  {"initiator_id", "player-1"},
                                  {"system_id", 555},
                                  {"battle_log", nlohmann::json::array({-96, -97})},
                                  {"resources_dropped", {{"4001", 100}, {"4002", 50}}}};
    journal["initiator_fleet_data"]["deployed_fleets"]["1"] = {
        {"uid", "player-1"},
        {"fleet_id", 1},
        {"ship_ids", nlohmann::json::array({111})},
        {"hull_ids", nlohmann::json::array({77})},
        {"ship_components", {{"111", nlohmann::json::array({900})}}},
    };

    // No resolver wired ΓÇö exercise the dataless path first.
    {
      const auto event = battle_log_decoder::build_sidecar_catalog_snapshot_event(
          journal, names, nlohmann::json::object(), {}, 444, 222);

      CHECK(event["protocolVersion"] == "stfc.sidecar.events.v0");
      CHECK(event["type"] == "catalog.snapshot");
      CHECK(event["schemaVersion"] == "stfc.catalog.snapshot.v0");
      CHECK(event["journalId"] == "444");
      CHECK(event["scope"] == "battle");

      const auto& domains = event["catalog"]["domains"];
      REQUIRE(domains.contains("hulls"));
      REQUIRE(domains["hulls"].contains("77"));
      CHECK(domains["hulls"]["77"]["unresolved"] == true);

      REQUIRE(domains["ships"].contains("111"));
      CHECK(domains["ships"]["111"]["unresolved"] == true);

      REQUIRE(domains["components"].contains("900"));
      CHECK(domains["resources"].size() == 2);
      REQUIRE(domains["resources"].contains("4001"));
      CHECK(domains["systems"].contains("555"));

      REQUIRE(domains["players"].contains("player-1"));
      CHECK(domains["players"]["player-1"]["name"] == "Guff");
      CHECK(domains["players"]["player-1"]["unresolved"] == false);
      CHECK(domains["players"]["player-1"]["allianceId"] == "9001");
      CHECK(domains["players"]["player-1"]["allianceName"] == "House of Test");

      REQUIRE(domains["alliances"].contains("9001"));
      CHECK(domains["alliances"]["9001"]["name"] == "House of Test");
      CHECK(domains["alliances"]["9001"]["tag"] == "HOT");
      CHECK(domains["alliances"]["9001"]["unresolved"] == false);

      const auto& coverage = event["catalog"]["coverage"];
      const auto  present  = coverage["domainsPresent"];
      CHECK(std::ranges::find(present, "hulls") != present.end());
      CHECK(std::ranges::find(present, "players") != present.end());
      CHECK(std::ranges::find(present, "alliances") != present.end());
      CHECK(coverage["totalEntries"].get<int>() > 0);
      CHECK(coverage["resolvedEntries"].get<int>() >= 2); // player + alliance
    }

    // Plug resolvers and assert names plus metadata land.
    {
      battle_log_decoder::CatalogResolver resolver{};
      resolver.hull_name          = [](int64_t id) { return id == 77 ? std::string{"Saladin"} : std::string{}; };
      resolver.hull_type          = [](int64_t id) { return id == 77 ? std::string{"Battleship"} : std::string{}; };
      resolver.component_name     = [](int64_t id) { return id == 900 ? std::string{"Phaser Bank"} : std::string{}; };
      resolver.component_metadata = [](int64_t id) {
        return id == 900 ? nlohmann::json{{"locaId", "12345"}, {"locaKey", "component_phaser_bank"}}
                         : nlohmann::json::object();
      };

      const auto event = battle_log_decoder::build_sidecar_catalog_snapshot_event(
          journal, names, nlohmann::json::object(), resolver, 444, 222);
      const auto& hulls = event["catalog"]["domains"]["hulls"];
      REQUIRE(hulls.contains("77"));
      CHECK(hulls["77"]["name"] == "Saladin");
      CHECK(hulls["77"]["type"] == "Battleship");
      CHECK(hulls["77"]["unresolved"] == false);
      const auto& components = event["catalog"]["domains"]["components"];
      REQUIRE(components.contains("900"));
      CHECK(components["900"]["name"] == "Phaser Bank");
      CHECK(components["900"]["locaId"] == "12345");
      CHECK(components["900"]["locaKey"] == "component_phaser_bank");
      CHECK(components["900"]["unresolved"] == false);
    }
  }

  TEST_CASE("build_sidecar_catalog_snapshot_event carries metadata for unresolved combat effect domains")
  {
    const auto names   = nlohmann::json::object();
    const auto journal = nlohmann::json{{"id", 445},
                                        {"battle_type", 8},
                                        {"battle_time", "2026-04-27T01:23:45"},
                                        {"battle_log", nlohmann::json::array({-96, -97})}};
    const auto decoded = nlohmann::json{
        {"attack_rows", nlohmann::json::array(
                            {{{"triggeredEffects", nlohmann::json::array({{{"kind", "officer_ability"}, {"id", 7001}},
                                                                          {{"kind", "ship_ability"}, {"id", 8001}},
                                                                          {{"kind", "forbidden_tech"}, {"id", 9001}},
                                                                          {{"kind", "buff"}, {"id", 10001}},
                                                                          {{"kind", "debuff"}, {"id", 11001}}})}}})}};

    battle_log_decoder::CatalogResolver resolver{};
    resolver.officer_metadata = [](int64_t id) {
      return id == 7001 ? nlohmann::json{{"locaId", "17001"}, {"locaKey", "officer_category"}}
                        : nlohmann::json::object();
    };
    resolver.ability_metadata = [](int64_t id) {
      return id == 8001 ? nlohmann::json{{"locaId", "18001"}} : nlohmann::json::object();
    };
    resolver.forbidden_tech_metadata = [](int64_t id) {
      return id == 9001 ? nlohmann::json{{"locaId", "19001"}, {"locaKey", "forbidden_tech_category"}}
                        : nlohmann::json::object();
    };
    resolver.buff_metadata = [](int64_t id) {
      return id == 10001 ? nlohmann::json{{"locaId", "20001"}, {"locaKey", "buff_category"}} : nlohmann::json::object();
    };
    resolver.debuff_metadata = [](int64_t id) {
      return id == 11001 ? nlohmann::json{{"locaId", "21001"}, {"locaKey", "debuff_category"}}
                         : nlohmann::json::object();
    };

    const auto event =
        battle_log_decoder::build_sidecar_catalog_snapshot_event(journal, names, decoded, resolver, 445, 222);
    const auto& domains = event["catalog"]["domains"];

    REQUIRE(domains["officers"].contains("7001"));
    CHECK(domains["officers"]["7001"]["unresolved"] == true);
    CHECK(domains["officers"]["7001"]["locaId"] == "17001");
    CHECK(domains["officers"]["7001"]["locaKey"] == "officer_category");

    REQUIRE(domains["abilities"].contains("8001"));
    CHECK(domains["abilities"]["8001"]["unresolved"] == true);
    CHECK(domains["abilities"]["8001"]["locaId"] == "18001");

    REQUIRE(domains["forbiddenTech"].contains("9001"));
    CHECK(domains["forbiddenTech"]["9001"]["unresolved"] == true);
    CHECK(domains["forbiddenTech"]["9001"]["locaKey"] == "forbidden_tech_category");

    REQUIRE(domains["buffs"].contains("10001"));
    CHECK(domains["buffs"]["10001"]["unresolved"] == true);
    CHECK(domains["buffs"]["10001"]["locaKey"] == "buff_category");

    REQUIRE(domains["debuffs"].contains("11001"));
    CHECK(domains["debuffs"]["11001"]["unresolved"] == true);
    CHECK(domains["debuffs"]["11001"]["locaKey"] == "debuff_category");

    const auto present = event["catalog"]["coverage"]["domainsPresent"];
    CHECK(std::ranges::find(present, "officers") != present.end());
    CHECK(std::ranges::find(present, "abilities") != present.end());
    CHECK(std::ranges::find(present, "forbiddenTech") != present.end());
    CHECK(std::ranges::find(present, "buffs") != present.end());
    CHECK(std::ranges::find(present, "debuffs") != present.end());
  }
}

// ===========================================================================
// str_utils_pure.h
// ===========================================================================


TEST_SUITE("parse_hull_key")
{
  TEST_CASE("full hull key with LIVE suffix")
  { CHECK(parse_hull_key("Hull_L30_Destroyer_Klingon_LIVE") == "Lv.30 Destroyer Klingon"); }

  TEST_CASE("hull key without LIVE suffix")
  { CHECK(parse_hull_key("Hull_L45_Battleship_Federation") == "Lv.45 Battleship Federation"); }

  TEST_CASE("hull key without level prefix")
  { CHECK(parse_hull_key("Hull_Jellyfish_LIVE") == "Jellyfish"); }

  TEST_CASE("minimal key")
  { CHECK(parse_hull_key("Hull_L1_Scout") == "Lv.1 Scout"); }

  TEST_CASE("empty string")
  { CHECK(parse_hull_key("") == ""); }

  TEST_CASE("no Hull_ prefix")
  {
    // Still processes underscores and level
    CHECK(parse_hull_key("L10_Frigate") == "Lv.10 Frigate");
  }
}

// ===========================================================================
// fleet notification formatting
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
