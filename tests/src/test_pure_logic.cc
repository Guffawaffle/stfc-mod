#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "str_utils_pure.h"
#include "testable_functions.h"

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
