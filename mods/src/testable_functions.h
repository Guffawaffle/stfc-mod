#pragma once

// Testable pure functions extracted from notification_service.cc and
// battle_notify_parser.cc.  No IL2CPP, no platform, no game memory.

#include <cstdint>
#include <string>

// Toast state → human-readable title.  Returns nullptr for unknown states.
const char* toast_state_title(int state);

// Whether a toast state's payload is a BattleResultHeader-compatible battle summary.
bool toast_state_uses_battle_summary(int state);

// Strip Unity rich text tags: <color=#FF0000>, <b>, </size>, etc.
std::string strip_unity_rich_text(const std::string& s);

// Hull name key → display name:
//   "Hull_L30_Destroyer_Klingon_LIVE" → "Lv.30 Destroyer Klingon"
std::string parse_hull_key(const std::string& key);

// Fleet notification text helpers.
std::string format_duration_short(int64_t seconds);
std::string format_cargo_fill_text(float fillLevel);
std::string format_started_mining_title(const std::string& shipName, const std::string& resourceName);
std::string format_started_mining_body(const std::string& etaText, const std::string& cargoText);
std::string format_node_depleted_body(const std::string& shipName, const std::string& resourceName,
                                      const std::string& cargoText);

// Battle summary formatting
struct BattleSummaryPreview {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;

  std::string format_body() const;
};
