/**
 * @file fleet_notifications.cc
 * @brief Fleet notification state machine and message generation.
 *
 * This module tracks the last observed fleet states and mining ETA hints,
 * then emits OS notifications when meaningful fleet transitions occur.
 */
#include "patches/fleet_notifications.h"

#include "config.h"
#include "errormsg.h"
#include "patches/fleet_notification_scan_policy.h"
#include "patches/live_debug.h"
#include "patches/live_debug_fleet_serializers.h"
#include "patches/notification_audio.h"
#include "patches/notification_policy.h"
#include "patches/notification_service.h"

#include <prime/FleetPlayerData.h>
#include <prime/FleetsManager.h>
#include <prime/NotificationIncomingFleetParams.h>
#include <prime/SpecManager.h>
#include <prime/Toast.h>
#include <testable_functions.h>

#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
std::unordered_map<uint64_t, FleetState>  s_fleet_bar_states;
std::unordered_map<uint64_t, std::string> s_fleet_bar_ship_names;
std::unordered_map<uint64_t, std::string> s_fleet_bar_resource_names;
std::unordered_map<uint64_t, float>       s_fleet_bar_cargo_fill_levels;
std::unordered_map<uint64_t, int64_t>     s_mining_viewer_remaining_seconds;
FleetNotificationScanPolicy               s_runtime_scan_policy;
bool                                      s_runtime_scan_active_logged = false;

constexpr size_t kIncomingAttackDedupeMaxEntries = 256;

IncomingAttackPolicyDeduper s_recent_incoming_attack_notifications(kIncomingAttackDedupeMaxEntries);

struct IncomingAttackNotificationContext {
  int         candidate_count = 0;
  uint64_t    fleet_id        = 0;
  std::string ship_name;
  FleetState  state = FleetState::Unknown;
};

int64_t incoming_attack_now_seconds()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool fleet_state_requires_scan_follow_through(const FleetState state)
{
  switch (state) {
    case FleetState::TieringUp:
    case FleetState::Repairing:
    case FleetState::Battling:
    case FleetState::WarpCharging:
    case FleetState::Warping:
    case FleetState::Impulsing:
    case FleetState::Capturing:
      return true;
    default:
      return false;
  }
}

bool incoming_attack_notifications_enabled_for_kind(IncomingAttackPolicyAttackerKind attackerKind,
                                                    bool                             allow_when_unclassified)
{
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Player:
      return notification_delivery_enabled(NotificationKind::BattleIncomingAttackPlayer);
    case IncomingAttackPolicyAttackerKind::Hostile:
      return notification_delivery_enabled(NotificationKind::BattleIncomingAttackHostile);
    default:
      if (!notification_delivery_enabled(NotificationKind::BattleIncomingAttackPlayer)
          && !notification_delivery_enabled(NotificationKind::BattleIncomingAttackHostile)) {
        return false;
      }

      return allow_when_unclassified
             || notification_policy_delivery_equivalent(NotificationKind::BattleIncomingAttackPlayer,
                                                        NotificationKind::BattleIncomingAttackHostile);
  }
}

NotificationKind incoming_attack_notification_kind_for_delivery(IncomingAttackPolicyAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Player:
      return NotificationKind::BattleIncomingAttackPlayer;
    case IncomingAttackPolicyAttackerKind::Hostile:
      return NotificationKind::BattleIncomingAttackHostile;
    default:
      return notification_delivery_enabled(NotificationKind::BattleIncomingAttackPlayer)
                 ? NotificationKind::BattleIncomingAttackPlayer
                 : NotificationKind::BattleIncomingAttackHostile;
  }
}

std::optional<NotificationKind> notification_kind_from_fleet_transition(FleetBarTransitionNotificationKind kind)
{
  switch (kind) {
    case FleetBarTransitionNotificationKind::ArrivedInSystem:
      return NotificationKind::FleetArrivedInSystem;
    case FleetBarTransitionNotificationKind::ArrivedAtDestination:
      return NotificationKind::FleetArrivedAtDestination;
    case FleetBarTransitionNotificationKind::StartedMining:
      return NotificationKind::FleetStartedMining;
    case FleetBarTransitionNotificationKind::RepairComplete:
      return NotificationKind::FleetRepairComplete;
    case FleetBarTransitionNotificationKind::Docked:
      return NotificationKind::FleetDocked;
    default:
      return std::nullopt;
  }
}

bool should_hide_unknown_incoming_attack_notification(IncomingAttackPolicyAttackerKind attackerKind)
{
  return attackerKind == IncomingAttackPolicyAttackerKind::Unknown
         && !notification_policy_delivery_equivalent(NotificationKind::BattleIncomingAttackPlayer,
                                                     NotificationKind::BattleIncomingAttackHostile);
}

bool should_emit_incoming_attack_notification(const char* source, uint64_t fleetId, int targetType,
                                              IncomingAttackPolicyAttackerKind attackerKind,
                                              bool allow_when_unclassified, std::string_view attackerIdentity = {})
{
  if (!incoming_attack_notifications_enabled_for_kind(attackerKind, allow_when_unclassified)) {
    return false;
  }

  const auto key    = incoming_attack_policy_dedupe_key(fleetId, targetType, attackerKind, attackerIdentity);
  const auto result = s_recent_incoming_attack_notifications.should_emit(key, incoming_attack_now_seconds());
  if (!result.emitted) {
    spdlog::info("[IncomingAttack] source={} mode=suppressed reason=dedupe-window fleetId={} targetType={} "
                 "attackerKind={} attackerIdentity='{}' windowSec={}",
                 source ? source : "unknown", fleetId, targetType,
                 incoming_attack_policy_attacker_kind_name(attackerKind), key.attacker_identity,
                 incoming_attack_policy_dedupe_window_seconds(key));
    return false;
  }

  return true;
}

std::string fleet_bar_ship_name(FleetPlayerData* fleet)
{
  auto* hull = fleet ? fleet->Hull : nullptr;
  auto  name = (hull && hull->Name) ? to_string(hull->Name) : std::string{"?"};

  constexpr std::string_view live_suffix = "_LIVE";
  if (name.size() >= live_suffix.size()
      && name.compare(name.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
    name.erase(name.size() - live_suffix.size());
  }

  for (auto& ch : name) {
    if (ch == '_') {
      ch = ' ';
    }
  }

  return name;
}

std::string fleet_bar_cached_ship_name(uint64_t fleetId)
{
  auto it = s_fleet_bar_ship_names.find(fleetId);
  if (it == s_fleet_bar_ship_names.end()) {
    return {};
  }

  return it->second;
}

std::string fleet_bar_cached_resource_name(uint64_t fleetId)
{
  auto it = s_fleet_bar_resource_names.find(fleetId);
  if (it == s_fleet_bar_resource_names.end()) {
    return {};
  }

  return it->second;
}

float fleet_bar_cached_cargo_fill_level(uint64_t fleetId)
{
  auto it = s_fleet_bar_cargo_fill_levels.find(fleetId);
  if (it == s_fleet_bar_cargo_fill_levels.end()) {
    return -1.0f;
  }

  return it->second;
}

std::string normalize_resource_name(const std::string& name)
{
  if (name.empty()) {
    return {};
  }

  auto                       normalized  = name;
  constexpr std::string_view live_suffix = "_LIVE";
  if (normalized.size() >= live_suffix.size()
      && normalized.compare(normalized.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
    normalized.erase(normalized.size() - live_suffix.size());
  }

  for (auto& ch : normalized) {
    if (ch == '_') {
      ch = ' ';
    }
  }

  constexpr std::string_view resource_prefix = "Resource ";
  if (normalized.size() >= resource_prefix.size()
      && normalized.compare(0, resource_prefix.size(), resource_prefix) == 0) {
    normalized.erase(0, resource_prefix.size());
  }

  return normalized;
}

std::string fleet_bar_resource_name(FleetPlayerData* fleet)
{
  auto* miningData = fleet ? fleet->MiningData : nullptr;
  if (!miningData) {
    return {};
  }

  const auto resourceId  = miningData->ResourceId;
  auto*      specManager = SpecManager::Instance();
  if (!specManager) {
    return {};
  }

  auto* resourceSpec = specManager->GetResourceSpec(resourceId);
  auto* rawName      = resourceSpec ? resourceSpec->Name : nullptr;
  auto  rawNameText  = rawName ? to_string(rawName) : std::string{};
  return normalize_resource_name(rawNameText);
}

std::string fleet_cargo_fill_text(float fillLevel)
{ return format_cargo_fill_text(fillLevel); }

void populate_context_from_fleet_cache(IncomingAttackNotificationContext& context, uint64_t fleetId)
{
  context.fleet_id  = fleetId;
  auto state_it     = s_fleet_bar_states.find(fleetId);
  context.state     = state_it != s_fleet_bar_states.end() ? state_it->second : FleetState::Unknown;
  context.ship_name = fleet_bar_cached_ship_name(fleetId);
}

int incoming_attack_candidate_count()
{ return static_cast<int>(s_fleet_bar_states.size()); }

IncomingAttackNotificationContext context_from_target_fleet(uint64_t targetFleetId)
{
  IncomingAttackNotificationContext context;
  context.candidate_count = incoming_attack_candidate_count();
  if (targetFleetId != 0) {
    populate_context_from_fleet_cache(context, targetFleetId);
  }

  return context;
}

std::string
build_incoming_attack_body(const IncomingAttackNotificationContext& context,
                           IncomingAttackPolicyAttackerKind attackerKind = IncomingAttackPolicyAttackerKind::Unknown)
{ return incoming_attack_policy_fleet_body(context.ship_name, attackerKind); }

std::string build_station_incoming_attack_body(IncomingAttackPolicyAttackerKind attackerKind)
{ return incoming_attack_policy_station_body(attackerKind); }

int64_t duration_ticks_to_seconds(int64_t ticks)
{
  if (ticks < 0) {
    return -1;
  }

  return static_cast<int64_t>(std::llround(static_cast<double>(ticks) / 10000000.0));
}

void maybe_notify_fleet_state_transition(uint64_t fleetId, const std::string& shipName, FleetState oldState,
                                         FleetState newState, const std::string& resourceName,
                                         const std::string& cargoText, std::string_view observationSource)
{
  auto eta_it = s_mining_viewer_remaining_seconds.find(fleetId);
  auto etaText =
      (eta_it != s_mining_viewer_remaining_seconds.end()) ? format_duration_short(eta_it->second) : std::string{};

  auto decision = fleet_bar_transition_notification_decision({
      static_cast<int>(oldState),
      static_cast<int>(newState),
      notification_delivery_enabled(NotificationKind::FleetArrivedInSystem),
      notification_delivery_enabled(NotificationKind::FleetArrivedAtDestination),
      notification_delivery_enabled(NotificationKind::FleetStartedMining),
      notification_delivery_enabled(NotificationKind::FleetDocked),
      notification_delivery_enabled(NotificationKind::FleetRepairComplete),
      shipName,
      resourceName,
      etaText,
      cargoText,
  });

  if (decision.clear_mining_eta) {
    s_mining_viewer_remaining_seconds.erase(fleetId);
  }

  if (decision.suppressed_ambiguous_docked) {
    spdlog::debug("[FleetState] source={} suppress ambiguous docked transition id={} ship='{}' oldState={} newState={}",
                  observationSource, fleetId, shipName, static_cast<int>(oldState), static_cast<int>(newState));
    return;
  }

  if (decision.kind == FleetBarTransitionNotificationKind::None) {
    return;
  }

  spdlog::debug("[FleetState] source={} kind={} id={} ship='{}' oldState={} newState={}", observationSource,
                fleet_bar_transition_notification_kind_name(decision.kind), fleetId, shipName,
                static_cast<int>(oldState), static_cast<int>(newState));
  const auto notification_kind = notification_kind_from_fleet_transition(decision.kind);
  if (notification_kind.has_value()) {
    notification_emit(notification_kind.value(), decision.title.c_str(), decision.body.c_str());
  }
}
} // namespace

void fleet_notifications_init()
{
  s_runtime_scan_policy.Reset();
  s_runtime_scan_active_logged = false;
  notification_init();
  notification_audio_init();
}

bool fleet_notifications_runtime_events_enabled()
{
  if (!Config::Get().installFleetArrivalHooks) {
    return false;
  }

  return notification_delivery_enabled(NotificationKind::FleetArrivedInSystem)
         || notification_delivery_enabled(NotificationKind::FleetArrivedAtDestination)
         || notification_delivery_enabled(NotificationKind::FleetStartedMining)
         || notification_delivery_enabled(NotificationKind::FleetDocked)
         || notification_delivery_enabled(NotificationKind::FleetRepairComplete);
}

const char* fleet_notifications_observe_fleet_state(FleetPlayerData* fleet, std::string_view observationSource)
{
  if (!fleet) {
    return nullptr;
  }

  auto fleetId      = fleet->Id;
  auto currentState = fleet->CurrentState;
  if (fleet_notifications_runtime_events_enabled() && observationSource != "fleets-manager-scan"
      && fleet_state_requires_scan_follow_through(currentState) && !s_runtime_scan_policy.ScanRequested()) {
    s_runtime_scan_policy.RequestScan();
    spdlog::info("[FleetState] source={} status=scan-requested fleet={} state={}", observationSource, fleetId,
                 static_cast<int>(currentState));
  }

  auto shipName       = fleet_bar_ship_name(fleet);
  auto resourceName   = fleet_bar_resource_name(fleet);
  auto cargoFillLevel = fleet->CargoResourceFillLevel;
  auto cargoText      = fleet_cargo_fill_text(cargoFillLevel);

  auto it               = s_fleet_bar_states.find(fleetId);
  auto previousState    = FleetState::Unknown;
  auto hadPreviousState = false;
  if (it != s_fleet_bar_states.end()) {
    previousState    = it->second;
    hadPreviousState = true;
  }

  const char* runtimeTriggerSource = nullptr;

  s_fleet_bar_ship_names[fleetId] = shipName;
  if (!resourceName.empty()) {
    s_fleet_bar_resource_names[fleetId] = resourceName;
  }
  s_fleet_bar_cargo_fill_levels[fleetId] = cargoFillLevel;

  if (hadPreviousState && previousState != currentState) {
    maybe_notify_fleet_state_transition(fleetId, shipName, previousState, currentState, resourceName, cargoText,
                                        observationSource);
    runtimeTriggerSource = fleet_runtime_trigger_source_for_state_transition(static_cast<int>(previousState),
                                                                             static_cast<int>(currentState));
  }

  s_fleet_bar_states[fleetId] = currentState;
  return runtimeTriggerSource;
}

FleetNotificationRuntimeScanResult fleet_notifications_observe_runtime_fleets()
{
  FleetNotificationRuntimeScanResult result;
  if (!fleet_notifications_runtime_events_enabled()) {
    return result;
  }

  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager) {
    return result;
  }

  for (int slot_index = 0; slot_index < kFleetIndexMax; ++slot_index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(slot_index);
    if (!fleet) {
      continue;
    }

    fleet_notifications_observe_fleet_state(fleet, "fleets-manager-scan");
    ++result.observed_count;
    if (fleet_state_requires_scan_follow_through(fleet->CurrentState)) {
      ++result.follow_through_count;
    }
  }

  return result;
}

void fleet_notifications_tick()
{
  const auto now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
  const auto scan_decision = s_runtime_scan_policy.Evaluate(now_ms);
  if (scan_decision == FleetNotificationScanDecision::Expired) {
    s_runtime_scan_active_logged = false;
    spdlog::warn("[FleetState] source=fleets-manager-scan status=suspended reason=max-lifetime lifetimeMs={}",
                 kFleetNotificationScanMaxLifetimeMs);
    return;
  }
  if (scan_decision != FleetNotificationScanDecision::Scan) {
    return;
  }

  const auto result = fleet_notifications_observe_runtime_fleets();
  if (result.observed_count > 0 && !s_runtime_scan_active_logged) {
    s_runtime_scan_active_logged = true;
    spdlog::info("[FleetState] source=fleets-manager-scan status=active cadenceMs={} fleetCount={} followThrough={}",
                 kFleetNotificationScanIntervalMs, result.observed_count, result.follow_through_count);
  }

  const auto observation = s_runtime_scan_policy.RecordObservation(result.observed_count, result.follow_through_count);
  if (observation == FleetNotificationScanObservation::Settled) {
    s_runtime_scan_active_logged = false;
    spdlog::info("[FleetState] source=fleets-manager-scan status=idle");
  } else if (observation == FleetNotificationScanObservation::NoFleets) {
    s_runtime_scan_active_logged = false;
    spdlog::warn("[FleetState] source=fleets-manager-scan status=suspended reason=no-fleets emptyScans={}",
                 kFleetNotificationScanMaxConsecutiveEmpty);
  }
}

void fleet_notifications_suspend_runtime_scan()
{
  s_runtime_scan_policy.Suspend();
  s_runtime_scan_active_logged = false;
}

void fleet_notifications_observe_node_depleted(int64_t fleetId)
{
  if (!notification_delivery_enabled(NotificationKind::FleetNodeDepleted)) {
    return;
  }

  auto shipName     = fleet_bar_cached_ship_name(static_cast<uint64_t>(fleetId));
  auto resourceName = fleet_bar_cached_resource_name(static_cast<uint64_t>(fleetId));
  auto cargoText    = fleet_cargo_fill_text(fleet_bar_cached_cargo_fill_level(static_cast<uint64_t>(fleetId)));

  s_mining_viewer_remaining_seconds.erase(static_cast<uint64_t>(fleetId));

  auto body = format_node_depleted_body(shipName, resourceName, cargoText);
  notification_emit(NotificationKind::FleetNodeDepleted, "Node Depleted", body.c_str());
}

void fleet_notifications_notify_incoming_attack_target(const ToastFleetQueueNotificationsSignal& signal)
{
  const auto* source            = signal.source;
  const auto  targetFleetId     = signal.target_fleet_id;
  const auto  targetType        = signal.target_type;
  const auto  attackerFleetType = signal.attacker_fleet_type;
  const auto  attackerIdentity  = signal.attacker_identity;
  auto        attacker_kind     = incoming_attack_policy_attacker_kind_from_fleet_type(attackerFleetType);

  if (targetType == static_cast<int>(NotificationIncomingAttackTargetType::Station)) {
    const bool hide_notification = should_hide_unknown_incoming_attack_notification(attacker_kind);
    if (!should_emit_incoming_attack_notification(source, 0, targetType, attacker_kind, true, attackerIdentity)) {
      return;
    }

    auto body  = build_station_incoming_attack_body(attacker_kind);
    auto title = incoming_attack_policy_title_for_kind(attacker_kind);
    spdlog::info(
        "[IncomingAttack] source={} mode=targeted targetType={} targetTypeName={} rawTargetFleetId={} "
        "attackerFleetType={} attackerKind={} attackerIdentity='{}' hidden={} resolvedTarget=station body='{}'",
        source ? source : "unknown", targetType, incoming_attack_policy_target_type_name(targetType), targetFleetId,
        attackerFleetType, incoming_attack_policy_attacker_kind_name(attacker_kind), attackerIdentity,
        hide_notification, body);
    live_debug_record_incoming_attack_notification_context(source ? source : "unknown", body,
                                                           incoming_attack_candidate_count(), 0, "",
                                                           static_cast<int>(FleetState::Unknown), attackerFleetType);
    if (hide_notification) {
      return;
    }
    notification_emit(incoming_attack_notification_kind_for_delivery(attacker_kind), title ? title : "Incoming Attack!",
                      body.c_str());
    return;
  }

  const auto context           = context_from_target_fleet(targetFleetId);
  const auto dedupe_fleet_id   = targetFleetId != 0 ? targetFleetId : context.fleet_id;
  const bool hide_notification = should_hide_unknown_incoming_attack_notification(attacker_kind);

  if (!should_emit_incoming_attack_notification(source, dedupe_fleet_id, targetType, attacker_kind, true,
                                                attackerIdentity)) {
    return;
  }

  const auto body  = build_incoming_attack_body(context, attacker_kind);
  auto       title = incoming_attack_policy_title_for_kind(attacker_kind);
  spdlog::info("[IncomingAttack] source={} mode=targeted targetType={} targetTypeName={} rawTargetFleetId={} "
               "resolvedFleetId={} ship='{}' state={} attackerFleetType={} attackerKind={} attackerIdentity='{}' "
               "candidateCount={} hidden={} body='{}'",
               source ? source : "unknown", targetType, incoming_attack_policy_target_type_name(targetType),
               targetFleetId, context.fleet_id, context.ship_name, static_cast<int>(context.state), attackerFleetType,
               incoming_attack_policy_attacker_kind_name(attacker_kind), attackerIdentity, context.candidate_count,
               hide_notification, body);
  spdlog::debug("[IncomingAttack] notify source={} targetFleetId={} targetType={} attackerFleetType={} attackerKind={} "
                "candidateCount={} fleetId={} ship='{}' state={} body='{}'",
                source ? source : "unknown", targetFleetId, targetType, attackerFleetType,
                incoming_attack_policy_attacker_kind_name(attacker_kind), context.candidate_count, context.fleet_id,
                context.ship_name, static_cast<int>(context.state), body);
  live_debug_record_incoming_attack_notification_context(source ? source : "unknown", body, context.candidate_count,
                                                         context.fleet_id, context.ship_name,
                                                         static_cast<int>(context.state), attackerFleetType);
  if (hide_notification) {
    return;
  }
  notification_emit(incoming_attack_notification_kind_for_delivery(attacker_kind), title ? title : "Incoming Attack!",
                    body.c_str());
}

void fleet_notifications_notify_incoming_attack_target(const char* source, uint64_t targetFleetId, int targetType,
                                                       int attackerFleetType, std::string_view attackerIdentity)
{
  fleet_notifications_notify_incoming_attack_target(
      ToastFleetQueueNotificationsSignal{source, targetFleetId, targetType, attackerFleetType, attackerIdentity});
}

void fleet_notifications_observe_mining_timer(FleetPlayerData* selectedFleet, int64_t remainingTicks)
{
  if (!selectedFleet) {
    return;
  }

  auto remainingSeconds = duration_ticks_to_seconds(remainingTicks);
  if (remainingSeconds > 0) {
    s_mining_viewer_remaining_seconds[selectedFleet->Id] = remainingSeconds;
  }
}
