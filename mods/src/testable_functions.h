#pragma once

// Testable pure functions extracted from notification_service.cc and
// battle_notify_parser.cc.  No IL2CPP, no platform, no game memory.

#include "patches/input_binding/input_binding.h"
#include "patches/input_binding/input_dispatcher.h"
#include "patches/space_action_inputs.h"

#include "patches/hotkey_policy.h"

#include "bounded_ttl_cache.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
  bool        saw_deprecated_alias  = false;
  bool        has_conflicting_alias = false;
};

// Startup shortcut policy: Scopely's shortcut map initializes only when the
// shortcut policy explicitly permits native shortcut ownership.
bool should_call_original_initialize_actions(bool use_scopely_hotkeys, bool allow_key_fallthrough);
bool should_call_original_initialize_actions(ScopelyShortcutPolicy policy);

// Per-frame ScreenManager::Update policy after the router has made its decision.
bool should_call_original_screen_update(bool router_allows_original, bool allow_key_fallthrough);
bool should_call_original_screen_update(bool router_allows_original, OriginalFramePolicy policy);

ScopelyShortcutPolicy resolve_scopely_shortcut_policy(bool use_scopely_hotkeys, bool allow_key_fallthrough);
OriginalFramePolicy   resolve_original_frame_policy(bool allow_key_fallthrough);
const char*           scopely_shortcut_policy_name(ScopelyShortcutPolicy policy);
const char*           original_frame_policy_name(OriginalFramePolicy policy);
bool                  hotkey_dispatcher_owns_inputs(bool hotkeys_enabled, ScopelyShortcutPolicy scopely_shortcuts);
bool hotkey_router_should_suppress_native_shortcut_callback(bool input_system_callback, bool hotkeys_enabled,
                                                            ScopelyShortcutPolicy scopely_shortcuts,
                                                            bool                  native_shortcuts_suppressed);

// Escape-exit policy at the real back-button seam. Returns true when the current
// Escape-triggered back-button press should be suppressed instead of letting the
// game open its exit prompt.
bool should_suppress_escape_exit(bool disable_escape_exit, bool escape_pressed, int escape_exit_timer_ms,
                                 int64_t elapsed_ms_since_last_escape_press);

// Per-frame space-action routing policy after inputs are sampled.
bool hotkey_router_should_execute_space_action(const SpaceActionInputs& inputs, bool deferred_retry_pending);

enum class FleetActionRequestMode {
  None = 0,
  Default,
  AskHelp,
};

FleetActionRequestMode fleet_action_request_mode(bool wanted_state_available, bool help_state_active);
const char*            fleet_action_request_mode_name(FleetActionRequestMode mode);
bool hotkey_router_should_clear_deferred_space_action(bool forced_retry_pending, uint64_t generation_before,
                                                      uint64_t generation_after);

bool space_action_duplicate_submission_should_suppress(uint64_t previous_fleet_id, uintptr_t previous_target_identity,
                                                       uint64_t current_fleet_id, uintptr_t current_target_identity,
                                                       int64_t elapsed_ms, int64_t suppression_window_ms);

// Translate canonical fleet winners into space-action intent consumed by fleet_actions.
SpaceActionInputs hotkey_router_runtime_space_action_inputs(bool fleet_primary_pressed, bool fleet_secondary_pressed,
                                                            bool fleet_queue_add_pressed,
                                                            bool fleet_recall_cancel_pressed, bool fleet_recall_pressed,
                                                            bool fleet_repair_pressed, bool fleet_service_pressed);

// Resolve the canonical disable-hotkeys shortcut while accepting deprecated keys.
HotkeyDisableShortcutAliasDecision resolve_hotkey_disable_shortcut_alias(const HotkeyDisableShortcutAliasInput& input);

// Legacy [ui].notify_on_banner_types / notify_banner_types compatibility.
bool legacy_notification_allowlist_requests_all(std::string_view token);

enum class HotkeyRouterStartupAction {
  Continue = 0,
  DisableHotkeys,
  EnableHotkeys,
  AllowOriginal,
  SuppressOriginal,
};

enum class HotkeyRouterDispatchAction {
  Continue = 0,
  SuppressOriginal,
  AllowOriginal,
};

HotkeyRouterStartupAction hotkey_router_startup_action(bool disable_hotkeys_pressed, bool enable_hotkeys_pressed,
                                                       bool use_scopely_hotkeys, bool hotkeys_enabled);
HotkeyRouterStartupAction hotkey_router_startup_action(bool disable_hotkeys_pressed, bool enable_hotkeys_pressed,
                                                       ScopelyShortcutPolicy scopely_shortcuts, bool hotkeys_enabled);
int                       hotkey_router_ship_select_request(const std::array<bool, 8>& ship_select_keys_down);
bool hotkey_router_should_clear_input_focus(bool escape_pressed, bool input_focused, bool is_in_chat);
bool hotkey_router_should_toggle_queue(bool is_in_chat, bool input_focused, bool toggle_queue_pressed);
HotkeyRouterDispatchAction hotkey_router_dispatch_action(bool entry_active, bool handler_stops,
                                                         bool handler_allows_original);
HotkeyRouterDispatchAction hotkey_router_dispatch_action(bool entry_active, bool handler_stops,
                                                         bool handler_allows_original,
                                                         bool key_event_consumes_original);

enum class HotkeyRouterQuitAction {
  None = 0,
  QuitProcess,
};

enum class HotkeyRouterSelectCurrentAction {
  None = 0,
  ViewActiveFleet,
};

enum class HotkeyRouterChatOpenAction {
  None = 0,
  ActivateExistingInput,
  OpenAllianceSide,
  OpenAllianceFullscreen,
};

enum class HotkeyRouterChatChannelAction {
  None = 0,
  Global,
  Alliance,
  Private,
};

enum class HotkeyRouterOfficerCanvasAction {
  None = 0,
  MoveLeft,
  MoveRight,
};

enum class HotkeyRouterSimpleFleetAction {
  None = 0,
  QueueClear,
  ViewInfo,
};

HotkeyRouterQuitAction          hotkey_router_quit_action(bool quit_pressed);
HotkeyRouterSelectCurrentAction hotkey_router_select_current_action(bool is_in_chat, bool input_focused,
                                                                    bool select_current_pressed);
HotkeyRouterChatOpenAction      hotkey_router_chat_open_action(bool is_in_chat, bool input_focused, bool side_chat_open,
                                                               input_binding::InputActionId action);
HotkeyRouterChatChannelAction   hotkey_router_chat_channel_action(bool is_in_chat, input_binding::InputActionId action);
HotkeyRouterOfficerCanvasAction hotkey_router_officer_canvas_action(bool is_in_chat, bool input_focused,
                                                                    input_binding::InputActionId action);
input_binding::InputActionId    hotkey_router_table_dispatch_request(bool is_in_chat, bool input_focused,
                                                                     input_binding::InputActionId action);
HotkeyRouterSimpleFleetAction   hotkey_router_simple_fleet_action(bool                         input_focused,
                                                                  input_binding::InputActionId action);
std::array<bool, 8>
hotkey_router_native_fleet_selection_guard_slots(std::span<const input_binding::DispatchCandidate> winners,
                                                 bool dispatcher_owns_inputs);
std::array<bool, 8> hotkey_router_update_native_fleet_selection_guard_slots(
    const std::array<bool, 8>& previous_slots, std::span<const input_binding::DispatchCandidate> winners,
    std::span<const input_binding::DispatchKeyState> key_states, bool dispatcher_owns_inputs);
bool hotkey_router_native_shortcuts_suppressed(bool                                              previous_suppressed,
                                               std::span<const input_binding::DispatchCandidate> winners,
                                               std::span<const input_binding::DispatchKeyState>  key_states,
                                               bool dispatcher_owns_inputs);

enum class IncomingAttackPolicyAttackerKind {
  Unknown = 0,
  Player  = 1,
  Hostile = 2,
};

enum class IncomingAttackPolicyTargetKind {
  Global  = 0,
  Fleet   = 1,
  Station = 2,
};

struct IncomingAttackPolicyDedupKey {
  IncomingAttackPolicyTargetKind   target_kind   = IncomingAttackPolicyTargetKind::Global;
  uint64_t                         target_id     = 0;
  IncomingAttackPolicyAttackerKind attacker_kind = IncomingAttackPolicyAttackerKind::Unknown;
  std::string                      attacker_identity;
};

struct IncomingAttackPolicyDedupKeyHasher {
  size_t operator()(const IncomingAttackPolicyDedupKey& key) const noexcept;
};

bool operator==(const IncomingAttackPolicyDedupKey& lhs, const IncomingAttackPolicyDedupKey& rhs);

struct IncomingAttackPolicyDedupeResult {
  bool   emitted              = false;
  bool   suppressed_by_window = false;
  bool   evicted_oldest       = false;
  size_t cache_size           = 0;
};

class IncomingAttackPolicyDeduper
{
public:
  explicit IncomingAttackPolicyDeduper(size_t max_entries = 256);

  IncomingAttackPolicyDedupeResult should_emit(const IncomingAttackPolicyDedupKey& key, int64_t now_seconds);
  size_t                           size() const;
  bool                             contains(const IncomingAttackPolicyDedupKey& key) const;

private:
  using DedupeClock = std::chrono::steady_clock;

  BoundedTtlDeduper<IncomingAttackPolicyDedupKey, DedupeClock, IncomingAttackPolicyDedupKeyHasher> recent_;
};

IncomingAttackPolicyAttackerKind incoming_attack_policy_attacker_kind_from_fleet_type(int attackerFleetType);
const char*                    incoming_attack_policy_attacker_kind_name(IncomingAttackPolicyAttackerKind attackerKind);
IncomingAttackPolicyTargetKind incoming_attack_policy_target_kind(uint64_t fleetId, int targetType);
IncomingAttackPolicyDedupKey   incoming_attack_policy_dedupe_key(uint64_t fleetId, int targetType,
                                                                 IncomingAttackPolicyAttackerKind attackerKind,
                                                                 std::string_view                 attackerIdentity);
const char*                    incoming_attack_policy_target_type_name(int targetType);
const char*                    incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind attackerKind);
std::string incoming_attack_policy_fleet_body(std::string_view shipName, IncomingAttackPolicyAttackerKind attackerKind);
std::string incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind attackerKind);
int64_t     incoming_attack_policy_dedupe_window_seconds(const IncomingAttackPolicyDedupKey& key);
bool        incoming_attack_policy_consumes_toast_state(int state);

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

enum class FleetBarTransitionState {
  Unknown      = 0,
  IdleInSpace  = 1,
  Docked       = 2,
  Mining       = 4,
  Destroyed    = 8,
  TieringUp    = 16,
  Repairing    = 32,
  CannotLaunch = 56,
  Battling     = 64,
  WarpCharging = 128,
  Warping      = 256,
  CanRemove    = 384,
  CannotMove   = 504,
  Impulsing    = 512,
  CanManage    = 899,
  Capturing    = 1024,
  CanRecall    = 1541,
  CanEngage    = 1543,
  Deployed     = 1989,
  CanLocate    = 1991,
};

enum class FleetBarTransitionNotificationKind {
  None = 0,
  ArrivedInSystem,
  ArrivedAtDestination,
  StartedMining,
  RepairComplete,
  Docked,
};

struct FleetBarTransitionNotificationInput {
  int         old_state                     = 0;
  int         new_state                     = 0;
  bool        notify_arrived_in_system      = false;
  bool        notify_arrived_at_destination = false;
  bool        notify_started_mining         = false;
  bool        notify_docked                 = false;
  bool        notify_repair_complete        = false;
  std::string ship_name;
  std::string resource_name;
  std::string eta_text;
  std::string cargo_text;
};

struct FleetBarTransitionNotificationDecision {
  FleetBarTransitionNotificationKind kind = FleetBarTransitionNotificationKind::None;
  std::string                        title;
  std::string                        body;
  bool                               clear_mining_eta            = false;
  bool                               suppressed_ambiguous_docked = false;
};

FleetBarTransitionState fleet_bar_transition_state_from_value(int state);
const char*             fleet_bar_transition_notification_kind_name(FleetBarTransitionNotificationKind kind);
bool fleet_bar_transition_arrived_in_system_event_enabled(bool osArrivedInSystemEnabled, bool audioEnabled,
                                                          bool audioArrivedInSystemEnabled);
bool fleet_bar_transition_should_notify_os(FleetBarTransitionNotificationKind kind, bool osArrivedInSystemEnabled);
bool fleet_bar_transition_should_notify_audio(FleetBarTransitionNotificationKind kind, bool audioEnabled,
                                              bool audioArrivedInSystemEnabled);
const char* fleet_runtime_trigger_source_for_state_transition(int old_state, int new_state);
FleetBarTransitionNotificationDecision
fleet_bar_transition_notification_decision(const FleetBarTransitionNotificationInput& input);

// Battle summary formatting
struct BattleSummaryPreview {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;

  std::string format_body() const;
};
