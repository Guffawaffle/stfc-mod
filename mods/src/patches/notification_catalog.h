/**
 * @file notification_catalog.h
 * @brief Authoritative notification event names, legacy paths, toast states, and sounds.
 */
#pragma once

#include "toast_state.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

enum class NotificationKind : uint16_t {
  BattleVictory = 0,
  BattleDefeat,
  BattlePartialVictory,
  BattleStationVictory,
  BattleStationDefeat,
  BattleStationBattle,
  BattleIncomingAttackPlayer,
  BattleIncomingAttackHostile,
  BattleFleetBattle,
  BattleArmadaBattleWon,
  BattleArmadaBattleLost,
  BattleAssaultVictory,
  BattleAssaultDefeat,
  ArmadaCreated,
  ArmadaCanceled,
  EventTournament,
  EventChainedEventScored,
  ExperimentalStandard,
  ExperimentalFactionWarning,
  ExperimentalFactionLevelUp,
  ExperimentalFactionLevelDown,
  ExperimentalFactionDiscovered,
  ExperimentalArmadaIncomingAttack,
  ExperimentalDiplomacyUpdated,
  ExperimentalJoinedTakeover,
  ExperimentalCompetitorJoinedTakeover,
  ExperimentalAbandonedTerritory,
  ExperimentalTakeoverVictory,
  ExperimentalTakeoverDefeat,
  ExperimentalTreasuryProgress,
  ExperimentalTreasuryFull,
  ExperimentalAchievement,
  ExperimentalChallengeComplete,
  ExperimentalChallengeFailed,
  ExperimentalStrikeHit,
  ExperimentalStrikeDefeat,
  ExperimentalWarchestProgress,
  ExperimentalWarchestFull,
  ExperimentalArenaTimeLeft,
  ExperimentalFleetPresetApplied,
  ExperimentalSurgeWarmupEnded,
  ExperimentalSurgeHostileGroupDefeated,
  ExperimentalSurgeTimeLeft,
  ExperimentalQueueForLeaseActivated,
  ExperimentalQueueForLeaseExpired,
  ExperimentalPermanentQueuePurchased,
  ExperimentalOutpostStartedOrEnded,
  ExperimentalCrossAllianceArmadaVictory,
  ExperimentalCrossAllianceArmadaDefeat,
  ExperimentalCrossAllianceArmadaPartialVictory,
  ExperimentalFactionWeeklyEventsProgress,
  ExperimentalFactionWeeklyEventsComplete,
  ExperimentalArmadaPlayerBlocked,
  ExperimentalArmadaPlayerUnblocked,
  ExperimentalDynamicCrisisUpdate,
  ExperimentalDynamicCrisisFailed,
  ExperimentalDynamicCrisisCompleted,
  ExperimentalGalacticAnomalySystemEntered,
  FleetArrivedInSystem,
  FleetArrivedAtDestination,
  FleetStartedMining,
  FleetNodeDepleted,
  FleetDocked,
  FleetRepairComplete,
  Count,
};

enum class NotificationSound : uint8_t {
  None = 0,
  Default,
  Info,
  Success,
  Warning,
  Alarm,
  Arrival,
  Soft,
  Ping,
  Repair,
  Count,
};

struct NotificationEventCatalogEntry {
  NotificationKind  kind;
  std::string_view  canonical_key;
  std::string_view  runtime_name;
  std::string_view  legacy_category;
  std::string_view  legacy_key;
  int               toast_state;
  NotificationSound catalog_sound;
};

inline constexpr std::array kNotificationEventCatalog{
    NotificationEventCatalogEntry{NotificationKind::BattleVictory, "victory", "battle.victory", "battle", "victory",
                                  Victory, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::BattleDefeat, "defeat", "battle.defeat", "battle", "defeat", Defeat,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::BattlePartialVictory, "partial_victory", "battle.partial_victory",
                                  "battle", "partial_victory", PartialVictory, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::BattleStationVictory, "station_victory", "battle.station_victory",
                                  "battle", "station_victory", StationVictory, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::BattleStationDefeat, "station_defeat", "battle.station_defeat",
                                  "battle", "station_defeat", StationDefeat, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::BattleStationBattle, "station_battle", "battle.station_battle",
                                  "battle", "station_battle", StationBattle, NotificationSound::Alarm},
    NotificationEventCatalogEntry{NotificationKind::BattleIncomingAttackPlayer, "incoming_attack_player",
                                  "battle.incoming_attack_player", "battle", "incoming_attack_player", IncomingAttack,
                                  NotificationSound::Alarm},
    NotificationEventCatalogEntry{NotificationKind::BattleIncomingAttackHostile, "incoming_attack_hostile",
                                  "battle.incoming_attack_hostile", "battle", "incoming_attack_hostile",
                                  IncomingAttackFaction, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::BattleFleetBattle, "fleet_battle", "battle.fleet_battle", "battle",
                                  "fleet_battle", FleetBattle, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::BattleArmadaBattleWon, "armada_battle_won",
                                  "battle.armada_battle_won", "battle", "armada_battle_won", ArmadaBattleWon,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::BattleArmadaBattleLost, "armada_battle_lost",
                                  "battle.armada_battle_lost", "battle", "armada_battle_lost", ArmadaBattleLost,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::BattleAssaultVictory, "assault_victory", "battle.assault_victory",
                                  "battle", "assault_victory", AssaultVictory, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::BattleAssaultDefeat, "assault_defeat", "battle.assault_defeat",
                                  "battle", "assault_defeat", AssaultDefeat, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ArmadaCreated, "armada_created", "armada.created", "armada",
                                  "created", ArmadaCreated, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ArmadaCanceled, "armada_canceled", "armada.canceled", "armada",
                                  "canceled", ArmadaCanceled, NotificationSound::Soft},
    NotificationEventCatalogEntry{NotificationKind::EventTournament, "tournament", "event.tournament", "event",
                                  "tournament", Tournament, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::EventChainedEventScored, "chained_event_scored",
                                  "event.chained_event_scored", "event", "chained_event_scored", ChainedEventScored,
                                  NotificationSound::Ping},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalStandard, "standard", "experimental.standard",
                                  "experimental", "standard", Standard, NotificationSound::Default},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionWarning, "faction_warning",
                                  "experimental.faction_warning", "experimental", "faction_warning", FactionWarning,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionLevelUp, "faction_level_up",
                                  "experimental.faction_level_up", "experimental", "faction_level_up", FactionLevelUp,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionLevelDown, "faction_level_down",
                                  "experimental.faction_level_down", "experimental", "faction_level_down",
                                  FactionLevelDown, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionDiscovered, "faction_discovered",
                                  "experimental.faction_discovered", "experimental", "faction_discovered",
                                  FactionDiscovered, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalArmadaIncomingAttack, "armada_incoming_attack",
                                  "experimental.armada_incoming_attack", "experimental", "armada_incoming_attack",
                                  ArmadaIncomingAttack, NotificationSound::Alarm},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalDiplomacyUpdated, "diplomacy_updated",
                                  "experimental.diplomacy_updated", "experimental", "diplomacy_updated",
                                  DiplomacyUpdated, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalJoinedTakeover, "joined_takeover",
                                  "experimental.joined_takeover", "experimental", "joined_takeover", JoinedTakeover,
                                  NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalCompetitorJoinedTakeover, "competitor_joined_takeover",
                                  "experimental.competitor_joined_takeover", "experimental",
                                  "competitor_joined_takeover", CompetitorJoinedTakeover, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalAbandonedTerritory, "abandoned_territory",
                                  "experimental.abandoned_territory", "experimental", "abandoned_territory",
                                  AbandonedTerritory, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalTakeoverVictory, "takeover_victory",
                                  "experimental.takeover_victory", "experimental", "takeover_victory", TakeoverVictory,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalTakeoverDefeat, "takeover_defeat",
                                  "experimental.takeover_defeat", "experimental", "takeover_defeat", TakeoverDefeat,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalTreasuryProgress, "treasury_progress",
                                  "experimental.treasury_progress", "experimental", "treasury_progress",
                                  TreasuryProgress, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalTreasuryFull, "treasury_full",
                                  "experimental.treasury_full", "experimental", "treasury_full", TreasuryFull,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalAchievement, "achievement", "experimental.achievement",
                                  "experimental", "achievement", Achievement, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalChallengeComplete, "challenge_complete",
                                  "experimental.challenge_complete", "experimental", "challenge_complete",
                                  ChallengeComplete, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalChallengeFailed, "challenge_failed",
                                  "experimental.challenge_failed", "experimental", "challenge_failed", ChallengeFailed,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalStrikeHit, "strike_hit", "experimental.strike_hit",
                                  "experimental", "strike_hit", StrikeHit, NotificationSound::Ping},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalStrikeDefeat, "strike_defeat",
                                  "experimental.strike_defeat", "experimental", "strike_defeat", StrikeDefeat,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalWarchestProgress, "warchest_progress",
                                  "experimental.warchest_progress", "experimental", "warchest_progress",
                                  WarchestProgress, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalWarchestFull, "warchest_full",
                                  "experimental.warchest_full", "experimental", "warchest_full", WarchestFull,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalArenaTimeLeft, "arena_time_left",
                                  "experimental.arena_time_left", "experimental", "arena_time_left", ArenaTimeLeft,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFleetPresetApplied, "fleet_preset_applied",
                                  "experimental.fleet_preset_applied", "experimental", "fleet_preset_applied",
                                  FleetPresetApplied, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalSurgeWarmupEnded, "surge_warmup_ended",
                                  "experimental.surge_warmup_ended", "experimental", "surge_warmup_ended",
                                  SurgeWarmUpEnded, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalSurgeHostileGroupDefeated,
                                  "surge_hostile_group_defeated", "experimental.surge_hostile_group_defeated",
                                  "experimental", "surge_hostile_group_defeated", SurgeHostileGroupDefeated,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalSurgeTimeLeft, "surge_time_left",
                                  "experimental.surge_time_left", "experimental", "surge_time_left", SurgeTimeLeft,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalQueueForLeaseActivated, "queue_for_lease_activated",
                                  "experimental.queue_for_lease_activated", "experimental", "queue_for_lease_activated",
                                  QueueForLeaseActivated, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalQueueForLeaseExpired, "queue_for_lease_expired",
                                  "experimental.queue_for_lease_expired", "experimental", "queue_for_lease_expired",
                                  QueueForLeaseExpired, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalPermanentQueuePurchased, "permanent_queue_purchased",
                                  "experimental.permanent_queue_purchased", "experimental", "permanent_queue_purchased",
                                  PermanentQueuePurchased, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalOutpostStartedOrEnded, "outpost_started_or_ended",
                                  "experimental.outpost_started_or_ended", "experimental", "outpost_started_or_ended",
                                  OutpostStartedOrEnded, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalCrossAllianceArmadaVictory,
                                  "cross_alliance_armada_victory", "experimental.cross_alliance_armada_victory",
                                  "experimental", "cross_alliance_armada_victory", CrossAllianceArmadaVictory,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalCrossAllianceArmadaDefeat,
                                  "cross_alliance_armada_defeat", "experimental.cross_alliance_armada_defeat",
                                  "experimental", "cross_alliance_armada_defeat", CrossAllianceArmadaDefeat,
                                  NotificationSound::Warning},
    NotificationEventCatalogEntry{
        NotificationKind::ExperimentalCrossAllianceArmadaPartialVictory, "cross_alliance_armada_partial_victory",
        "experimental.cross_alliance_armada_partial_victory", "experimental", "cross_alliance_armada_partial_victory",
        CrossAllianceArmadaPartialVictory, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionWeeklyEventsProgress,
                                  "faction_weekly_events_progress", "experimental.faction_weekly_events_progress",
                                  "experimental", "faction_weekly_events_progress", FactionWeeklyEventsProgress,
                                  NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalFactionWeeklyEventsComplete,
                                  "faction_weekly_events_complete", "experimental.faction_weekly_events_complete",
                                  "experimental", "faction_weekly_events_complete", FactionWeeklyEventsComplete,
                                  NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalArmadaPlayerBlocked, "armada_player_blocked",
                                  "experimental.armada_player_blocked", "experimental", "armada_player_blocked",
                                  ArmadaPlayerBlocked, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalArmadaPlayerUnblocked, "armada_player_unblocked",
                                  "experimental.armada_player_unblocked", "experimental", "armada_player_unblocked",
                                  ArmadaPlayerUnblocked, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalDynamicCrisisUpdate, "dynamic_crisis_update",
                                  "experimental.dynamic_crisis_update", "experimental", "dynamic_crisis_update",
                                  DynamicCrisisUpdate, NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalDynamicCrisisFailed, "dynamic_crisis_failed",
                                  "experimental.dynamic_crisis_failed", "experimental", "dynamic_crisis_failed",
                                  DynamicCrisisFailed, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalDynamicCrisisCompleted, "dynamic_crisis_completed",
                                  "experimental.dynamic_crisis_completed", "experimental", "dynamic_crisis_completed",
                                  DynamicCrisisCompleted, NotificationSound::Success},
    NotificationEventCatalogEntry{NotificationKind::ExperimentalGalacticAnomalySystemEntered,
                                  "galactic_anomaly_system_entered", "experimental.galactic_anomaly_system_entered",
                                  "experimental", "galactic_anomaly_system_entered", GalacticAnomalySystemEntered,
                                  NotificationSound::Info},
    NotificationEventCatalogEntry{NotificationKind::FleetArrivedInSystem, "fleet_arrived_in_system",
                                  "fleet.arrived_in_system", "fleet", "arrived_in_system", -1,
                                  NotificationSound::Arrival},
    NotificationEventCatalogEntry{NotificationKind::FleetArrivedAtDestination, "fleet_arrived_at_destination",
                                  "fleet.arrived_at_destination", "fleet", "arrived_at_destination", -1,
                                  NotificationSound::Soft},
    NotificationEventCatalogEntry{NotificationKind::FleetStartedMining, "fleet_started_mining", "fleet.started_mining",
                                  "fleet", "started_mining", -1, NotificationSound::Ping},
    NotificationEventCatalogEntry{NotificationKind::FleetNodeDepleted, "fleet_node_depleted", "fleet.node_depleted",
                                  "fleet", "node_depleted", -1, NotificationSound::Warning},
    NotificationEventCatalogEntry{NotificationKind::FleetDocked, "fleet_docked", "fleet.docked", "fleet", "docked", -1,
                                  NotificationSound::Soft},
    NotificationEventCatalogEntry{NotificationKind::FleetRepairComplete, "fleet_repair_complete",
                                  "fleet.repair_complete", "fleet", "repair_complete", -1, NotificationSound::Repair},
};

inline constexpr size_t kNotificationKindCount = static_cast<size_t>(NotificationKind::Count);
static_assert(kNotificationEventCatalog.size() == kNotificationKindCount);
static_assert([] {
  for (size_t index = 0; index < kNotificationEventCatalog.size(); ++index) {
    if (static_cast<size_t>(kNotificationEventCatalog[index].kind) != index) {
      return false;
    }
  }
  return true;
}());

// The complete catalog remains available for compatibility and private
// experiments, but fresh/generated configs advertise only this explicitly
// release-supported surface.
inline constexpr std::array kPublicNotificationKinds{
    NotificationKind::BattleVictory,         NotificationKind::BattleDefeat,
    NotificationKind::BattleArmadaBattleWon, NotificationKind::BattleArmadaBattleLost,
    NotificationKind::ArmadaCreated,         NotificationKind::FleetArrivedInSystem,
    NotificationKind::FleetNodeDepleted,     NotificationKind::FleetRepairComplete,
};

static_assert([] {
  for (size_t index = 0; index < kPublicNotificationKinds.size(); ++index) {
    if (static_cast<size_t>(kPublicNotificationKinds[index]) >= kNotificationKindCount) {
      return false;
    }
    for (size_t other = index + 1; other < kPublicNotificationKinds.size(); ++other) {
      if (kPublicNotificationKinds[index] == kPublicNotificationKinds[other]) {
        return false;
      }
    }
  }
  return true;
}());

inline constexpr std::span<const NotificationEventCatalogEntry> notification_event_catalog()
{ return kNotificationEventCatalog; }

inline constexpr std::span<const NotificationKind> public_notification_kinds()
{ return kPublicNotificationKinds; }

inline constexpr const NotificationEventCatalogEntry* notification_catalog_entry(const NotificationKind kind)
{
  const auto index = static_cast<size_t>(kind);
  return index < kNotificationEventCatalog.size() ? &kNotificationEventCatalog[index] : nullptr;
}
