#pragma once

// Testable pure functions extracted from notification_service.cc and
// battle_notify_parser.cc.  No IL2CPP, no platform, no game memory.

#include "bounded_ttl_cache.h"

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

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

// Startup shortcut policy: Scopely's shortcut map must initialize whenever the
// original game input path is expected to handle shortcuts.
bool should_call_original_initialize_actions(bool use_scopely_hotkeys, bool allow_key_fallthrough);

// Per-frame ScreenManager::Update policy after the router has made its decision.
bool should_call_original_screen_update(bool router_allows_original, bool allow_key_fallthrough);

// Escape-exit policy at the real back-button seam. Returns true when the current
// Escape-triggered back-button press should be suppressed instead of letting the
// game open its exit prompt.
bool should_suppress_escape_exit(bool disable_escape_exit,
                 bool escape_pressed,
                 int escape_exit_timer_ms,
                 int64_t elapsed_ms_since_last_escape_press);

// Resolve the canonical disable-hotkeys shortcut while accepting deprecated keys.
HotkeyDisableShortcutAliasDecision resolve_hotkey_disable_shortcut_alias(
    const HotkeyDisableShortcutAliasInput& input);

enum class IncomingAttackPolicyAttackerKind {
  Unknown = 0,
  Player = 1,
  Hostile = 2,
};

enum class IncomingAttackPolicyTargetKind {
  Global = 0,
  Fleet = 1,
  Station = 2,
};

struct IncomingAttackPolicyDedupKey {
  IncomingAttackPolicyTargetKind target_kind = IncomingAttackPolicyTargetKind::Global;
  uint64_t target_id = 0;
  IncomingAttackPolicyAttackerKind attacker_kind = IncomingAttackPolicyAttackerKind::Unknown;
  std::string attacker_identity;
};

struct IncomingAttackPolicyDedupKeyHasher {
  size_t operator()(const IncomingAttackPolicyDedupKey& key) const noexcept;
};

bool operator==(const IncomingAttackPolicyDedupKey& lhs, const IncomingAttackPolicyDedupKey& rhs);

struct IncomingAttackPolicyDedupeResult {
  bool emitted = false;
  bool suppressed_by_window = false;
  bool evicted_oldest = false;
  size_t cache_size = 0;
};

class IncomingAttackPolicyDeduper {
public:
  explicit IncomingAttackPolicyDeduper(size_t max_entries = 256);

  IncomingAttackPolicyDedupeResult should_emit(const IncomingAttackPolicyDedupKey& key, int64_t now_seconds);
  size_t size() const;
  bool contains(const IncomingAttackPolicyDedupKey& key) const;

private:
  using DedupeClock = std::chrono::steady_clock;

  BoundedTtlDeduper<IncomingAttackPolicyDedupKey, DedupeClock, IncomingAttackPolicyDedupKeyHasher> recent_;
};

IncomingAttackPolicyAttackerKind incoming_attack_policy_attacker_kind_from_fleet_type(int attackerFleetType);
const char* incoming_attack_policy_attacker_kind_name(IncomingAttackPolicyAttackerKind attackerKind);
IncomingAttackPolicyTargetKind incoming_attack_policy_target_kind(uint64_t fleetId, int targetType);
IncomingAttackPolicyDedupKey incoming_attack_policy_dedupe_key(uint64_t fleetId,
                                                               int targetType,
                                                               IncomingAttackPolicyAttackerKind attackerKind,
                                                               std::string_view attackerIdentity);
const char* incoming_attack_policy_target_type_name(int targetType);
const char* incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind attackerKind);
std::string incoming_attack_policy_fleet_body(std::string_view shipName,
                                              IncomingAttackPolicyAttackerKind attackerKind);
std::string incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind attackerKind);
int64_t incoming_attack_policy_dedupe_window_seconds(const IncomingAttackPolicyDedupKey& key);
bool incoming_attack_policy_consumes_toast_state(int state);

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
