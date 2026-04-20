// Testable pure functions — implementations extracted from
// notification_service.cc and battle_notify_parser.cc so they can be
// compiled and tested without IL2CPP or game dependencies.

#include "testable_functions.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Toast state enum values (mirrored from prime/Toast.h's ToastState enum
// so this file doesn't need the IL2CPP include chain)
// ---------------------------------------------------------------------------
enum ToastStateValues {
  TS_Standard                  = 0,
  TS_FactionWarning            = 1,
  TS_FactionLevelUp            = 2,
  TS_FactionLevelDown          = 3,
  TS_FactionDiscovered         = 4,
  TS_IncomingAttack            = 5,
  TS_IncomingAttackFaction     = 6,
  TS_FleetBattle               = 7,
  TS_StationBattle             = 8,
  TS_StationVictory            = 9,
  TS_Victory                   = 10,
  TS_Defeat                    = 11,
  TS_StationDefeat             = 12,
  TS_Tournament                = 14,
  TS_ArmadaCreated             = 15,
  TS_ArmadaCanceled            = 16,
  TS_ArmadaIncomingAttack      = 17,
  TS_ArmadaBattleWon           = 18,
  TS_ArmadaBattleLost          = 19,
  TS_DiplomacyUpdated          = 20,
  TS_JoinedTakeover            = 21,
  TS_CompetitorJoinedTakeover  = 22,
  TS_AbandonedTerritory        = 23,
  TS_TakeoverVictory           = 24,
  TS_TakeoverDefeat            = 25,
  TS_TreasuryProgress          = 26,
  TS_TreasuryFull              = 27,
  TS_Achievement               = 28,
  TS_AssaultVictory            = 29,
  TS_AssaultDefeat             = 30,
  TS_ChallengeComplete         = 31,
  TS_ChallengeFailed           = 32,
  TS_StrikeHit                 = 33,
  TS_StrikeDefeat              = 34,
  TS_WarchestProgress          = 35,
  TS_WarchestFull              = 36,
  TS_PartialVictory            = 37,
  TS_ArenaTimeLeft             = 38,
  TS_ChainedEventScored        = 39,
  TS_FleetPresetApplied        = 40,
  TS_SurgeWarmUpEnded          = 41,
  TS_SurgeHostileGroupDefeated = 42,
  TS_SurgeTimeLeft             = 43,
};

// ---------------------------------------------------------------------------
// toast_state_title
// ---------------------------------------------------------------------------
const char* toast_state_title(int state)
{
  switch (state) {
    case TS_Victory:                   return "Victory!";
    case TS_Defeat:                    return "Defeat";
    case TS_PartialVictory:            return "Partial Victory";
    case TS_StationVictory:            return "Station Victory!";
    case TS_StationDefeat:             return "Station Defeat";
    case TS_StationBattle:             return "Station Under Attack!";
    case TS_IncomingAttack:            return "Incoming Attack!";
    case TS_IncomingAttackFaction:     return "Incoming Faction Attack!";
    case TS_FleetBattle:               return "Fleet Battle";
    case TS_ArmadaBattleWon:           return "Armada Victory!";
    case TS_ArmadaBattleLost:          return "Armada Defeated";
    case TS_ArmadaCreated:             return "Armada Created";
    case TS_ArmadaCanceled:            return "Armada Canceled";
    case TS_ArmadaIncomingAttack:      return "Armada Under Attack!";
    case TS_AssaultVictory:            return "Assault Victory!";
    case TS_AssaultDefeat:             return "Assault Defeat";
    case TS_Tournament:                return "Event Progress";
    case TS_ChainedEventScored:        return "Event Progress";
    case TS_Achievement:               return "Achievement";
    case TS_ChallengeComplete:         return "Challenge Complete";
    case TS_ChallengeFailed:           return "Challenge Failed";
    case TS_TakeoverVictory:           return "Takeover Victory!";
    case TS_TakeoverDefeat:            return "Takeover Defeat";
    case TS_TreasuryProgress:          return "Treasury Progress";
    case TS_TreasuryFull:              return "Treasury Full";
    case TS_WarchestProgress:          return "Warchest Progress";
    case TS_WarchestFull:              return "Warchest Full";
    case TS_FactionLevelUp:            return "Faction Level Up";
    case TS_FactionLevelDown:          return "Faction Level Down";
    case TS_FactionDiscovered:         return "Faction Discovered";
    case TS_FactionWarning:            return "Faction Warning";
    case TS_DiplomacyUpdated:          return "Diplomacy Updated";
    case TS_StrikeHit:                 return "Strike Hit";
    case TS_StrikeDefeat:              return "Strike Defeat";
    case TS_SurgeWarmUpEnded:          return "Surge Started";
    case TS_SurgeHostileGroupDefeated: return "Surge Hostiles Defeated";
    case TS_SurgeTimeLeft:             return "Surge Time Warning";
    case TS_ArenaTimeLeft:             return "Arena Time Warning";
    case TS_FleetPresetApplied:        return "Fleet Preset Applied";
    default:                           return nullptr;
  }
}

// ---------------------------------------------------------------------------
// strip_unity_rich_text
// ---------------------------------------------------------------------------
std::string strip_unity_rich_text(const std::string& s)
{
  std::string result;
  result.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '<') {
      auto end = s.find('>', i);
      if (end != std::string::npos) { i = end + 1; continue; }
    }
    result += s[i++];
  }
  return result;
}

// ---------------------------------------------------------------------------
// parse_hull_key
// ---------------------------------------------------------------------------
std::string parse_hull_key(const std::string& key)
{
  auto s = key;

  if (s.size() > 5 && s.ends_with("_LIVE"))
    s = s.substr(0, s.size() - 5);

  if (s.starts_with("Hull_"))
    s = s.substr(5);

  for (auto& c : s)
    if (c == '_') c = ' ';

  if (s.size() >= 2 && s[0] == 'L' && std::isdigit(s[1])) {
    auto space = s.find(' ');
    auto lvl   = s.substr(1, space == std::string::npos ? std::string::npos : space - 1);
    auto rest  = space == std::string::npos ? "" : s.substr(space);
    s = "Lv." + lvl + rest;
  }

  return s;
}

// ---------------------------------------------------------------------------
// Fleet notification formatting
// ---------------------------------------------------------------------------
std::string format_duration_short(int64_t seconds)
{
  if (seconds <= 0) return "";

  auto hourPart   = seconds / 3600;
  auto minutePart = (seconds % 3600) / 60;
  auto secondPart = seconds % 60;

  std::ostringstream out;
  if (hourPart > 0) {
    out << hourPart << "h";
    if (minutePart > 0) out << ' ' << minutePart << "m";
    return out.str();
  }

  if (minutePart > 0) {
    out << minutePart << "m";
    if (secondPart > 0) out << ' ' << secondPart << "s";
    return out.str();
  }

  out << secondPart << "s";
  return out.str();
}

std::string format_cargo_fill_text(float fillLevel)
{
  if (!std::isfinite(fillLevel) || fillLevel < 0.0f) return "";

  auto percent = static_cast<int>(std::lround(std::clamp(fillLevel, 0.0f, 1.0f) * 100.0f));
  return "Current Cargo: " + std::to_string(percent) + "%";
}

std::string format_started_mining_title(const std::string& shipName, const std::string& resourceName)
{
  auto subject = shipName.empty() ? std::string{"Fleet"} : shipName;
  auto title   = subject + " started mining";

  if (!resourceName.empty()) {
    title += " " + resourceName;
  }

  return title;
}

std::string format_started_mining_body(const std::string& etaText, const std::string& cargoText)
{
  std::string body;

  if (!etaText.empty()) {
    body += "ETA " + etaText;
  }

  if (!cargoText.empty()) {
    if (!body.empty()) {
      body += "\n";
    }
    body += cargoText;
  }

  return body;
}

std::string format_node_depleted_body(const std::string& shipName, const std::string& resourceName,
                                      const std::string& cargoText)
{
  auto subject = (shipName.empty() || shipName == "?") ? std::string{"Fleet"} : shipName;
  auto body    = subject + " depleted its";

  if (!resourceName.empty()) {
    body += " " + resourceName + " node.";
  } else {
    body += " node.";
  }

  if (!cargoText.empty()) {
    body += " " + cargoText + ".";
  }

  return body;
}

// ---------------------------------------------------------------------------
// BattleSummaryPreview::format_body
// ---------------------------------------------------------------------------
std::string BattleSummaryPreview::format_body() const
{
  auto format_side = [](const std::string& name, const std::string& ship) -> std::string {
    if (name.empty()) return "";
    if (ship.empty()) return name;
    return name + " (" + ship + ")";
  };

  auto left  = format_side(playerName, playerShip);
  auto right = format_side(enemyName, enemyShip);
  if (left.empty() && right.empty()) return "";
  if (left.empty()) return right;
  if (right.empty()) return left;
  return left + " vs " + right;
}
