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
#include "patches/live_debug.h"
#include "patches/notification_service.h"

#include <prime/FleetPlayerData.h>
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

namespace {
std::unordered_map<uint64_t, FleetState>   s_fleet_bar_states;
std::unordered_map<uint64_t, std::string>  s_fleet_bar_ship_names;
std::unordered_map<uint64_t, std::string>  s_fleet_bar_resource_names;
std::unordered_map<uint64_t, float>        s_fleet_bar_cargo_fill_levels;
std::unordered_map<uint64_t, int64_t>      s_mining_viewer_remaining_seconds;

constexpr size_t kIncomingAttackDedupeMaxEntries = 256;

IncomingAttackPolicyDeduper s_recent_incoming_attack_notifications(kIncomingAttackDedupeMaxEntries);

struct IncomingAttackNotificationContext {
  int         candidate_count = 0;
  uint64_t    fleet_id = 0;
  std::string ship_name;
  FleetState  state = FleetState::Unknown;
};

int64_t incoming_attack_now_seconds()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool incoming_attack_notifications_enabled_for_kind(IncomingAttackPolicyAttackerKind attackerKind,
                                                    bool allow_when_unclassified)
{
  const auto& notifications = Config::Get().notifications;

  switch (attackerKind) {
    case IncomingAttackPolicyAttackerKind::Player:
      return notifications.incoming_attack_player;
    case IncomingAttackPolicyAttackerKind::Hostile:
      return notifications.incoming_attack_hostile;
    default:
      if (!notifications.AnyIncomingAttackEnabled()) {
        return false;
      }

      return allow_when_unclassified || !notifications.IncomingAttackSplitEnabled();
  }
}

bool should_hide_unknown_incoming_attack_notification(IncomingAttackPolicyAttackerKind attackerKind)
{
  return attackerKind == IncomingAttackPolicyAttackerKind::Unknown &&
      Config::Get().notifications.IncomingAttackSplitEnabled();
}

bool should_emit_incoming_attack_notification(const char* source,
                                              uint64_t fleetId,
                                              int targetType,
                                              IncomingAttackPolicyAttackerKind attackerKind,
                                              bool allow_when_unclassified,
                                              std::string_view attackerIdentity = {})
{
  if (!incoming_attack_notifications_enabled_for_kind(attackerKind, allow_when_unclassified)) {
    return false;
  }

  const auto key = incoming_attack_policy_dedupe_key(fleetId, targetType, attackerKind, attackerIdentity);
  const auto result = s_recent_incoming_attack_notifications.should_emit(key, incoming_attack_now_seconds());
  if (!result.emitted) {
    spdlog::info("[IncomingAttack] source={} mode=suppressed reason=dedupe-window fleetId={} targetType={} attackerKind={} attackerIdentity='{}' windowSec={}",
                  source ? source : "unknown",
                  fleetId,
                  targetType,
                  incoming_attack_policy_attacker_kind_name(attackerKind),
                  key.attacker_identity,
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
  if (name.size() >= live_suffix.size() &&
      name.compare(name.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
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

  auto normalized = name;
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

  const auto resourceId = miningData->ResourceId;
  auto* specManager = SpecManager::Instance();
  if (!specManager) {
    return {};
  }

  auto* resourceSpec = specManager->GetResourceSpec(resourceId);
  auto* rawName      = resourceSpec ? resourceSpec->Name : nullptr;
  auto  rawNameText  = rawName ? to_string(rawName) : std::string{};
  return normalize_resource_name(rawNameText);
}

std::string fleet_cargo_fill_text(float fillLevel)
{
  return format_cargo_fill_text(fillLevel);
}

void populate_context_from_fleet_cache(IncomingAttackNotificationContext& context, uint64_t fleetId)
{
  context.fleet_id = fleetId;
  auto state_it = s_fleet_bar_states.find(fleetId);
  context.state = state_it != s_fleet_bar_states.end() ? state_it->second : FleetState::Unknown;
  context.ship_name = fleet_bar_cached_ship_name(fleetId);
}

int incoming_attack_candidate_count()
{
  return static_cast<int>(s_fleet_bar_states.size());
}

IncomingAttackNotificationContext context_from_target_fleet(uint64_t targetFleetId)
{
  IncomingAttackNotificationContext context;
  context.candidate_count = incoming_attack_candidate_count();
  if (targetFleetId != 0) {
    populate_context_from_fleet_cache(context, targetFleetId);
  }

  return context;
}

std::string build_incoming_attack_body(const IncomingAttackNotificationContext& context,
                                       IncomingAttackPolicyAttackerKind attackerKind = IncomingAttackPolicyAttackerKind::Unknown)
{
  return incoming_attack_policy_fleet_body(context.ship_name, attackerKind);
}

std::string build_station_incoming_attack_body(IncomingAttackPolicyAttackerKind attackerKind)
{
  return incoming_attack_policy_station_body(attackerKind);
}

int64_t duration_ticks_to_seconds(int64_t ticks)
{
  if (ticks < 0) {
    return -1;
  }

  return static_cast<int64_t>(std::llround(static_cast<double>(ticks) / 10000000.0));
}

void maybe_notify_fleet_bar_transition(uint64_t fleetId, const std::string& shipName,
                                       FleetState oldState, FleetState newState,
                                       const std::string& resourceName,
                                       const std::string& cargoText)
{
  if (oldState == FleetState::Warping && newState == FleetState::Impulsing) {
    if (!Config::Get().notifications.fleet_arrived_in_system) {
      return;
    }

    auto body = "Your " + shipName + " has arrived in-system";
    spdlog::debug("[FleetBar] ARRIVED_IN_SYSTEM id={} ship='{}'", fleetId, shipName);
    notification_show("Fleet Arrived", body.c_str());
    return;
  }

  if (oldState != FleetState::Mining && newState == FleetState::Mining) {
    if (!Config::Get().notifications.fleet_started_mining) {
      return;
    }

    auto it      = s_mining_viewer_remaining_seconds.find(fleetId);
    auto etaText = (it != s_mining_viewer_remaining_seconds.end()) ? format_duration_short(it->second)
                                                                   : std::string{};
    auto title   = format_started_mining_title(shipName, resourceName);
    auto body    = format_started_mining_body(etaText, cargoText);
    s_mining_viewer_remaining_seconds.erase(fleetId);
    notification_show(title.c_str(), body.c_str());
    return;
  }

  if (oldState == FleetState::Mining && newState != FleetState::Mining) {
    s_mining_viewer_remaining_seconds.erase(fleetId);
  }

  if (oldState != FleetState::Docked && newState == FleetState::Docked) {
    if (oldState == FleetState::Repairing) {
      if (!Config::Get().notifications.fleet_repair_complete) {
        spdlog::debug("[FleetBar] suppress docked-after-repair id={} ship='{}'", fleetId, shipName);
        return;
      }

      auto body = "Your " + shipName + " finished repairs";
      spdlog::debug("[FleetBar] REPAIR_COMPLETE id={} ship='{}'", fleetId, shipName);
      notification_show("Repair Complete", body.c_str());
      return;
    }

    if (!Config::Get().notifications.fleet_docked) {
      return;
    }

    auto body = "Your " + shipName + " docked";
    spdlog::debug("[FleetBar] DOCKED id={} ship='{}'", fleetId, shipName);
    notification_show("Fleet Docked", body.c_str());
    return;
  }
}
} // namespace

void fleet_notifications_init()
{
  notification_init();
}

void fleet_notifications_observe_fleet_bar(FleetPlayerData* fleet)
{
  if (!fleet) {
    return;
  }

  auto fleetId         = fleet->Id;
  auto currentState    = fleet->CurrentState;
  auto shipName        = fleet_bar_ship_name(fleet);
  auto resourceName    = fleet_bar_resource_name(fleet);
  auto cargoFillLevel  = fleet->CargoResourceFillLevel;
  auto cargoText       = fleet_cargo_fill_text(cargoFillLevel);

  auto it               = s_fleet_bar_states.find(fleetId);
  auto previousState    = FleetState::Unknown;
  auto hadPreviousState = false;
  if (it != s_fleet_bar_states.end()) {
    previousState    = it->second;
    hadPreviousState = true;
  }

  s_fleet_bar_ship_names[fleetId] = shipName;
  if (!resourceName.empty()) {
    s_fleet_bar_resource_names[fleetId] = resourceName;
  }
  s_fleet_bar_cargo_fill_levels[fleetId] = cargoFillLevel;

  if (hadPreviousState && previousState != currentState) {
    maybe_notify_fleet_bar_transition(fleetId, shipName, previousState, currentState, resourceName, cargoText);
  }

  s_fleet_bar_states[fleetId] = currentState;
}

void fleet_notifications_observe_node_depleted(int64_t fleetId)
{
  if (!Config::Get().notifications.fleet_node_depleted) {
    return;
  }

  auto shipName     = fleet_bar_cached_ship_name(static_cast<uint64_t>(fleetId));
  auto resourceName = fleet_bar_cached_resource_name(static_cast<uint64_t>(fleetId));
  auto cargoText    = fleet_cargo_fill_text(fleet_bar_cached_cargo_fill_level(static_cast<uint64_t>(fleetId)));

  s_mining_viewer_remaining_seconds.erase(static_cast<uint64_t>(fleetId));

  auto body = format_node_depleted_body(shipName, resourceName, cargoText);
  notification_show("Node Depleted", body.c_str());
}

void fleet_notifications_notify_incoming_attack_target(const char* source, uint64_t targetFleetId, int targetType,
                                                       int attackerFleetType, std::string_view attackerIdentity)
{
  auto attacker_kind = incoming_attack_policy_attacker_kind_from_fleet_type(attackerFleetType);

  if (targetType == static_cast<int>(NotificationIncomingAttackTargetType::Station)) {
    const bool hide_notification = should_hide_unknown_incoming_attack_notification(attacker_kind);
    if (!should_emit_incoming_attack_notification(source, 0, targetType, attacker_kind, true, attackerIdentity)) {
      return;
    }

    auto body = build_station_incoming_attack_body(attacker_kind);
    auto title = incoming_attack_policy_title_for_kind(attacker_kind);
    spdlog::info("[IncomingAttack] source={} mode=targeted targetType={} targetTypeName={} rawTargetFleetId={} attackerFleetType={} attackerKind={} attackerIdentity='{}' hidden={} resolvedTarget=station body='{}'",
                 source ? source : "unknown",
                 targetType,
                 incoming_attack_policy_target_type_name(targetType),
                 targetFleetId,
                 attackerFleetType,
                 incoming_attack_policy_attacker_kind_name(attacker_kind),
                 attackerIdentity,
                 hide_notification,
                 body);
    live_debug_record_incoming_attack_notification_context(source ? source : "unknown",
                                                           body,
                                                           incoming_attack_candidate_count(),
                                                           0,
                                                           "",
                                                           static_cast<int>(FleetState::Unknown),
                                                           attackerFleetType);
    if (hide_notification) {
      return;
    }
    notification_show(title ? title : "Incoming Attack!", body.c_str());
    return;
  }

  const auto context = context_from_target_fleet(targetFleetId);
  const auto dedupe_fleet_id = targetFleetId != 0 ? targetFleetId : context.fleet_id;
  const bool hide_notification = should_hide_unknown_incoming_attack_notification(attacker_kind);

  if (!should_emit_incoming_attack_notification(source, dedupe_fleet_id, targetType, attacker_kind, true, attackerIdentity)) {
    return;
  }

  const auto body = build_incoming_attack_body(context, attacker_kind);
  auto title = incoming_attack_policy_title_for_kind(attacker_kind);
  spdlog::info("[IncomingAttack] source={} mode=targeted targetType={} targetTypeName={} rawTargetFleetId={} resolvedFleetId={} ship='{}' state={} attackerFleetType={} attackerKind={} attackerIdentity='{}' candidateCount={} hidden={} body='{}'",
               source ? source : "unknown",
               targetType,
               incoming_attack_policy_target_type_name(targetType),
               targetFleetId,
               context.fleet_id,
               context.ship_name,
               static_cast<int>(context.state),
               attackerFleetType,
               incoming_attack_policy_attacker_kind_name(attacker_kind),
               attackerIdentity,
               context.candidate_count,
               hide_notification,
               body);
  spdlog::debug("[IncomingAttack] notify source={} targetFleetId={} targetType={} attackerFleetType={} attackerKind={} candidateCount={} fleetId={} ship='{}' state={} body='{}'",
                source ? source : "unknown",
                targetFleetId,
                targetType,
                attackerFleetType,
                incoming_attack_policy_attacker_kind_name(attacker_kind),
                context.candidate_count,
                context.fleet_id,
                context.ship_name,
                static_cast<int>(context.state),
                body);
  live_debug_record_incoming_attack_notification_context(source ? source : "unknown",
                                                         body,
                                                         context.candidate_count,
                                                         context.fleet_id,
                                                         context.ship_name,
                                                         static_cast<int>(context.state),
                                                         attackerFleetType);
  if (hide_notification) {
    return;
  }
  notification_show(title ? title : "Incoming Attack!", body.c_str());
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
