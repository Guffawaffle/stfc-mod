#include "patches/notification_policy.h"

#include "config.h"
#include "toast_state.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace
{
struct NotificationEventSpec {
  NotificationKind kind;
  std::string_view category;
  std::string_view key;
  int              toast_state = -1;
  bool NotificationConfig::* legacy_system_member = nullptr;
  bool NotificationConfig::* legacy_audio_member  = nullptr;
  NotificationSound default_sound = NotificationSound::Default;
};

std::array<NotificationPolicy, kNotificationKindCount> s_notification_policy{};

constexpr std::array kEventSpecs{
    NotificationEventSpec{NotificationKind::BattleVictory, "battle", "victory", Victory, nullptr, nullptr,
                          NotificationSound::Success},
    NotificationEventSpec{NotificationKind::BattleDefeat, "battle", "defeat", Defeat, nullptr, nullptr,
                          NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::BattlePartialVictory, "battle", "partial_victory", PartialVictory,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::BattleStationVictory, "battle", "station_victory", StationVictory,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::BattleStationDefeat, "battle", "station_defeat", StationDefeat, nullptr,
                          nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::BattleStationBattle, "battle", "station_battle", StationBattle, nullptr,
                          nullptr, NotificationSound::Alarm},
    NotificationEventSpec{NotificationKind::BattleIncomingAttackPlayer, "battle", "incoming_attack_player",
                          IncomingAttack, &NotificationConfig::incoming_attack_player, nullptr,
                          NotificationSound::Alarm},
    NotificationEventSpec{NotificationKind::BattleIncomingAttackHostile, "battle", "incoming_attack_hostile",
                          IncomingAttackFaction, &NotificationConfig::incoming_attack_hostile, nullptr,
                          NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::BattleFleetBattle, "battle", "fleet_battle", FleetBattle, nullptr,
                          nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::BattleArmadaBattleWon, "battle", "armada_battle_won", ArmadaBattleWon,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::BattleArmadaBattleLost, "battle", "armada_battle_lost", ArmadaBattleLost,
                          nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::BattleAssaultVictory, "battle", "assault_victory", AssaultVictory,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::BattleAssaultDefeat, "battle", "assault_defeat", AssaultDefeat, nullptr,
                          nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ArmadaCreated, "armada", "created", ArmadaCreated, nullptr, nullptr,
                          NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ArmadaCanceled, "armada", "canceled", ArmadaCanceled, nullptr, nullptr,
                          NotificationSound::Soft},
    NotificationEventSpec{NotificationKind::EventTournament, "event", "tournament", Tournament, nullptr, nullptr,
                          NotificationSound::Info},
    NotificationEventSpec{NotificationKind::EventChainedEventScored, "event", "chained_event_scored",
                          ChainedEventScored, nullptr, nullptr, NotificationSound::Ping},
    NotificationEventSpec{NotificationKind::ExperimentalStandard, "experimental", "standard", Standard, nullptr,
                          nullptr, NotificationSound::Default},
    NotificationEventSpec{NotificationKind::ExperimentalFactionWarning, "experimental", "faction_warning",
                          FactionWarning, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalFactionLevelUp, "experimental", "faction_level_up",
                          FactionLevelUp, nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalFactionLevelDown, "experimental", "faction_level_down",
                          FactionLevelDown, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalFactionDiscovered, "experimental", "faction_discovered",
                          FactionDiscovered, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalArmadaIncomingAttack, "experimental", "armada_incoming_attack",
                          ArmadaIncomingAttack, nullptr, nullptr, NotificationSound::Alarm},
    NotificationEventSpec{NotificationKind::ExperimentalDiplomacyUpdated, "experimental", "diplomacy_updated",
                          DiplomacyUpdated, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalJoinedTakeover, "experimental", "joined_takeover",
                          JoinedTakeover, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalCompetitorJoinedTakeover, "experimental",
                          "competitor_joined_takeover", CompetitorJoinedTakeover, nullptr, nullptr,
                          NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalAbandonedTerritory, "experimental", "abandoned_territory",
                          AbandonedTerritory, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalTakeoverVictory, "experimental", "takeover_victory",
                          TakeoverVictory, nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalTakeoverDefeat, "experimental", "takeover_defeat",
                          TakeoverDefeat, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalTreasuryProgress, "experimental", "treasury_progress",
                          TreasuryProgress, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalTreasuryFull, "experimental", "treasury_full", TreasuryFull,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalAchievement, "experimental", "achievement", Achievement,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalChallengeComplete, "experimental", "challenge_complete",
                          ChallengeComplete, nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalChallengeFailed, "experimental", "challenge_failed",
                          ChallengeFailed, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalStrikeHit, "experimental", "strike_hit", StrikeHit, nullptr,
                          nullptr, NotificationSound::Ping},
    NotificationEventSpec{NotificationKind::ExperimentalStrikeDefeat, "experimental", "strike_defeat", StrikeDefeat,
                          nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalWarchestProgress, "experimental", "warchest_progress",
                          WarchestProgress, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalWarchestFull, "experimental", "warchest_full", WarchestFull,
                          nullptr, nullptr, NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalArenaTimeLeft, "experimental", "arena_time_left",
                          ArenaTimeLeft, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::ExperimentalFleetPresetApplied, "experimental", "fleet_preset_applied",
                          FleetPresetApplied, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalSurgeWarmupEnded, "experimental", "surge_warmup_ended",
                          SurgeWarmUpEnded, nullptr, nullptr, NotificationSound::Info},
    NotificationEventSpec{NotificationKind::ExperimentalSurgeHostileGroupDefeated, "experimental",
                          "surge_hostile_group_defeated", SurgeHostileGroupDefeated, nullptr, nullptr,
                          NotificationSound::Success},
    NotificationEventSpec{NotificationKind::ExperimentalSurgeTimeLeft, "experimental", "surge_time_left",
                          SurgeTimeLeft, nullptr, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::FleetArrivedInSystem, "fleet", "arrived_in_system", -1,
                          &NotificationConfig::fleet_arrived_in_system,
                          &NotificationConfig::audio_fleet_arrived_in_system, NotificationSound::Arrival},
    NotificationEventSpec{NotificationKind::FleetArrivedAtDestination, "fleet", "arrived_at_destination", -1,
                          &NotificationConfig::fleet_arrived_at_destination, nullptr, NotificationSound::Soft},
    NotificationEventSpec{NotificationKind::FleetStartedMining, "fleet", "started_mining", -1,
                          &NotificationConfig::fleet_started_mining, nullptr, NotificationSound::Ping},
    NotificationEventSpec{NotificationKind::FleetNodeDepleted, "fleet", "node_depleted", -1,
                          &NotificationConfig::fleet_node_depleted, nullptr, NotificationSound::Warning},
    NotificationEventSpec{NotificationKind::FleetDocked, "fleet", "docked", -1, &NotificationConfig::fleet_docked,
                          nullptr, NotificationSound::Soft},
    NotificationEventSpec{NotificationKind::FleetRepairComplete, "fleet", "repair_complete", -1,
                          &NotificationConfig::fleet_repair_complete, nullptr, NotificationSound::Repair},
};

static_assert(kEventSpecs.size() == kNotificationKindCount);

std::string normalize_sound_name(std::string_view value)
{
  std::string normalized;
  normalized.reserve(value.size());

  for (const auto ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    if (std::isspace(byte) || ch == '-') {
      normalized.push_back('_');
    } else {
      normalized.push_back(static_cast<char>(std::tolower(byte)));
    }
  }

  return normalized;
}

const toml::table* nested_table(const toml::table& table, std::initializer_list<std::string_view> path)
{
  const toml::table* current = &table;
  for (const auto segment : path) {
    const auto* node = current->get(segment);
    if (!node) {
      return nullptr;
    }

    current = node->as_table();
    if (!current) {
      return nullptr;
    }
  }

  return current;
}

toml::table* ensure_table(toml::table& table, std::initializer_list<std::string_view> path)
{
  auto* current = &table;
  for (const auto segment_view : path) {
    const auto segment = std::string(segment_view);
    auto*      node    = current->get(segment);
    if (!node || !node->is_table()) {
      current->insert_or_assign(segment, toml::table{});
      node = current->get(segment);
    }

    current = node ? node->as_table() : nullptr;
    if (!current) {
      return nullptr;
    }
  }

  return current;
}

std::string event_path(const NotificationEventSpec& spec)
{ return "notifications.events." + std::string(spec.category) + "." + std::string(spec.key); }

bool default_system_enabled(const NotificationConfig& legacy, const NotificationEventSpec& spec)
{
  if (spec.legacy_system_member) {
    return legacy.*(spec.legacy_system_member);
  }

  if (spec.toast_state >= 0) {
    return legacy.EnabledForToastState(spec.toast_state);
  }

  return false;
}

bool default_audio_enabled(const NotificationConfig& legacy, const NotificationEventSpec& spec)
{ return spec.legacy_audio_member ? legacy.*(spec.legacy_audio_member) : false; }

std::optional<bool> read_bool_field(const toml::table& table, std::string_view field, std::string_view path)
{
  const auto* node = table.get(field);
  if (!node) {
    return std::nullopt;
  }

  if (const auto value = node->value<bool>(); value.has_value()) {
    return value.value();
  }

  spdlog::warn("[NotifyPolicy] Invalid boolean {}.{}; keeping default", path, field);
  return std::nullopt;
}

std::optional<NotificationSound> read_sound_field(const toml::table& table, std::string_view field,
                                                  std::string_view path)
{
  const auto* node = table.get(field);
  if (!node) {
    return std::nullopt;
  }

  const auto value = node->value<std::string>();
  if (!value.has_value()) {
    spdlog::warn("[NotifyPolicy] Invalid string {}.{}; keeping default", path, field);
    return std::nullopt;
  }

  const auto parsed = notification_sound_from_name(value.value());
  if (!parsed.has_value()) {
    spdlog::warn("[NotifyPolicy] Unknown notification sound '{}' at {}.{}; keeping default", value.value(), path,
                 field);
    return std::nullopt;
  }

  return parsed;
}

NotificationSound read_default_sound(const toml::table& config)
{
  const auto* audio = nested_table(config, {"notifications", "audio"});
  if (!audio) {
    return NotificationSound::Default;
  }

  return read_sound_field(*audio, "default_sound", "notifications.audio").value_or(NotificationSound::Default);
}

NotificationPolicy load_event_policy(const toml::table& config, const NotificationConfig& legacy,
                                     const NotificationEventSpec& spec, NotificationSound default_sound)
{
  NotificationPolicy policy{
      default_system_enabled(legacy, spec),
      default_audio_enabled(legacy, spec),
      spec.default_sound == NotificationSound::Default ? default_sound : spec.default_sound,
  };

  const auto* category_table = nested_table(config, {"notifications", "events", spec.category});
  if (!category_table) {
    return policy;
  }

  const auto* node = category_table->get(spec.key);
  if (!node) {
    return policy;
  }

  const auto* table = node->as_table();
  const auto  path  = event_path(spec);
  if (!table) {
    spdlog::warn("[NotifyPolicy] Expected inline table at {}; keeping legacy/default policy", path);
    return policy;
  }

  policy.system = read_bool_field(*table, "system", path).value_or(policy.system);
  policy.audio  = read_bool_field(*table, "audio", path).value_or(policy.audio);
  policy.sound  = read_sound_field(*table, "sound", path).value_or(policy.sound);
  return policy;
}

void write_runtime_event_policy(toml::table& runtime_config, const NotificationEventSpec& spec,
                                const NotificationPolicy& policy)
{
  auto* category = ensure_table(runtime_config, {"notifications", "events", spec.category});
  if (!category) {
    return;
  }

  toml::table entry;
  entry.insert_or_assign("system", policy.system);
  entry.insert_or_assign("audio", policy.audio);
  entry.insert_or_assign("sound", notification_sound_name(policy.sound));
  entry.is_inline(true);
  category->insert_or_assign(std::string(spec.key), std::move(entry));
}
} // namespace

void notification_policy_load(const toml::table& config, toml::table& runtime_config,
                              const NotificationConfig& legacy_notifications)
{
  const auto default_sound = read_default_sound(config);
  auto*      audio         = ensure_table(runtime_config, {"notifications", "audio"});
  if (audio) {
    audio->insert_or_assign("default_sound", notification_sound_name(default_sound));
  }

  for (const auto& spec : kEventSpecs) {
    const auto index = static_cast<size_t>(spec.kind);
    s_notification_policy[index] = load_event_policy(config, legacy_notifications, spec, default_sound);
    write_runtime_event_policy(runtime_config, spec, s_notification_policy[index]);
  }
}

const NotificationPolicy& notification_policy_for(NotificationKind kind)
{
  const auto index = static_cast<size_t>(kind);
  if (index >= s_notification_policy.size()) {
    static constexpr NotificationPolicy disabled{};
    return disabled;
  }

  return s_notification_policy[index];
}

bool notification_policy_has_delivery(NotificationKind kind)
{
  const auto& policy = notification_policy_for(kind);
  return policy.system || (policy.audio && policy.sound != NotificationSound::None);
}

bool notification_policy_system_enabled(NotificationKind kind)
{ return notification_policy_for(kind).system; }

bool notification_policy_audio_enabled(NotificationKind kind)
{
  const auto& policy = notification_policy_for(kind);
  return policy.audio && policy.sound != NotificationSound::None;
}

std::optional<NotificationKind> notification_kind_from_toast_state(int state)
{
  switch (state) {
    case Victory: return NotificationKind::BattleVictory;
    case Defeat: return NotificationKind::BattleDefeat;
    case PartialVictory: return NotificationKind::BattlePartialVictory;
    case StationVictory: return NotificationKind::BattleStationVictory;
    case StationDefeat: return NotificationKind::BattleStationDefeat;
    case StationBattle: return NotificationKind::BattleStationBattle;
    case IncomingAttack: return NotificationKind::BattleIncomingAttackPlayer;
    case IncomingAttackFaction: return NotificationKind::BattleIncomingAttackHostile;
    case FleetBattle: return NotificationKind::BattleFleetBattle;
    case ArmadaBattleWon: return NotificationKind::BattleArmadaBattleWon;
    case ArmadaBattleLost: return NotificationKind::BattleArmadaBattleLost;
    case AssaultVictory: return NotificationKind::BattleAssaultVictory;
    case AssaultDefeat: return NotificationKind::BattleAssaultDefeat;
    case ArmadaCreated: return NotificationKind::ArmadaCreated;
    case ArmadaCanceled: return NotificationKind::ArmadaCanceled;
    case Tournament: return NotificationKind::EventTournament;
    case ChainedEventScored: return NotificationKind::EventChainedEventScored;
    case Standard: return NotificationKind::ExperimentalStandard;
    case FactionWarning: return NotificationKind::ExperimentalFactionWarning;
    case FactionLevelUp: return NotificationKind::ExperimentalFactionLevelUp;
    case FactionLevelDown: return NotificationKind::ExperimentalFactionLevelDown;
    case FactionDiscovered: return NotificationKind::ExperimentalFactionDiscovered;
    case ArmadaIncomingAttack: return NotificationKind::ExperimentalArmadaIncomingAttack;
    case DiplomacyUpdated: return NotificationKind::ExperimentalDiplomacyUpdated;
    case JoinedTakeover: return NotificationKind::ExperimentalJoinedTakeover;
    case CompetitorJoinedTakeover: return NotificationKind::ExperimentalCompetitorJoinedTakeover;
    case AbandonedTerritory: return NotificationKind::ExperimentalAbandonedTerritory;
    case TakeoverVictory: return NotificationKind::ExperimentalTakeoverVictory;
    case TakeoverDefeat: return NotificationKind::ExperimentalTakeoverDefeat;
    case TreasuryProgress: return NotificationKind::ExperimentalTreasuryProgress;
    case TreasuryFull: return NotificationKind::ExperimentalTreasuryFull;
    case Achievement: return NotificationKind::ExperimentalAchievement;
    case ChallengeComplete: return NotificationKind::ExperimentalChallengeComplete;
    case ChallengeFailed: return NotificationKind::ExperimentalChallengeFailed;
    case StrikeHit: return NotificationKind::ExperimentalStrikeHit;
    case StrikeDefeat: return NotificationKind::ExperimentalStrikeDefeat;
    case WarchestProgress: return NotificationKind::ExperimentalWarchestProgress;
    case WarchestFull: return NotificationKind::ExperimentalWarchestFull;
    case ArenaTimeLeft: return NotificationKind::ExperimentalArenaTimeLeft;
    case FleetPresetApplied: return NotificationKind::ExperimentalFleetPresetApplied;
    case SurgeWarmUpEnded: return NotificationKind::ExperimentalSurgeWarmupEnded;
    case SurgeHostileGroupDefeated: return NotificationKind::ExperimentalSurgeHostileGroupDefeated;
    case SurgeTimeLeft: return NotificationKind::ExperimentalSurgeTimeLeft;
    default: return std::nullopt;
  }
}

const char* notification_kind_name(NotificationKind kind)
{
  switch (kind) {
    case NotificationKind::BattleVictory: return "battle.victory";
    case NotificationKind::BattleDefeat: return "battle.defeat";
    case NotificationKind::BattlePartialVictory: return "battle.partial_victory";
    case NotificationKind::BattleStationVictory: return "battle.station_victory";
    case NotificationKind::BattleStationDefeat: return "battle.station_defeat";
    case NotificationKind::BattleStationBattle: return "battle.station_battle";
    case NotificationKind::BattleIncomingAttackPlayer: return "battle.incoming_attack_player";
    case NotificationKind::BattleIncomingAttackHostile: return "battle.incoming_attack_hostile";
    case NotificationKind::BattleFleetBattle: return "battle.fleet_battle";
    case NotificationKind::BattleArmadaBattleWon: return "battle.armada_battle_won";
    case NotificationKind::BattleArmadaBattleLost: return "battle.armada_battle_lost";
    case NotificationKind::BattleAssaultVictory: return "battle.assault_victory";
    case NotificationKind::BattleAssaultDefeat: return "battle.assault_defeat";
    case NotificationKind::ArmadaCreated: return "armada.created";
    case NotificationKind::ArmadaCanceled: return "armada.canceled";
    case NotificationKind::EventTournament: return "event.tournament";
    case NotificationKind::EventChainedEventScored: return "event.chained_event_scored";
    case NotificationKind::ExperimentalStandard: return "experimental.standard";
    case NotificationKind::ExperimentalFactionWarning: return "experimental.faction_warning";
    case NotificationKind::ExperimentalFactionLevelUp: return "experimental.faction_level_up";
    case NotificationKind::ExperimentalFactionLevelDown: return "experimental.faction_level_down";
    case NotificationKind::ExperimentalFactionDiscovered: return "experimental.faction_discovered";
    case NotificationKind::ExperimentalArmadaIncomingAttack: return "experimental.armada_incoming_attack";
    case NotificationKind::ExperimentalDiplomacyUpdated: return "experimental.diplomacy_updated";
    case NotificationKind::ExperimentalJoinedTakeover: return "experimental.joined_takeover";
    case NotificationKind::ExperimentalCompetitorJoinedTakeover: return "experimental.competitor_joined_takeover";
    case NotificationKind::ExperimentalAbandonedTerritory: return "experimental.abandoned_territory";
    case NotificationKind::ExperimentalTakeoverVictory: return "experimental.takeover_victory";
    case NotificationKind::ExperimentalTakeoverDefeat: return "experimental.takeover_defeat";
    case NotificationKind::ExperimentalTreasuryProgress: return "experimental.treasury_progress";
    case NotificationKind::ExperimentalTreasuryFull: return "experimental.treasury_full";
    case NotificationKind::ExperimentalAchievement: return "experimental.achievement";
    case NotificationKind::ExperimentalChallengeComplete: return "experimental.challenge_complete";
    case NotificationKind::ExperimentalChallengeFailed: return "experimental.challenge_failed";
    case NotificationKind::ExperimentalStrikeHit: return "experimental.strike_hit";
    case NotificationKind::ExperimentalStrikeDefeat: return "experimental.strike_defeat";
    case NotificationKind::ExperimentalWarchestProgress: return "experimental.warchest_progress";
    case NotificationKind::ExperimentalWarchestFull: return "experimental.warchest_full";
    case NotificationKind::ExperimentalArenaTimeLeft: return "experimental.arena_time_left";
    case NotificationKind::ExperimentalFleetPresetApplied: return "experimental.fleet_preset_applied";
    case NotificationKind::ExperimentalSurgeWarmupEnded: return "experimental.surge_warmup_ended";
    case NotificationKind::ExperimentalSurgeHostileGroupDefeated: return "experimental.surge_hostile_group_defeated";
    case NotificationKind::ExperimentalSurgeTimeLeft: return "experimental.surge_time_left";
    case NotificationKind::FleetArrivedInSystem: return "fleet.arrived_in_system";
    case NotificationKind::FleetArrivedAtDestination: return "fleet.arrived_at_destination";
    case NotificationKind::FleetStartedMining: return "fleet.started_mining";
    case NotificationKind::FleetNodeDepleted: return "fleet.node_depleted";
    case NotificationKind::FleetDocked: return "fleet.docked";
    case NotificationKind::FleetRepairComplete: return "fleet.repair_complete";
    default: return "unknown";
  }
}

const char* notification_sound_name(NotificationSound sound)
{
  switch (sound) {
    case NotificationSound::None: return "none";
    case NotificationSound::Default: return "default";
    case NotificationSound::Info: return "info";
    case NotificationSound::Success: return "success";
    case NotificationSound::Warning: return "warning";
    case NotificationSound::Alarm: return "alarm";
    case NotificationSound::Arrival: return "arrival";
    case NotificationSound::Soft: return "soft";
    case NotificationSound::Ping: return "ping";
    case NotificationSound::Repair: return "repair";
    default: return "default";
  }
}

std::optional<NotificationSound> notification_sound_from_name(std::string_view name)
{
  const auto normalized = normalize_sound_name(name);
  if (normalized == "none" || normalized == "off" || normalized == "silent") {
    return NotificationSound::None;
  }
  if (normalized == "default") {
    return NotificationSound::Default;
  }
  if (normalized == "info") {
    return NotificationSound::Info;
  }
  if (normalized == "success" || normalized == "victory") {
    return NotificationSound::Success;
  }
  if (normalized == "warning" || normalized == "warn") {
    return NotificationSound::Warning;
  }
  if (normalized == "alarm" || normalized == "attack") {
    return NotificationSound::Alarm;
  }
  if (normalized == "arrival" || normalized == "arrive") {
    return NotificationSound::Arrival;
  }
  if (normalized == "soft" || normalized == "quiet") {
    return NotificationSound::Soft;
  }
  if (normalized == "ping") {
    return NotificationSound::Ping;
  }
  if (normalized == "repair" || normalized == "repaired") {
    return NotificationSound::Repair;
  }

  return std::nullopt;
}
