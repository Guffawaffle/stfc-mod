/**
 * @file hotkey_router_runtime_query.cc
 * @brief Implementation of the runtime-binding query / dispatch helpers.
 */
#include "patches/hotkey_router_runtime_query.h"

#include "patches/fleet_actions.h"
#include "patches/hotkey_router_trace_log.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "testable_functions.h"

#include "prime/FleetBarViewController.h"
#include "prime/Hub.h"

#include <spdlog/spdlog.h>

#include <array>

namespace hotkey_router_runtime_query
{
bool runtime_binding_winner_present(const input_binding::DispatchPlan& plan, const input_binding::InputActionId action,
                                    const input_binding::InputLayer layer)
{ return plan.winner_lookup.Contains(action, layer); }

const input_binding::DispatchCandidate* runtime_binding_winner(const input_binding::DispatchPlan& plan,
                                                               const input_binding::InputActionId action,
                                                               const input_binding::InputLayer    layer)
{
  for (const auto& winner : plan.winners) {
    if (winner.action == action && winner.layer == layer) {
      return &winner;
    }
  }
  return nullptr;
}

bool runtime_binding_consumes_original_key_event(const input_binding::DispatchPlan& plan,
                                                 const input_binding::InputActionId action,
                                                 const input_binding::InputLayer    layer)
{
  const auto* winner = runtime_binding_winner(plan, action, layer);
  return winner && input_binding::ConsumesOriginalKeyEvent(*winner);
}

input_binding::InputActionId first_runtime_binding_winner(const input_binding::DispatchPlan&                  plan,
                                                          const std::span<const input_binding::InputActionId> actions)
{ return plan.winner_lookup.First(actions); }

int ship_select_request_from_runtime_bindings(const input_binding::DispatchPlan& plan)
{
  return hotkey_router_ship_select_request(std::array<bool, 8>{
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip1, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip2, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip3, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip4, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip5, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip6, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip7, input_binding::InputLayer::Fleet),
      runtime_binding_winner_present(plan, input_binding::InputActionId::SelectShip8, input_binding::InputLayer::Fleet),
  });
}

GameFunction dispatcher_owned_game_function(const input_binding::InputActionId action)
{ return input_binding::ActionGameFunction(action); }

HotkeyRouterDispatchAction dispatch_runtime_bound_table_action(const input_binding::DispatchCandidate& candidate)
{
  using hotkey_router_trace_log::dispatch_decision_name;
  using hotkey_router_trace_log::game_function_name;
  using hotkey_router_trace_log::hotkey_trace_action;
  using hotkey_router_trace_log::input_action_name;
  using hotkey_router_trace_log::key_code_name;
  using hotkey_router_trace_log::router_dispatch_action_name;
  using hotkey_router_trace_log::trigger_mode_name;

  const auto action        = candidate.action;
  const auto game_function = dispatcher_owned_game_function(action);
  if (game_function == GameFunction::Max) {
    if (hotkey_trace_action(action)) {
      spdlog::trace(
          "[HotkeyTrace] table-dispatch action={} key={} game_function=Max result=continue reason=no-game-function",
          input_action_name(action), key_code_name(candidate.key));
    }
    return HotkeyRouterDispatchAction::Continue;
  }

  for (const auto& entry : GetHotkeyDispatchTable()) {
    if (entry.game_function != game_function) {
      continue;
    }

    const auto decision      = entry.handler();
    const auto router_action = hotkey_router_dispatch_action(true, decision == DispatchDecision::HandledStop,
                                                             decision == DispatchDecision::HandledAllowOriginal,
                                                             input_binding::ConsumesOriginalKeyEvent(candidate));
    if (hotkey_trace_action(action)) {
      spdlog::trace("[HotkeyTrace] table-dispatch action={} key={} trigger={} game_function={} handler_decision={} "
                    "consumes_original={} router_action={}",
                    input_action_name(action), key_code_name(candidate.key), trigger_mode_name(candidate.trigger_mode),
                    game_function_name(game_function), dispatch_decision_name(decision),
                    input_binding::ConsumesOriginalKeyEvent(candidate), router_dispatch_action_name(router_action));
    }
    return router_action;
  }

  if (hotkey_trace_action(action)) {
    spdlog::trace(
        "[HotkeyTrace] table-dispatch action={} key={} game_function={} result=continue reason=no-table-entry",
        input_action_name(action), key_code_name(candidate.key), game_function_name(game_function));
  }

  return HotkeyRouterDispatchAction::Continue;
}

void dispatch_runtime_bound_simple_fleet_action(const input_binding::InputActionId action)
{
  switch (action) {
    case input_binding::InputActionId::FleetQueueClear:
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat()) {
        if (auto fleet_bar = ObjectFinder<FleetBarViewController>::Get(); fleet_bar) {
          ClearFleetActionQueue(fleet_bar);
        }
      }
      break;
    default:
      break;
  }
}

HotkeyRouterStartupAction startup_action_from_runtime_bindings(const input_binding::DispatchPlan& plan,
                                                               const ScopelyShortcutPolicy        scopely_shortcuts,
                                                               const bool                         hotkeys_enabled)
{
  return hotkey_router_startup_action(runtime_binding_winner_present(plan, input_binding::InputActionId::HotkeysDisable,
                                                                     input_binding::InputLayer::Global),
                                      runtime_binding_winner_present(plan, input_binding::InputActionId::HotkeysEnable,
                                                                     input_binding::InputLayer::Global),
                                      scopely_shortcuts, hotkeys_enabled);
}
}  // namespace hotkey_router_runtime_query
