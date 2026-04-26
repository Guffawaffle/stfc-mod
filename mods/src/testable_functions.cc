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
// Hotkey startup/fallthrough/config decisions
// ---------------------------------------------------------------------------
bool should_call_original_initialize_actions(bool use_scopely_hotkeys, bool allow_key_fallthrough)
{
  return use_scopely_hotkeys || allow_key_fallthrough;
}

bool should_call_original_screen_update(bool router_allows_original, bool allow_key_fallthrough)
{
  return router_allows_original || allow_key_fallthrough;
}

bool should_suppress_escape_exit(bool disable_escape_exit,
                                 bool escape_pressed,
                                 int escape_exit_timer_ms,
                                 int64_t elapsed_ms_since_last_escape_press)
{
  if (!disable_escape_exit || !escape_pressed) {
    return false;
  }

  if (escape_exit_timer_ms <= 0) {
    return true;
  }

  return elapsed_ms_since_last_escape_press < 0 || elapsed_ms_since_last_escape_press > escape_exit_timer_ms;
}

HotkeyDisableShortcutAliasDecision resolve_hotkey_disable_shortcut_alias(
    const HotkeyDisableShortcutAliasInput& input)
{
  HotkeyDisableShortcutAliasDecision decision;
  decision.key = "set_hotkeys_disable";
  decision.value = input.default_value;
  decision.source_key = decision.key;
  decision.saw_deprecated_alias = input.has_deprecated_typo || input.has_legacy_disabled;

  if (input.has_canonical) {
    decision.value = input.canonical;

    if ((input.has_deprecated_typo && input.deprecated_typo != input.canonical)
        || (input.has_legacy_disabled && input.legacy_disabled != input.canonical)) {
      decision.has_conflicting_alias = true;
    }

    return decision;
  }

  if (input.has_deprecated_typo) {
    decision.value = input.deprecated_typo;
    decision.source_key = "set_hotkeys_disble";
    decision.used_deprecated_alias = true;

    if (input.has_legacy_disabled && input.legacy_disabled != input.deprecated_typo) {
      decision.has_conflicting_alias = true;
    }

    return decision;
  }

  if (input.has_legacy_disabled) {
    decision.value = input.legacy_disabled;
    decision.source_key = "set_hotkeys_disabled";
    decision.used_deprecated_alias = true;
  }

  return decision;
}

// ---------------------------------------------------------------------------
// Incoming attack policy
// ---------------------------------------------------------------------------
namespace {
constexpr int kIncomingAttackTargetTypeStation = 3;
constexpr int64_t kIncomingAttackGenericDedupeWindowSeconds = 10;
constexpr int64_t kIncomingAttackIdentifiedDedupeWindowSeconds = 120;
}

size_t IncomingAttackPolicyDedupKeyHasher::operator()(const IncomingAttackPolicyDedupKey& key) const noexcept
{
  auto hash = std::hash<uint64_t>{}(key.target_id);
  hash ^= std::hash<int>{}(static_cast<int>(key.target_kind)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  hash ^= std::hash<int>{}(static_cast<int>(key.attacker_kind)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  hash ^= std::hash<std::string>{}(key.attacker_identity) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  return hash;
}

bool operator==(const IncomingAttackPolicyDedupKey& lhs, const IncomingAttackPolicyDedupKey& rhs)
{
  return lhs.target_kind == rhs.target_kind && lhs.target_id == rhs.target_id &&
      lhs.attacker_kind == rhs.attacker_kind && lhs.attacker_identity == rhs.attacker_identity;
}

IncomingAttackPolicyDeduper::IncomingAttackPolicyDeduper(size_t max_entries)
  : recent_(max_entries)
{
}

IncomingAttackPolicyDedupeResult IncomingAttackPolicyDeduper::should_emit(const IncomingAttackPolicyDedupKey& key,
                                                                          int64_t now_seconds)
{
  const auto now = DedupeClock::time_point{std::chrono::seconds(now_seconds)};
  const auto ttl = std::chrono::seconds(incoming_attack_policy_dedupe_window_seconds(key));
  const auto dedupe_result = recent_.should_emit(key, now, ttl);
  return {dedupe_result.emitted,
          dedupe_result.suppressed_by_window,
          dedupe_result.evicted_oldest,
          dedupe_result.cache_size};
}

size_t IncomingAttackPolicyDeduper::size() const
{
  return recent_.size();
}

bool IncomingAttackPolicyDeduper::contains(const IncomingAttackPolicyDedupKey& key) const
{
  return recent_.contains(key);
}

IncomingAttackPolicyAttackerKind incoming_attack_policy_attacker_kind_from_fleet_type(int attackerFleetType)
{
  switch (attackerFleetType) {
    case 1:
      return IncomingAttackPolicyAttackerKind::Player;
    case 2:
    case 3:
    case 4:
    case 6:
      return IncomingAttackPolicyAttackerKind::Hostile;
    default:
      return IncomingAttackPolicyAttackerKind::Unknown;
  }
}

const char* incoming_attack_policy_attacker_kind_name(IncomingAttackPolicyAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Player:
      return "Player";
    case IncomingAttackPolicyAttackerKind::Hostile:
      return "Hostile";
    default:
      return "Unknown";
  }
}

IncomingAttackPolicyTargetKind incoming_attack_policy_target_kind(uint64_t fleetId, int targetType)
{
  if (targetType == kIncomingAttackTargetTypeStation) {
    return IncomingAttackPolicyTargetKind::Station;
  }

  if (fleetId != 0) {
    return IncomingAttackPolicyTargetKind::Fleet;
  }

  return IncomingAttackPolicyTargetKind::Global;
}

IncomingAttackPolicyDedupKey incoming_attack_policy_dedupe_key(uint64_t fleetId,
                                                               int targetType,
                                                               IncomingAttackPolicyAttackerKind attackerKind,
                                                               std::string_view attackerIdentity)
{
  const auto target_kind = incoming_attack_policy_target_kind(fleetId, targetType);
  const auto target_id = target_kind == IncomingAttackPolicyTargetKind::Fleet ? fleetId : 0;
  return { target_kind, target_id, attackerKind, std::string(attackerIdentity) };
}

const char* incoming_attack_policy_target_type_name(int targetType)
{
  switch (targetType) {
    case 0:
      return "None";
    case 1:
      return "Fleet";
    case 2:
      return "DockingPoint";
    case 3:
      return "Station";
    default:
      return "Unknown";
  }
}

const char* incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Hostile:
      return "Incoming Hostile Attack";
    case IncomingAttackPolicyAttackerKind::Player:
      return "Incoming Player Attack";
    default:
      return "Incoming Attack!";
  }
}

std::string incoming_attack_policy_fleet_body(std::string_view shipName,
                                              IncomingAttackPolicyAttackerKind attackerKind)
{
  const auto subject = shipName.empty() ? std::string{"fleet"} : std::string(shipName);
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Hostile:
      return "Your " + subject + " is being chased.";
    case IncomingAttackPolicyAttackerKind::Player:
      return "Your " + subject + " is under attack by another player.";
    default:
      return "Your " + subject + " is under attack.";
  }
}

std::string incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Hostile:
      return "Your station is under attack by a hostile.";
    case IncomingAttackPolicyAttackerKind::Player:
      return "Your station is under attack by another player.";
    default:
      return "Your station is under attack.";
  }
}

int64_t incoming_attack_policy_dedupe_window_seconds(const IncomingAttackPolicyDedupKey& key)
{
  return key.attacker_identity.empty() ? kIncomingAttackGenericDedupeWindowSeconds
                                       : kIncomingAttackIdentifiedDedupeWindowSeconds;
}

bool incoming_attack_policy_consumes_toast_state(int state)
{
  return state == 5 || state == 6;
}

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
// toast_state_uses_battle_summary
// ---------------------------------------------------------------------------
bool toast_state_uses_battle_summary(int state)
{
  switch (state) {
    case TS_Victory:
    case TS_Defeat:
    case TS_PartialVictory:
    case TS_StationVictory:
    case TS_StationDefeat:
    case TS_StationBattle:
    case TS_FleetBattle:
    case TS_ArmadaBattleWon:
    case TS_ArmadaBattleLost:
    case TS_AssaultVictory:
    case TS_AssaultDefeat:
      return true;
    default:
      return false;
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
