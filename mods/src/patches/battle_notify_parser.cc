/**
 * @file battle_notify_parser.cc
 * @brief Extracts battle participant names and ship hulls from combat toast data.
 *
 * Parses BattleResultHeader objects attached to combat-related Toast events.
 * Uses SEH on Windows to guard against invalid IL2CPP pointers that can occur
 * when game objects are collected mid-access. Resolves hull IDs to readable
 * names via SpecService.
 */
#include "patches/battle_notify_parser.h"
#include "patches/game_localization.h"

#include "str_utils.h"
#include "testable_functions.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/BattleResultHeader.h>
#include <prime/HullSpec.h>
#include <prime/SpecService.h>
#include <prime/Toast.h>
#include <prime/UserProfile.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include <cctype>
#include <string>

#if _WIN32
#include <windows.h>
#endif

// ─── SEH Safety Wrapper ───────────────────────────────────────────────────────────────

/**
 * @brief Execute a callable inside a Windows SEH __try/__except block.
 *
 * IL2CPP pointers may become invalid between frames due to GC. This wrapper
 * catches access violations so a single bad pointer doesn't crash the mod.
 *
 * @return true if fn() completed without an SEH exception.
 */
template <typename Fn>
static bool seh_call(Fn fn)
{
#if _WIN32
  __try {
    fn();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  fn();
  return true;
#endif
}

// ─── Hull Name Parsing ───────────────────────────────────────────────────────────────

/**
 * @brief Convert an internal hull key string to a human-readable name.
 *
 * Strips the "Hull_" prefix and "_LIVE" suffix, replaces underscores with
 * spaces, and converts level prefixes ("L30") to "Lv.30" format.
 * Example: "Hull_L30_Destroyer_Klingon_LIVE" → "Lv.30 Destroyer Klingon"
 *
 * @param key Raw hull name key from HullSpec.
 * @return Formatted display name.
 */
static std::string parse_hull_key(const std::string& key)
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

  auto new_word = true;
  for (auto& character : s) {
    const auto byte = static_cast<unsigned char>(character);
    if (std::isalpha(byte)) {
      character = static_cast<char>(new_word ? std::toupper(byte) : std::tolower(byte));
      new_word  = false;
    } else if (character == ' ' || character == '-' || character == '_') {
      new_word = true;
    }
  }

  return s;
}

// ─── Hull ID Resolution ──────────────────────────────────────────────────────────────

/**
 * @brief Resolve a hull ID to a display name via the game's SpecService.
 * @param brh BattleResultHeader providing access to SpecService.
 * @param hullId The numeric hull ID to look up.
 * @return Parsed display name, or "Hull#<id>" if lookup fails.
 */
static std::string resolve_hull_name(BattleResultHeader* brh, long hullId)
{
  if (hullId == 0) return "";

  auto* specSvc = reinterpret_cast<SpecService*>(brh->get_SpecService());
  if (!specSvc) return fmt::format("Hull#{}", hullId);

  auto* hull = specSvc->GetHull(hullId);
  if (!hull) return fmt::format("Hull#{}", hullId);

  if (auto* id_refs = hull->IdRefsValue; id_refs) {
    const auto numeric_id = id_refs->LocaId;
    if (numeric_id != 0) {
      auto localized = game_localization::LocalizeIdentifier("ship_name_{0}", "ships", numeric_id);
      if (!localized.empty() && localized != fmt::format("ship_name_{}", numeric_id)) {
        return localized;
      }
    }

    if (auto* string_id = id_refs->LocaStringId; string_id) {
      const auto identifier = to_string(string_id);
      auto localized = game_localization::LocalizeIdentifier("ship_name_{0}", "ships", identifier);
      if (!localized.empty() && localized != fmt::format("ship_name_{}", identifier)) {
        return localized;
      }
    }
  }

  auto* nameStr = hull->Name;
  auto nameKey  = nameStr ? to_string(nameStr) : std::string{};
  if (!nameKey.empty()) return parse_hull_key(nameKey);

  return fmt::format("Hull#{}", hullId);
}

// ─── Battle Summary Formatting ───────────────────────────────────────────────────────

/** Intermediate structure holding extracted battle participant data. */
struct BattleSummaryData {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;
  bool        isPvp = false;

  /** @brief Format the summary as "Player (Ship) vs Enemy (Ship)".
   *  For NPCs (empty name), uses the ship hull name as the identifier. */
  std::string format_body() const
  {
    auto format_side = [](const std::string& name, const std::string& ship) -> std::string {
      if (!name.empty() && !ship.empty()) return fmt::format("{} ({})", name, ship);
      if (!name.empty()) return name;
      if (!ship.empty()) return ship;
      return "";
    };

    auto left  = format_side(playerName, playerShip);
    auto right = !isPvp && !enemyName.empty() ? enemyName : format_side(enemyName, enemyShip);
    if (left.empty() && right.empty()) return "";
    if (left.empty()) return right;
    if (right.empty()) return left;
    return left + " vs " + right;
  }
};

// ─── BattleResultHeader Extraction ───────────────────────────────────────────────────

/**
 * @brief Extract player and enemy names + ship hull names from a BattleResultHeader.
 *
 * Each field extraction is wrapped in seh_call() to survive invalid pointers.
 * Falls back to NPC LocaId formatting when player names are empty.
 *
 * @param data Raw Il2CppObject* from Toast::get_Data(), cast to BattleResultHeader.
 * @return Populated BattleSummaryData (fields may be empty on failure).
 */
static BattleSummaryData build_battle_data(Il2CppObject* data)
{
  BattleSummaryData result;
  if (!data) return result;

  auto* brh = reinterpret_cast<BattleResultHeader*>(data);

  if (!seh_call([&] {
        auto* p       = brh->get_PlayerUserProfile();
        auto* profile = reinterpret_cast<UserProfile*>(p);
        if (profile) {
          auto* nameStr = profile->Name;
          if (nameStr) result.playerName = to_string(nameStr);
          // NPC profiles have empty names — leave blank, hull name used instead
        }
      }))
    spdlog::warn("[Notify] SEH: get_PlayerUserProfile crashed");

  if (!seh_call([&] {
        auto* e       = brh->get_EnemyUserProfile();
        auto* profile = reinterpret_cast<UserProfile*>(e);
        if (profile) {
          auto* nameStr = profile->Name;
          if (nameStr) result.enemyName = to_string(nameStr);
          if (result.enemyName.empty() && profile->LocaId != 0) {
            auto localized =
                game_localization::LocalizeIdentifier("marauder_name_only_{0}", "navigation", profile->LocaId);
            if (!localized.empty() && localized != "Unknown") {
              result.enemyName = std::move(localized);
            }
          }
        }
      }))
    spdlog::warn("[Notify] SEH: get_EnemyUserProfile crashed");

  if (!seh_call([&] {
        const auto type = brh->Type;
        result.isPvp    = type == BattleType::Fleet || type == BattleType::Base || type == BattleType::ArmadaBase
                       || type == BattleType::ArmadaAsb || type == BattleType::ArmadaMta
                       || type == BattleType::PvpCuttingBeam || type == BattleType::PvpChainShot;
      }))
    spdlog::warn("[Notify] SEH: BattleType crashed");

  if (!seh_call([&] {
        auto hid          = brh->PlayerShipHullId;
        result.playerShip = resolve_hull_name(brh, hid);
      }))
    spdlog::warn("[Notify] SEH: PlayerShipHullId crashed");

  if (!seh_call([&] {
        auto hid         = brh->EnemyShipHullId;
        result.enemyShip = resolve_hull_name(brh, hid);
      }))
    spdlog::warn("[Notify] SEH: EnemyShipHullId crashed");

  spdlog::debug("[Notify] Battle: {} ({}) vs {} ({})", result.playerName, result.playerShip,
                result.enemyName, result.enemyShip);
  return result;
}

// ─── Public API ──────────────────────────────────────────────────────────────────────
std::string battle_notify_parse(Toast* toast)
{
  auto state = toast->get_State();

  if (!toast_state_uses_battle_summary(state)) {
    return {};
  }

  auto* data = toast->get_Data();
  if (!data) return {};

  auto bsd = build_battle_data(data);
  return bsd.format_body();
}

bool battle_notify_is_armada(Toast* toast)
{
#if !defined(_WIN32)
  // This payload read relies on Windows SEH for fail-closed access to IL2CPP
  // memory. Do not expose an unguarded equivalent on other platforms.
  (void)toast;
  return false;
#else
  if (!toast) {
    return false;
  }

  auto* data = toast->get_Data();
  if (!data) {
    return false;
  }

  bool is_armada = false;
  if (!seh_call([&] { is_armada = reinterpret_cast<BattleResultHeader*>(data)->IsArmadaBattle; })) {
    spdlog::warn("[Notify] SEH: IsArmadaBattle crashed");
    return false;
  }

  return is_armada;
#endif
}
