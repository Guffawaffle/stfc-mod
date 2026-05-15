/**
 * @file cargo_display.cc
 * @brief Automatic cargo/rewards panel display based on target type and config.
 *
 * Evaluates the selected target's fleet type (player, hostile, armada, station)
 * against per-type config toggles to decide whether to auto-show the cargo
 * rewards panel. Called from hook delegates in the hotkey router.
 */
#include "config.h"
#include "errormsg.h"

#include "patches/cargo_display.h"
#include "patches/viewer_mgmt.h"

#include "prime/PreScanTargetWidget.h"

#include <spdlog/spdlog.h>

namespace
{
bool VisibleOrShowing(VisibilityController* controller)
{
  return controller && (controller->_state == VisibilityState::Visible || controller->_state == VisibilityState::Show);
}

const char* VisibilityStateName(VisibilityState state)
{
  switch (state) {
  case VisibilityState::Unknown:
    return "Unknown";
  case VisibilityState::Show:
    return "Show";
  case VisibilityState::Hide:
    return "Hide";
  case VisibilityState::Hidden:
    return "Hidden";
  case VisibilityState::Visible:
    return "Visible";
  }
  return "Unexpected";
}

const char* FleetTypeName(DeployedFleetType type)
{
  switch (type) {
  case DeployedFleetType::Nonexistent:
    return "Nonexistent";
  case DeployedFleetType::Player:
    return "Player";
  case DeployedFleetType::Marauder:
    return "Marauder";
  case DeployedFleetType::NpcInstantiated:
    return "NpcInstantiated";
  case DeployedFleetType::Sentinel:
    return "Sentinel";
  case DeployedFleetType::Alliance:
    return "Alliance";
  }
  return "Unexpected";
}

const char* HullTypeName(HullType type)
{
  switch (type) {
  case HullType::Any:
    return "Any";
  case HullType::Destroyer:
    return "Destroyer";
  case HullType::Survey:
    return "Survey";
  case HullType::Explorer:
    return "Explorer";
  case HullType::Battleship:
    return "Battleship";
  case HullType::Defense:
    return "Defense";
  case HullType::ArmadaTarget:
    return "ArmadaTarget";
  }
  return "Unexpected";
}

struct CargoDecision {
  bool show;
  const char* reason;
  BattleTargetData* context;
  FleetDeployedData* target_fleet;
  HullSpec* hull;
  DeployedFleetType fleet_type;
  HullType hull_type;
  bool show_cargo_default;
  bool show_player_cargo;
  bool show_station_cargo;
  bool show_hostile_cargo;
  bool show_armada_cargo;
};

CargoDecision EvaluateCargoDecision(RewardsButtonWidget* widget)
{
  CargoDecision decision{
      .show               = false,
      .reason             = "unknown",
      .context            = nullptr,
      .target_fleet       = nullptr,
      .hull               = nullptr,
      .fleet_type         = DeployedFleetType::Nonexistent,
      .hull_type          = HullType::Any,
      .show_cargo_default = Config::Get().show_cargo_default,
      .show_player_cargo  = Config::Get().show_player_cargo,
      .show_station_cargo = Config::Get().show_station_cargo,
      .show_hostile_cargo = Config::Get().show_hostile_cargo,
      .show_armada_cargo  = Config::Get().show_armada_cargo,
  };

  if (!decision.show_cargo_default) {
    decision.reason = "default-disabled";
    return decision;
  }

  if (!widget) {
    decision.reason = "missing-widget";
    return decision;
  }

  decision.context = widget->Context;
  if (!decision.context) {
    decision.reason = "missing-context";
    return decision;
  }

  decision.target_fleet = decision.context->TargetFleetDeployedData;
  if (!decision.target_fleet) {
    decision.show   = decision.show_station_cargo;
    decision.reason = decision.show ? "station-enabled" : "station-disabled";
    return decision;
  }

  decision.fleet_type = decision.target_fleet->FleetType;
  if (decision.fleet_type == DeployedFleetType::Player) {
    decision.show   = decision.show_player_cargo;
    decision.reason = decision.show ? "player-enabled" : "player-disabled";
    return decision;
  }

  if (decision.fleet_type == DeployedFleetType::Marauder) {
    decision.hull = decision.target_fleet->Hull;
    if (decision.hull) {
      decision.hull_type = decision.hull->Type;
    }

    if (decision.hull && decision.hull_type == HullType::ArmadaTarget) {
      decision.show   = decision.show_armada_cargo;
      decision.reason = decision.show ? "armada-enabled" : "armada-disabled";
      return decision;
    }

    decision.show   = decision.show_hostile_cargo;
    decision.reason = decision.show ? "hostile-enabled" : "hostile-disabled";
    return decision;
  }

  decision.reason = "unsupported-fleet-type";
  return decision;
}

void LogCargoDecision(const char* source, PreScanTargetWidget* pre_scan_widget, RewardsButtonWidget* rewards_widget,
                      const CargoDecision& decision)
{
  auto* pre_scan_visibility = pre_scan_widget ? pre_scan_widget->_visibilityController : nullptr;
  auto* rewards_controller  = rewards_widget ? rewards_widget->_rewardsController : nullptr;

  spdlog::trace("[CargoDisplay] source={} pre_scan={:p} pre_scan_state={} rewards_widget={:p} "
                "rewards_controller={:p} rewards_state={} context={:p} target_fleet={:p} fleet_type={} hull={:p} "
                "hull_type={} decision={} reason={} show_default={} player={} station={} hostile={} armada={}",
                source, static_cast<void*>(pre_scan_widget),
                pre_scan_visibility ? VisibilityStateName(pre_scan_visibility->_state) : "null",
                static_cast<void*>(rewards_widget), static_cast<void*>(rewards_controller),
                rewards_controller ? VisibilityStateName(rewards_controller->_state) : "null",
                static_cast<void*>(decision.context), static_cast<void*>(decision.target_fleet),
                FleetTypeName(decision.fleet_type), static_cast<void*>(decision.hull), HullTypeName(decision.hull_type),
                decision.show, decision.reason, decision.show_cargo_default, decision.show_player_cargo,
                decision.show_station_cargo, decision.show_hostile_cargo, decision.show_armada_cargo);
}
} // namespace

bool CheckShowCargo(RewardsButtonWidget* widget)
{
  return EvaluateCargoDecision(widget).show;
}

void HandleCargoBindContext(RewardsButtonWidget* _this)
{
  const auto decision = EvaluateCargoDecision(_this);
  LogCargoDecision("bind-context", nullptr, _this, decision);
  if (decision.show) {
    _this->_rewardsController->Show(true);
    SetInfoPending(1);
  }
}

void HandleCargoShowFleet(PreScanTargetWidget* _this)
{
  auto rewards_button_widget = _this->_rewardsButtonWidget;
  const auto decision        = EvaluateCargoDecision(rewards_button_widget);
  LogCargoDecision("show-fleet", _this, rewards_button_widget, decision);
  if (decision.show) {
    rewards_button_widget->_rewardsController->Show(true);
    SetInfoPending(1);
  }
}

void RefreshVisibleCargoDisplays()
{
  auto visible_targets = 0;
  auto shown_panels    = 0;
  auto hidden_panels   = 0;

  const auto pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAllNonNull();
  for (auto* pre_scan_widget : pre_scan_widgets) {
    if (!pre_scan_widget || !VisibleOrShowing(pre_scan_widget->_visibilityController)) {
      continue;
    }

    ++visible_targets;

    auto rewards_widget = pre_scan_widget->_rewardsButtonWidget;
    if (!rewards_widget || !rewards_widget->_rewardsController) {
      const auto decision = EvaluateCargoDecision(rewards_widget);
      LogCargoDecision("refresh-missing-rewards", pre_scan_widget, rewards_widget, decision);
      continue;
    }

    const auto decision = EvaluateCargoDecision(rewards_widget);
    LogCargoDecision("refresh-visible", pre_scan_widget, rewards_widget, decision);
    if (decision.show) {
      rewards_widget->_rewardsController->Show(true);
      SetInfoPending(1);
      ++shown_panels;
      continue;
    }

    if (VisibleOrShowing(rewards_widget->_rewardsController)) {
      rewards_widget->_rewardsController->Hide(true);
      ++hidden_panels;
    }
  }

  spdlog::debug("[CargoDisplay] refresh visible_targets={} shown_panels={} hidden_panels={} show_default={} player={} "
                "station={} hostile={} armada={}",
                visible_targets, shown_panels, hidden_panels, Config::Get().show_cargo_default,
                Config::Get().show_player_cargo, Config::Get().show_station_cargo, Config::Get().show_hostile_cargo,
                Config::Get().show_armada_cargo);
}
