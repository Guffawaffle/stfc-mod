#include "test_pure_common.h"

#include "patches/sync_battlelog_payload.h"

TEST_SUITE("sync_battlelog_payload")
{
  TEST_CASE("legacy public battlelog sync payload preserves upstream-compatible shape")
  {
    const auto names =
        nlohmann::json{{"player-1", {{"name", "Guff"}, {"alliance_name", "House of Test"}, {"alliance_tag", "HOT"}}},
                       {"mar_45", {{"name", "Target"}}}};

    auto journal              = nlohmann::json::object();
    journal["id"]             = 2709118446356718841LL;
    journal["battle_type"]    = 8;
    journal["battle_time"]    = "2026-04-26T23:04:17";
    journal["initiator_id"]   = "player-1";
    journal["target_id"]      = "mar_45";
    journal["initiator_wins"] = true;
    journal["system_id"]      = 2682660367670527124LL;
    journal["battle_log"]     = nlohmann::json::array({-96, 2682660367670527124LL, -97});

    const auto payload = sync_battlelog_payload::BuildLegacyBattlelogSyncPayload(names, journal);

    REQUIRE(payload.is_array());
    REQUIRE(payload.size() == 1);

    const auto& entry = payload.front();
    CHECK(entry["type"] == "battlelog");
    CHECK(entry["names"] == names);
    CHECK(entry["journal"] == journal);
    CHECK(entry["journal"]["id"].is_number_integer());
    CHECK(entry["journal"]["battle_log"][1].is_number_integer());
    CHECK_FALSE(entry.contains("protocolVersion"));
    CHECK_FALSE(entry.contains("schemaVersion"));
    CHECK_FALSE(entry.contains("capture"));
    CHECK_FALSE(entry.contains("capturedAtUnixMs"));
  }
}
