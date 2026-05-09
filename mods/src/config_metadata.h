#pragma once

#include "config.h"
#include "defaultconfig.h"
#include "toast_state.h"

#include <array>
#include <string_view>

namespace config_metadata
{
namespace DCC  = DefaultConfig::Control;
namespace DCN  = DefaultConfig::Notifications;
namespace DCSH = DefaultConfig::Shortcuts;

struct BoolConfigSpec {
  std::string_view section;
  std::string_view key;
  std::string_view runtime_key;
  bool             default_value;
  std::string_view docs;
};

inline constexpr BoolConfigSpec kHotkeysEnabledConfig{
    "control", "hotkeys_enabled", "hotkeys_enabled", DCC::hotkeys_enabled, "Master toggle for mod keyboard hotkeys.",
};

inline constexpr BoolConfigSpec kHotkeysExtendedConfig{
    "control", "hotkeys_extended", "hotkeys_extended", DCC::hotkeys_extended, "Enable extended keyboard shortcuts.",
};

inline constexpr BoolConfigSpec kUseScopelyHotkeysConfig{
    "control",
    "use_scopely_hotkeys",
    "use_scopely_hotkeys",
    DCC::use_scopely_hotkeys,
    "Use Scopely's built-in shortcut layer instead of the mod hotkey router.",
};

inline constexpr BoolConfigSpec kAllowKeyFallthroughConfig{
    "control",
    "allow_key_fallthrough",
    "allow_key_fallthrough",
    false,
    "Allow unhandled key frames to fall through to the original game input path.",
};

inline constexpr std::array kHotkeyBoolConfigSpecs{
    kHotkeysEnabledConfig,
    kHotkeysExtendedConfig,
    kUseScopelyHotkeysConfig,
    kAllowKeyFallthroughConfig,
};

struct ShortcutConfigSpec {
  std::string_view section;
  std::string_view key;
  std::string_view runtime_key;
  std::string_view default_value;
  std::string_view deprecated_typo_key;
  std::string_view legacy_key;
  std::string_view docs;
};

inline constexpr ShortcutConfigSpec kDisableHotkeysShortcutConfig{
    "shortcuts",
    "set_hotkeys_disable",
    "set_hotkeys_disable",
    DCSH::set_hotkeys_disabled,
    "set_hotkeys_disble",
    "set_hotkeys_disabled",
    "Shortcut that disables mod hotkeys at runtime.",
};

struct NotificationBoolConfigSpec {
  std::string_view canonical_path;
  std::string_view section;
  std::string_view key;
  std::string_view runtime_key;
  bool             default_value;
  bool NotificationConfig::* member;
  std::string_view           docs;
};

inline constexpr NotificationBoolConfigSpec notificationBoolConfigSpecs[] = {
    {"notifications.system.enabled", "notifications", "notifications_enabled", "notifications_enabled", DCN::enabled,
     &NotificationConfig::enabled, "Master switch for OS notifications."},
    {"notifications.audio.enabled", "notifications", "notifications_audio_enabled", "notifications_audio_enabled",
     DCN::audio_enabled, &NotificationConfig::audio_enabled, "Master switch for in-game audible notifications."},
    {"notifications.audio.fleet.arrived_in_system", "notifications", "notifications_audio_fleet_arrived_in_system",
     "notifications_audio_fleet_arrived_in_system", DCN::Audio::fleet_arrived_in_system,
     &NotificationConfig::audio_fleet_arrived_in_system, "Play an in-game sound when a fleet arrives in-system."},
    {"notifications.system.fleet.arrived_in_system", "notifications", "notifications_fleet_arrived_in_system",
     "notifications_fleet_arrived_in_system", DCN::Fleet::arrived_in_system,
     &NotificationConfig::fleet_arrived_in_system, "Notify when a fleet arrives in-system."},
    {"notifications.system.fleet.arrived_at_destination", "notifications", "notifications_fleet_arrived_at_destination",
     "notifications_fleet_arrived_at_destination", DCN::Fleet::arrived_at_destination,
     &NotificationConfig::fleet_arrived_at_destination, "Notify when a fleet arrives at its destination."},
    {"notifications.system.fleet.started_mining", "notifications", "notifications_fleet_started_mining",
     "notifications_fleet_started_mining", DCN::Fleet::started_mining, &NotificationConfig::fleet_started_mining,
     "Notify when a fleet starts mining."},
    {"notifications.system.fleet.node_depleted", "notifications", "notifications_fleet_node_depleted",
     "notifications_fleet_node_depleted", DCN::Fleet::node_depleted, &NotificationConfig::fleet_node_depleted,
     "Notify when a mining node is depleted."},
    {"notifications.system.fleet.docked", "notifications", "notifications_fleet_docked", "notifications_fleet_docked",
     DCN::Fleet::docked, &NotificationConfig::fleet_docked, "Notify when a fleet docks."},
    {"notifications.system.fleet.repair_complete", "notifications", "notifications_fleet_repair_complete",
     "notifications_fleet_repair_complete", DCN::Fleet::repair_complete, &NotificationConfig::fleet_repair_complete,
     "Notify when a repairing fleet docks."},
};

struct NotificationToggleSpec {
  std::string_view section;
  std::string_view key;
  std::string_view runtime_key;
  int              toast_state;
  bool             default_value;
  std::string_view deprecated_key;
  std::string_view docs;
};

inline constexpr NotificationToggleSpec notificationToggleSpecs[] = {
    {"notifications", "notifications_victory", "notifications_victory", Victory, DCN::Battle::victory, "",
     "Notify for victory battle toasts."},
    {"notifications", "notifications_defeat", "notifications_defeat", Defeat, DCN::Battle::defeat, "",
     "Notify for defeat battle toasts."},
    {"notifications", "notifications_partial_victory", "notifications_partial_victory", PartialVictory,
     DCN::Battle::partial_victory, "", "Notify for partial victory battle toasts."},
    {"notifications", "notifications_station_victory", "notifications_station_victory", StationVictory,
     DCN::Battle::station_victory, "", "Notify for station victory toasts."},
    {"notifications", "notifications_station_defeat", "notifications_station_defeat", StationDefeat,
     DCN::Battle::station_defeat, "", "Notify for station defeat toasts."},
    {"notifications", "notifications_station_battle", "notifications_station_battle", StationBattle,
     DCN::Battle::station_battle, "", "Notify for station battle toasts."},
    {"notifications", "notifications_incoming_attack_player", "notifications_incoming_attack_player", IncomingAttack,
     DCN::Battle::incoming_attack_player, "notifications_incoming_attack", "Notify for incoming player attack toasts."},
    {"notifications", "notifications_incoming_attack_hostile", "notifications_incoming_attack_hostile",
     IncomingAttackFaction, DCN::Battle::incoming_attack_hostile, "notifications_incoming_attack_faction",
     "Notify for incoming hostile attack toasts."},
    {"notifications", "notifications_fleet_battle", "notifications_fleet_battle", FleetBattle,
     DCN::Battle::fleet_battle, "", "Notify for fleet battle toasts."},
    {"notifications", "notifications_armada_battle_won", "notifications_armada_battle_won", ArmadaBattleWon,
     DCN::Battle::armada_battle_won, "", "Notify for armada victory toasts."},
    {"notifications", "notifications_armada_battle_lost", "notifications_armada_battle_lost", ArmadaBattleLost,
     DCN::Battle::armada_battle_lost, "", "Notify for armada defeat toasts."},
    {"notifications", "notifications_assault_victory", "notifications_assault_victory", AssaultVictory,
     DCN::Battle::assault_victory, "", "Notify for assault victory toasts."},
    {"notifications", "notifications_assault_defeat", "notifications_assault_defeat", AssaultDefeat,
     DCN::Battle::assault_defeat, "", "Notify for assault defeat toasts."},
    {"notifications", "notifications_armada_created", "notifications_armada_created", ArmadaCreated,
     DCN::Armada::created, "", "Notify for armada created toasts."},
    {"notifications", "notifications_armada_canceled", "notifications_armada_canceled", ArmadaCanceled,
     DCN::Armada::canceled, "", "Notify for armada canceled toasts."},
    {"notifications", "notifications_tournament", "notifications_tournament", Tournament, DCN::Events::tournament, "",
     "Notify for tournament progress toasts."},
    {"notifications", "notifications_chained_event_scored", "notifications_chained_event_scored", ChainedEventScored,
     DCN::Events::chained_event_scored, "", "Notify for chained event score toasts."},
    {"notifications", "notifications_standard", "notifications_standard", Standard, DCN::Experimental::standard, "",
     "Notify for standard toasts."},
    {"notifications", "notifications_faction_warning", "notifications_faction_warning", FactionWarning,
     DCN::Experimental::faction_warning, "", "Notify for faction warning toasts."},
    {"notifications", "notifications_faction_level_up", "notifications_faction_level_up", FactionLevelUp,
     DCN::Experimental::faction_level_up, "", "Notify for faction level-up toasts."},
    {"notifications", "notifications_faction_level_down", "notifications_faction_level_down", FactionLevelDown,
     DCN::Experimental::faction_level_down, "", "Notify for faction level-down toasts."},
    {"notifications", "notifications_faction_discovered", "notifications_faction_discovered", FactionDiscovered,
     DCN::Experimental::faction_discovered, "", "Notify for faction discovery toasts."},
    {"notifications", "notifications_armada_incoming_attack", "notifications_armada_incoming_attack",
     ArmadaIncomingAttack, DCN::Experimental::armada_incoming_attack, "", "Notify for armada incoming attack toasts."},
    {"notifications", "notifications_diplomacy_updated", "notifications_diplomacy_updated", DiplomacyUpdated,
     DCN::Experimental::diplomacy_updated, "", "Notify for diplomacy update toasts."},
    {"notifications", "notifications_joined_takeover", "notifications_joined_takeover", JoinedTakeover,
     DCN::Experimental::joined_takeover, "", "Notify for joined takeover toasts."},
    {"notifications", "notifications_competitor_joined_takeover", "notifications_competitor_joined_takeover",
     CompetitorJoinedTakeover, DCN::Experimental::competitor_joined_takeover, "",
     "Notify for competitor joined takeover toasts."},
    {"notifications", "notifications_abandoned_territory", "notifications_abandoned_territory", AbandonedTerritory,
     DCN::Experimental::abandoned_territory, "", "Notify for abandoned territory toasts."},
    {"notifications", "notifications_takeover_victory", "notifications_takeover_victory", TakeoverVictory,
     DCN::Experimental::takeover_victory, "", "Notify for takeover victory toasts."},
    {"notifications", "notifications_takeover_defeat", "notifications_takeover_defeat", TakeoverDefeat,
     DCN::Experimental::takeover_defeat, "", "Notify for takeover defeat toasts."},
    {"notifications", "notifications_treasury_progress", "notifications_treasury_progress", TreasuryProgress,
     DCN::Experimental::treasury_progress, "", "Notify for treasury progress toasts."},
    {"notifications", "notifications_treasury_full", "notifications_treasury_full", TreasuryFull,
     DCN::Experimental::treasury_full, "", "Notify for treasury full toasts."},
    {"notifications", "notifications_achievement", "notifications_achievement", Achievement,
     DCN::Experimental::achievement, "", "Notify for achievement toasts."},
    {"notifications", "notifications_challenge_complete", "notifications_challenge_complete", ChallengeComplete,
     DCN::Experimental::challenge_complete, "", "Notify for challenge complete toasts."},
    {"notifications", "notifications_challenge_failed", "notifications_challenge_failed", ChallengeFailed,
     DCN::Experimental::challenge_failed, "", "Notify for challenge failed toasts."},
    {"notifications", "notifications_strike_hit", "notifications_strike_hit", StrikeHit, DCN::Experimental::strike_hit,
     "", "Notify for strike hit toasts."},
    {"notifications", "notifications_strike_defeat", "notifications_strike_defeat", StrikeDefeat,
     DCN::Experimental::strike_defeat, "", "Notify for strike defeat toasts."},
    {"notifications", "notifications_warchest_progress", "notifications_warchest_progress", WarchestProgress,
     DCN::Experimental::warchest_progress, "", "Notify for warchest progress toasts."},
    {"notifications", "notifications_warchest_full", "notifications_warchest_full", WarchestFull,
     DCN::Experimental::warchest_full, "", "Notify for warchest full toasts."},
    {"notifications", "notifications_arena_time_left", "notifications_arena_time_left", ArenaTimeLeft,
     DCN::Experimental::arena_time_left, "", "Notify for arena time warning toasts."},
    {"notifications", "notifications_fleet_preset_applied", "notifications_fleet_preset_applied", FleetPresetApplied,
     DCN::Experimental::fleet_preset_applied, "", "Notify for fleet preset applied toasts."},
    {"notifications", "notifications_surge_warmup_ended", "notifications_surge_warmup_ended", SurgeWarmUpEnded,
     DCN::Experimental::surge_warmup_ended, "", "Notify for surge warmup ended toasts."},
    {"notifications", "notifications_surge_hostile_group_defeated", "notifications_surge_hostile_group_defeated",
     SurgeHostileGroupDefeated, DCN::Experimental::surge_hostile_group_defeated, "",
     "Notify for surge hostile group defeated toasts."},
    {"notifications", "notifications_surge_time_left", "notifications_surge_time_left", SurgeTimeLeft,
     DCN::Experimental::surge_time_left, "", "Notify for surge time warning toasts."},
};

} // namespace config_metadata
