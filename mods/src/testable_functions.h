#pragma once

// Testable pure functions extracted from notification_service.cc and
// battle_notify_parser.cc.  No IL2CPP, no platform, no game memory.

#include <cstdint>
#include <string>

struct HotkeyDisableShortcutAliasInput {
  bool        has_canonical = false;
  std::string canonical;
  bool        has_deprecated_typo = false;
  std::string deprecated_typo;
  bool        has_legacy_disabled = false;
  std::string legacy_disabled;
  std::string default_value;
};

struct HotkeyDisableShortcutAliasDecision {
  std::string key;
  std::string value;
  std::string source_key;
  bool        used_deprecated_alias = false;
  bool        saw_deprecated_alias = false;
  bool        has_conflicting_alias = false;
};

// Startup shortcut policy: only the explicit Scopely hotkey toggle initializes
// Scopely's shortcut map. allow_key_fallthrough is a per-frame routing flag.
bool should_call_original_initialize_actions(bool use_scopely_hotkeys, bool allow_key_fallthrough);

// Per-frame ScreenManager::Update policy after the router has made its decision.
bool should_call_original_screen_update(bool router_allows_original, bool allow_key_fallthrough);

// Resolve the canonical disable-hotkeys shortcut while accepting deprecated keys.
HotkeyDisableShortcutAliasDecision resolve_hotkey_disable_shortcut_alias(
    const HotkeyDisableShortcutAliasInput& input);

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
