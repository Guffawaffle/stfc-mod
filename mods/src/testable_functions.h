#pragma once

// Testable pure functions extracted from notification_service.cc and
// battle_notify_parser.cc.  No IL2CPP, no platform, no game memory.

#include <string>

// Toast state → human-readable title.  Returns nullptr for unknown states.
const char* toast_state_title(int state);

// Strip Unity rich text tags: <color=#FF0000>, <b>, </size>, etc.
std::string strip_unity_rich_text(const std::string& s);

// Hull name key → display name:
//   "Hull_L30_Destroyer_Klingon_LIVE" → "Lv.30 Destroyer Klingon"
std::string parse_hull_key(const std::string& key);

// Battle summary formatting
struct BattleSummaryData {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;

  std::string format_body() const;
};
