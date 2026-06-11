/**
 * @file fleet_notifications.cc
 * @brief Fleet notification state machine and message generation.
 *
 * This module tracks the last observed fleet-bar states and mining ETA hints,
 * then emits OS notifications when meaningful fleet transitions occur.
 */
#include "patches/fleet_notifications.h"

#include "config.h"
#include "errormsg.h"
#include "patches/fleet_alert_evidence.h"
#include "patches/fleet_alert_evidence_dispatch.h"
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
#include <ctime>
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

constexpr size_t kIncomingAttackDedupeMaxEntries = 256;
constexpr std::string_view kFleetArrivalOwner = "FleetArrivalHooks";
constexpr std::string_view kFleetStateWidgetSeam = "Digit.Prime.HUD.FleetStateWidget.SetWidgetData";
constexpr std::string_view kToastFleetObserverQueueNotificationsSeam =
    "Digit.Prime.HUD.ToastFleetObserver.QueueNotifications";
constexpr std::string_view kFleetAlertEvidenceEffect = "publish-fleet-alert-evidence";

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

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string current_time_iso_utc()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_time = std::chrono::system_clock::to_time_t(now);

  std::tm utc{};
#if _WIN32
  gmtime_s(&utc, &now_time);
#else
  gmtime_r(&utc, &now_time);
#endif

  char buffer[sizeof("2026-05-17T22:00:00Z")];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
    return {};
  }

  return buffer;
}

FleetAlertRuntimeContext fleet_alert_runtime_context(std::string_view source,
                                                     std::string_view owner,
                                                     std::string_view seam,
                                                     std::string_view reason)
{
  return FleetAlertRuntimeContext{
      gameplay_dispatch_context(source, owner, seam, reason, kFleetAlertEvidenceEffect),
      current_time_iso_utc(),
      current_time_millis_utc(),
  };
}

std::string fleet_alert_state_name(FleetState state)
{
  switch (fleet_bar_transition_state_from_value(static_cast<int>(state))) {
    case FleetBarTransitionState::Unknown:
      return "Unknown";
    case FleetBarTransitionState::IdleInSpace:
      return "IdleInSpace";
    case FleetBarTransitionState::Docked:
      return "Docked";
    case FleetBarTransitionState::Mining:
      return "Mining";
    case FleetBarTransitionState::Destroyed:
      return "Destroyed";
    case FleetBarTransitionState::TieringUp:
      return "TieringUp";
    case FleetBarTransitionState::Repairing:
      return "Repairing";
    case FleetBarTransitionState::CannotLaunch:
      return "CannotLaunch";
    case FleetBarTransitionState::Battling:
      return "Battling";
    case FleetBarTransitionState::WarpCharging:
      return "WarpCharging";
    case FleetBarTransitionState::Warping:
      return "Warping";
    case FleetBarTransitionState::CanRemove:
      return "CanRemove";
    case FleetBarTransitionState::CannotMove:
      return "CannotMove";
    case FleetBarTransitionState::Impulsing:
      return "Impulsing";
    case FleetBarTransitionState::CanManage:
      return "CanManage";
    case FleetBarTransitionState::Capturing:
      return "Capturing";
    case FleetBarTransitionState::CanRecall:
      return "CanRecall";
    case FleetBarTransitionState::CanEngage:
      return "CanEngage";
    case FleetBarTransitionState::Deployed:
      return "Deployed";
    case FleetBarTransitionState::CanLocate:
      return "CanLocate";
  }

  return "Unknown";
}

bool incoming_attack_notifications_enabled_for_kind(IncomingAttackPolicyAttackerKind attackerKind,
                                                    bool                             allow_when_unclassified)
{
  const auto& notifications = Config::Get().notifications;

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

      return allow_when_unclassified || !notifications.IncomingAttackSplitEnabled();
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
    case FleetBarTransitionNotificationKind::ArrivedInSystem: return NotificationKind::FleetArrivedInSystem;
    case FleetBarTransitionNotificationKind::ArrivedAtDestination: return NotificationKind::FleetArrivedAtDestination;
    case FleetBarTransitionNotificationKind::StartedMining: return NotificationKind::FleetStartedMining;
    case FleetBarTransitionNotificationKind::RepairComplete: return NotificationKind::FleetRepairComplete;
    case FleetBarTransitionNotificationKind::Docked: return NotificationKind::FleetDocked;
    default: return std::nullopt;
  }
}

bool should_hide_unknown_incoming_attack_notification(IncomingAttackPolicyAttackerKind attackerKind)
{
  return attackerKind == IncomingAttackPolicyAttackerKind::Unknown
         && Config::Get().notifications.IncomingAttackSplitEnabled();
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

FleetAlertShipEvidence fleet_bar_ship_evidence(FleetPlayerData* fleet, const std::string& display_name)
{
  FleetAlertShipEvidence evidence;
  evidence.display_name = display_name;
  evidence.hull_name = display_name;

  auto* hull = fleet ? fleet->Hull : nullptr;
  if (hull) {
    evidence.hull_spec_id = std::to_string(hull->Id);
    if (hull->Name) {
      evidence.hull_name = fleet_bar_ship_name(fleet);
    }
  }

  auto* ship = fleet ? fleet->Ship : nullptr;
  if (ship && ship->ID != 0) {
    evidence.ship_id = std::to_string(ship->ID);
  }

  return evidence;
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

void maybe_notify_fleet_bar_transition(uint64_t fleetId, const std::string& shipName, FleetState oldState,
                                       FleetState newState, const std::string& resourceName,
                                       const std::string& cargoText)
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
    spdlog::debug("[FleetBar] suppress ambiguous docked transition id={} ship='{}' oldState={} newState={}", fleetId,
                  shipName, static_cast<int>(oldState), static_cast<int>(newState));
    return;
  }

  if (decision.kind == FleetBarTransitionNotificationKind::None) {
    return;
  }

  spdlog::debug("[FleetBar] {} id={} ship='{}'", fleet_bar_transition_notification_kind_name(decision.kind), fleetId,
                shipName);
  const auto notification_kind = notification_kind_from_fleet_transition(decision.kind);
  if (notification_kind.has_value()) {
    notification_emit(notification_kind.value(), decision.title.c_str(), decision.body.c_str());
  }
}

void maybe_publish_fleet_transition_alert_evidence(FleetPlayerData* fleet,
                                                   uint64_t fleetId,
                                                   const std::string& shipName,
                                                   FleetState previousState,
                                                   FleetState currentState,
                                                   const char* runtimeTriggerSource)
{
  if (!runtimeTriggerSource) {
    return;
  }

  const auto alert_event_type = fleet_alert_event_type_for_transition_reason(runtimeTriggerSource);
  if (!alert_event_type.has_value()) {
    return;
  }

  FleetAlertFleetTransitionEvidence evidence;
  evidence.runtime = fleet_alert_runtime_context(runtimeTriggerSource,
                                                 kFleetArrivalOwner,
                                                 kFleetStateWidgetSeam,
                                                 runtimeTriggerSource);
  evidence.alert_event_type = *alert_event_type;
  evidence.fleet_id = fleetId;
  evidence.previous_state = static_cast<int>(previousState);
  evidence.current_state = static_cast<int>(currentState);
  evidence.previous_state_name = fleet_alert_state_name(previousState);
  evidence.current_state_name = fleet_alert_state_name(currentState);
  evidence.ship = fleet_bar_ship_evidence(fleet, shipName);
  evidence.missing_evidence.push_back("systemId");
  fleet_alert_evidence_publish_fleet_transition(evidence);
}

void publish_incoming_attack_alert_evidence(const char* source,
                                            uint64_t targetFleetId,
                                            int targetType,
                                            int attackerFleetType,
                                            IncomingAttackPolicyAttackerKind attackerKind,
                                            std::string_view attackerIdentity,
                                            const IncomingAttackNotificationContext& context)
{
  FleetAlertIncomingAttackEvidence evidence;
  evidence.runtime = fleet_alert_runtime_context(source ? source : "unknown",
                                                 kFleetArrivalOwner,
                                                 kToastFleetObserverQueueNotificationsSeam,
                                                 "incoming-attack-materialized");
  evidence.target_type = targetType;
  evidence.target_type_name = incoming_attack_policy_target_type_name(targetType);
  evidence.target_fleet_id = targetFleetId;
  evidence.resolved_fleet_id = context.fleet_id;
  evidence.resolved_fleet_state = static_cast<int>(context.state);
  evidence.resolved_fleet_state_name = fleet_alert_state_name(context.state);
  evidence.target_ship_display_name = context.ship_name;
  evidence.attacker_fleet_type = attackerFleetType;
  evidence.attacker_kind = incoming_attack_policy_attacker_kind_name(attackerKind);
  evidence.attacker_identity = std::string(attackerIdentity);
  evidence.missing_evidence.push_back("systemId");
  if (context.fleet_id == 0) {
    evidence.missing_evidence.push_back("resolvedFleetId");
  }
  if (context.ship_name.empty()) {
    evidence.missing_evidence.push_back("targetShipDisplayName");
  }
  fleet_alert_evidence_publish_incoming_attack(evidence);
}
} // namespace

void fleet_notifications_init()
{
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

const char* fleet_notifications_observe_fleet_bar(FleetPlayerData* fleet)
{
  if (!fleet) {
    return nullptr;
  }

  auto fleetId        = fleet->Id;
  auto currentState   = fleet->CurrentState;
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
    runtimeTriggerSource = fleet_runtime_trigger_source_for_state_transition(static_cast<int>(previousState),
                                                                             static_cast<int>(currentState));
    maybe_publish_fleet_transition_alert_evidence(fleet, fleetId, shipName, previousState, currentState,
                                                  runtimeTriggerSource);
    maybe_notify_fleet_bar_transition(fleetId, shipName, previousState, currentState, resourceName, cargoText);
  }

  s_fleet_bar_states[fleetId] = currentState;
  return runtimeTriggerSource;
}

void fleet_notifications_observe_runtime_fleets()
{
  if (!fleet_notifications_runtime_events_enabled()) {
    return;
  }

  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager) {
    return;
  }

  for (int slot_index = 0; slot_index < kFleetIndexMax; ++slot_index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(slot_index);
    if (!fleet) {
      continue;
    }

    fleet_notifications_observe_fleet_bar(fleet);
  }
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
    IncomingAttackNotificationContext context;
    publish_incoming_attack_alert_evidence(source,
                                           targetFleetId,
                                           targetType,
                                           attackerFleetType,
                                           attacker_kind,
                                           attackerIdentity,
                                           context);
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

  publish_incoming_attack_alert_evidence(source,
                                         targetFleetId,
                                         targetType,
                                         attackerFleetType,
                                         attacker_kind,
                                         attackerIdentity,
                                         context);

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
