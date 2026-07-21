/**
 * @file notification_policy.h
 * @brief Load-time notification delivery policy matrix.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <toml++/toml.h>

class NotificationConfig;

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

struct NotificationPolicy {
  bool              system = false;
  bool              audio  = false;
  NotificationSound sound  = NotificationSound::Default;
};

inline constexpr size_t kNotificationKindCount = static_cast<size_t>(NotificationKind::Count);

void notification_policy_load(const toml::table& config, toml::table& runtime_config,
                              const NotificationConfig& legacy_notifications);

[[nodiscard]] const NotificationPolicy& notification_policy_for(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_has_delivery(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_system_enabled(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_audio_enabled(NotificationKind kind);
[[nodiscard]] std::optional<NotificationKind> notification_kind_from_toast_state(int state);
[[nodiscard]] const char*                    notification_kind_name(NotificationKind kind);
[[nodiscard]] const char*                    notification_sound_name(NotificationSound sound);
[[nodiscard]] std::optional<NotificationSound> notification_sound_from_name(std::string_view name);
