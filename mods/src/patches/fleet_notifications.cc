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

#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
std::unordered_map<uint64_t, FleetState>   s_fleet_bar_states;
std::unordered_map<uint64_t, FleetState>   s_fleet_bar_model_previous_states;
std::unordered_map<uint64_t, std::string>  s_fleet_bar_ship_names;
std::unordered_map<uint64_t, std::string>  s_fleet_bar_resource_names;
std::unordered_map<uint64_t, float>        s_fleet_bar_cargo_fill_levels;
std::unordered_map<uint64_t, int64_t>      s_mining_viewer_remaining_seconds;
std::chrono::steady_clock::time_point      s_last_incoming_attack_notification{};
uint64_t                                   s_last_incoming_attack_target_fleet_id = 0;

constexpr auto kIncomingAttackDedupWindow = std::chrono::seconds(10);

enum class IncomingAttackAttackerKind {
  Unknown = 0,
  Player = 1,
  Hostile = 2,
};

struct IncomingAttackNotificationContext {
  int         candidate_count = 0;
  uint64_t    fleet_id = 0;
  std::string ship_name;
  std::string resource_name;
  std::string cargo_text;
  FleetState  state = FleetState::Unknown;
};

IncomingAttackAttackerKind incoming_attack_attacker_kind_from_fleet_type(int attackerFleetType)
{
  switch (attackerFleetType) {
    case 1:
      return IncomingAttackAttackerKind::Player;
    case 2:
    case 3:
    case 4:
    case 6:
      return IncomingAttackAttackerKind::Hostile;
    default:
      return IncomingAttackAttackerKind::Unknown;
  }
}

const char* incoming_attack_attacker_kind_name(IncomingAttackAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackAttackerKind::Player:
      return "Player";
    case IncomingAttackAttackerKind::Hostile:
      return "Hostile";
    default:
      return "Unknown";
  }
}

std::string incoming_attack_subject_suffix(IncomingAttackAttackerKind attackerKind)
{
  switch (attackerKind) {
    case IncomingAttackAttackerKind::Player:
      return " by another player";
    case IncomingAttackAttackerKind::Hostile:
      return " by a hostile";
    default:
      return {};
  }
}

bool incoming_attack_notifications_enabled_for_kind(IncomingAttackAttackerKind attackerKind,
                                                    bool allow_when_unclassified)
{
  const auto& notifications = Config::Get().notifications;

  switch (attackerKind) {
    case IncomingAttackAttackerKind::Player:
      return notifications.incoming_attack_player;
    case IncomingAttackAttackerKind::Hostile:
      return notifications.incoming_attack_hostile;
    default:
      if (!notifications.AnyIncomingAttackEnabled()) {
        return false;
      }

      return allow_when_unclassified || !notifications.IncomingAttackSplitEnabled();
  }
}

bool should_hide_unknown_incoming_attack_notification(IncomingAttackAttackerKind attackerKind)
{
  return attackerKind == IncomingAttackAttackerKind::Unknown &&
      Config::Get().notifications.IncomingAttackSplitEnabled();
}

bool should_emit_incoming_attack_notification(const char* source,
                                              IncomingAttackAttackerKind attackerKind,
                                              bool allow_when_unclassified)
{
  if (!incoming_attack_notifications_enabled_for_kind(attackerKind, allow_when_unclassified)) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  if (s_last_incoming_attack_notification.time_since_epoch().count() != 0 &&
      now - s_last_incoming_attack_notification < kIncomingAttackDedupWindow) {
    spdlog::debug("[IncomingAttack] suppress {} notification inside dedupe window", source ? source : "unknown");
    return false;
  }

  s_last_incoming_attack_notification = now;
  return true;
}

bool should_notify_missed_incoming_attack(FleetState priorObservedState, FleetState currentState,
                                          FleetState modelPreviousState, FleetState priorModelPreviousState)
{
  if (priorObservedState == FleetState::Battling || currentState == FleetState::Battling) {
    return false;
  }

  if (modelPreviousState != FleetState::Battling) {
    return false;
  }

  return priorModelPreviousState != FleetState::Battling;
}

void maybe_notify_recent_incoming_attack(uint64_t fleetId, const std::string& shipName,
                                         FleetState priorObservedState, const std::string& resourceName,
                                         const std::string& cargoText)
{
  if (!should_emit_incoming_attack_notification("fleet-bar-recent", IncomingAttackAttackerKind::Unknown, false)) {
    return;
  }

  auto body = "Your " + shipName + " was attacked";
  if (priorObservedState == FleetState::Mining && !resourceName.empty()) {
    body += " while mining " + resourceName;
  }
  if (!cargoText.empty()) {
    body += " (" + cargoText + ")";
  }

  auto title = toast_state_title(IncomingAttack);
  spdlog::debug("[FleetBar] INCOMING_ATTACK_RECENT id={} ship='{}' resource='{}' cargo='{}'", fleetId, shipName,
                resourceName, cargoText);
  notification_show(title ? title : "Incoming Attack!", body.c_str());
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

int incoming_attack_candidate_score(FleetState state)
{
  switch (state) {
    case FleetState::Mining:
      return 100;
    case FleetState::IdleInSpace:
    case FleetState::CanRecall:
    case FleetState::CanLocate:
    case FleetState::Deployed:
    case FleetState::Capturing:
      return 80;
    case FleetState::Impulsing:
    case FleetState::Warping:
    case FleetState::WarpCharging:
      return 60;
    case FleetState::Docked:
    case FleetState::Destroyed:
    case FleetState::Repairing:
    case FleetState::TieringUp:
    case FleetState::CannotLaunch:
    case FleetState::Unknown:
      return 0;
    default:
      return 40;
  }
}

void populate_context_from_fleet_cache(IncomingAttackNotificationContext& context, uint64_t fleetId)
{
  context.fleet_id = fleetId;
  auto state_it = s_fleet_bar_states.find(fleetId);
  context.state = state_it != s_fleet_bar_states.end() ? state_it->second : FleetState::Unknown;
  context.ship_name = fleet_bar_cached_ship_name(fleetId);
  context.resource_name = fleet_bar_cached_resource_name(fleetId);
  context.cargo_text = fleet_cargo_fill_text(fleet_bar_cached_cargo_fill_level(fleetId));
}

int incoming_attack_candidate_count()
{
  int count = 0;
  for (const auto& [fleetId, state] : s_fleet_bar_states) {
    (void)fleetId;
    if (incoming_attack_candidate_score(state) > 0) {
      ++count;
    }
  }
  return count;
}

IncomingAttackNotificationContext infer_incoming_attack_context_from_fleet_cache()
{
  IncomingAttackNotificationContext result;
  int best_score = 0;
  bool best_score_tied = false;

  for (const auto& [fleetId, state] : s_fleet_bar_states) {
    const auto score = incoming_attack_candidate_score(state);
    if (score <= 0) {
      continue;
    }

    ++result.candidate_count;
    if (score < best_score) {
      continue;
    }

    if (score == best_score && result.fleet_id != 0) {
      best_score_tied = true;
      continue;
    }

    best_score = score;
    best_score_tied = false;
    populate_context_from_fleet_cache(result, fleetId);
  }

  if (best_score_tied && s_last_incoming_attack_target_fleet_id != 0) {
    auto last_state_it = s_fleet_bar_states.find(s_last_incoming_attack_target_fleet_id);
    if (last_state_it != s_fleet_bar_states.end() && incoming_attack_candidate_score(last_state_it->second) == best_score) {
      populate_context_from_fleet_cache(result, s_last_incoming_attack_target_fleet_id);
    }
  }

  return result;
}

IncomingAttackNotificationContext context_from_target_fleet(uint64_t targetFleetId)
{
  auto context = infer_incoming_attack_context_from_fleet_cache();
  if (targetFleetId == 0) {
    return context;
  }

  IncomingAttackNotificationContext targeted;
  targeted.candidate_count = incoming_attack_candidate_count();
  populate_context_from_fleet_cache(targeted, targetFleetId);
  if (!targeted.ship_name.empty()) {
    return targeted;
  }

  return context;
}

std::string build_incoming_attack_body(const IncomingAttackNotificationContext& context,
                                       IncomingAttackAttackerKind attackerKind = IncomingAttackAttackerKind::Unknown)
{
  const auto subject_suffix = incoming_attack_subject_suffix(attackerKind);

  if (context.ship_name.empty() && context.fleet_id != 0) {
    return "Your fleet is under attack" + subject_suffix;
  }

  if (context.ship_name.empty() || context.fleet_id == 0) {
    if (attackerKind == IncomingAttackAttackerKind::Hostile) {
      return "Open STFC to inspect the hostile and respond.";
    }

    return "Open STFC to inspect the attacker and respond.";
  }

  auto body = "Your " + context.ship_name + " is under attack" + subject_suffix;
  if (context.state == FleetState::Mining && !context.resource_name.empty()) {
    body += " while mining " + context.resource_name;
  }
  if (!context.cargo_text.empty()) {
    body += " (" + context.cargo_text + ")";
  }

  return body;
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

  if (oldState == FleetState::Mining && newState == FleetState::Battling) {
    if (!should_emit_incoming_attack_notification("fleet-bar-transition", IncomingAttackAttackerKind::Unknown, false)) {
      return;
    }

    auto body = "Your " + shipName + " was attacked";
    if (!resourceName.empty()) {
      body += " while mining " + resourceName;
    }
    if (!cargoText.empty()) {
      body += " (" + cargoText + ")";
    }

    auto title = toast_state_title(IncomingAttack);
    spdlog::debug("[FleetBar] INCOMING_ATTACK id={} ship='{}' resource='{}' cargo='{}'", fleetId, shipName,
                  resourceName, cargoText);
    notification_show(title ? title : "Incoming Attack!", body.c_str());
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
  auto modelPreviousState = fleet->PreviousState;
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

  auto previousModelState    = FleetState::Unknown;
  auto hadPreviousModelState = false;
  auto previousModelIt       = s_fleet_bar_model_previous_states.find(fleetId);
  if (previousModelIt != s_fleet_bar_model_previous_states.end()) {
    previousModelState    = previousModelIt->second;
    hadPreviousModelState = true;
  }

  s_fleet_bar_ship_names[fleetId] = shipName;
  if (!resourceName.empty()) {
    s_fleet_bar_resource_names[fleetId] = resourceName;
  }
  s_fleet_bar_cargo_fill_levels[fleetId] = cargoFillLevel;

  if (hadPreviousState && previousState != currentState) {
    maybe_notify_fleet_bar_transition(fleetId, shipName, previousState, currentState, resourceName, cargoText);
  }

  if (hadPreviousState && hadPreviousModelState
      && should_notify_missed_incoming_attack(previousState, currentState, modelPreviousState, previousModelState)) {
    maybe_notify_recent_incoming_attack(fleetId, shipName, previousState, resourceName, cargoText);
  }

  s_fleet_bar_states[fleetId] = currentState;
  s_fleet_bar_model_previous_states[fleetId] = modelPreviousState;
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

void fleet_notifications_notify_incoming_attack_detected(const char* source)
{
  if (!should_emit_incoming_attack_notification(source, IncomingAttackAttackerKind::Unknown, false)) {
    return;
  }

  const auto context = infer_incoming_attack_context_from_fleet_cache();
  const auto body = build_incoming_attack_body(context);
  if (context.fleet_id != 0) {
    s_last_incoming_attack_target_fleet_id = context.fleet_id;
  }
  auto title = toast_state_title(IncomingAttack);
  spdlog::debug("[IncomingAttack] notify source={} candidateCount={} fleetId={} ship='{}' state={} body='{}'",
                source ? source : "unknown",
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
                                                         0);
  notification_show(title ? title : "Incoming Attack!", body.c_str());
}

void fleet_notifications_notify_incoming_attack_target(const char* source, uint64_t targetFleetId, int targetType,
                                                       int attackerFleetType)
{
  const auto attacker_kind = incoming_attack_attacker_kind_from_fleet_type(attackerFleetType);
  const bool hide_notification = should_hide_unknown_incoming_attack_notification(attacker_kind);

  if (!should_emit_incoming_attack_notification(source, attacker_kind, true)) {
    return;
  }

  if (targetType == static_cast<int>(NotificationIncomingAttackTargetType::Station)) {
    auto body = std::string{"Your station is under attack"} + incoming_attack_subject_suffix(attacker_kind);
    auto title = toast_state_title(IncomingAttack);
    live_debug_record_incoming_attack_notification_context(source ? source : "unknown",
                                                           body,
                                                           incoming_attack_candidate_count(),
                                                           0,
                                                           "",
                                                           static_cast<int>(FleetState::Unknown),
                                                           attackerFleetType);
    if (hide_notification) {
      notification_show_hidden(title ? title : "Incoming Attack!", body.c_str());
    } else {
      notification_show(title ? title : "Incoming Attack!", body.c_str());
    }
    return;
  }

  const auto context = context_from_target_fleet(targetFleetId);
  const auto body = build_incoming_attack_body(context, attacker_kind);
  if (context.fleet_id != 0) {
    s_last_incoming_attack_target_fleet_id = context.fleet_id;
  }
  auto title = toast_state_title(IncomingAttack);
  spdlog::debug("[IncomingAttack] notify source={} targetFleetId={} targetType={} attackerFleetType={} attackerKind={} candidateCount={} fleetId={} ship='{}' state={} body='{}'",
                source ? source : "unknown",
                targetFleetId,
                targetType,
                attackerFleetType,
                incoming_attack_attacker_kind_name(attacker_kind),
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
    notification_show_hidden(title ? title : "Incoming Attack!", body.c_str());
  } else {
    notification_show(title ? title : "Incoming Attack!", body.c_str());
  }
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
