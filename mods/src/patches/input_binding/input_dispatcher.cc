#include "patches/input_binding/input_dispatcher.h"

#include <algorithm>

namespace input_binding
{
namespace {
using UsedConflictGroups = std::array<bool, static_cast<size_t>(ConflictGroup::Zoom) + 1>;

bool candidate_allowed(const InputActionSpec& spec, const DispatchRequest& request)
{ return spec.phase == request.phase && request.active_layers.Contains(spec.layer); }

void sort_by_priority(std::vector<DispatchCandidate>& candidates)
{
  std::ranges::sort(candidates, [](const auto& lhs, const auto& rhs) {
    if (lhs.priority != rhs.priority) {
      return lhs.priority > rhs.priority;
    }
    return static_cast<uint16_t>(lhs.action) < static_cast<uint16_t>(rhs.action);
  });
}

void select_winners(DispatchPlan& plan)
{
  UsedConflictGroups used_groups{};
  sort_by_priority(plan.candidates);
  plan.winners.clear();
  plan.winners.reserve(plan.candidates.size());
  plan.winner_lookup.Reset();

  for (const auto& candidate : plan.candidates) {
    if (candidate.conflict_group != ConflictGroup::None) {
      const auto group_index = static_cast<size_t>(candidate.conflict_group);
      if (used_groups[group_index]) {
        continue;
      }
      used_groups[group_index] = true;
    }

    plan.winners.push_back(candidate);
    plan.winner_lookup.Add(candidate, plan.winners.size() - 1);
  }
}

void append_candidates(const CompileResult& compile, const DispatchRequest& request, DispatchPlan& plan)
{
  auto matched_actions = compile.index.Match(request.trigger_mode, request.key, request.held_modifiers,
                                             request.allow_extra_modifiers);

  for (const auto action : matched_actions) {
    const auto* spec = FindActionSpec(action);
    if (!spec || !candidate_allowed(*spec, request)) {
      continue;
    }

    DispatchCandidate candidate{action, spec->conflict_group, spec->priority, spec->phase, spec->layer};
    plan.candidates.push_back(candidate);
  }
}
}

bool DispatchPlan::empty() const
{ return winners.empty(); }

void DispatchWinnerLookup::Reset()
{
  winner_order.fill(0);
  for (auto& layers : action_layers) {
    layers.fill(false);
  }
}

void DispatchWinnerLookup::Add(const DispatchCandidate& candidate, const size_t winner_index)
{
  const auto action_index = static_cast<size_t>(candidate.action);
  if (winner_order[action_index] == 0) {
    winner_order[action_index] = static_cast<uint16_t>(winner_index + 1);
  }

  action_layers[action_index][static_cast<size_t>(candidate.layer)] = true;
}

bool DispatchWinnerLookup::Contains(const InputActionId action, const InputLayer layer) const
{
  return action_layers[static_cast<size_t>(action)][static_cast<size_t>(layer)];
}

InputActionId DispatchWinnerLookup::First(const std::span<const InputActionId> actions) const
{
  auto          best_order  = uint16_t{0};
  auto          best_action = InputActionId::Max;

  for (const auto action : actions) {
    const auto order = winner_order[static_cast<size_t>(action)];
    if (order == 0) {
      continue;
    }

    if (best_order == 0 || order < best_order) {
      best_order  = order;
      best_action = action;
    }
  }

  return best_action;
}

DispatchPlan PlanDispatch(const CompileResult& compile, const DispatchRequest& request)
{
  DispatchPlan plan;

  append_candidates(compile, request, plan);
  select_winners(plan);
  return plan;
}

void PlanDispatchSnapshot(const CompileResult& compile,
                          const InputPhase phase,
                          const ActiveLayers active_layers,
                          const std::span<const DispatchKeyState> key_states,
                          DispatchPlan& plan,
                          const bool allow_extra_modifiers)
{
  plan.candidates.clear();
  plan.winners.clear();
  plan.winner_lookup.Reset();
  plan.candidates.reserve(key_states.size());
  plan.winners.reserve(key_states.size());

  for (const auto& key_state : key_states) {
    if (key_state.key == KeyCode::None) {
      continue;
    }

    if (key_state.down) {
      append_candidates(compile,
                        DispatchRequest{TriggerMode::Down, phase, key_state.key, key_state.held_modifiers,
                                        active_layers, allow_extra_modifiers},
                        plan);
    }

    if (key_state.pressed) {
      append_candidates(compile,
                        DispatchRequest{TriggerMode::Pressed, phase, key_state.key, key_state.held_modifiers,
                                        active_layers, allow_extra_modifiers},
                        plan);
    }
  }

  select_winners(plan);
}

DispatchPlan PlanDispatchSnapshot(const CompileResult& compile,
                                  const InputPhase phase,
                                  const ActiveLayers active_layers,
                                  const std::span<const DispatchKeyState> key_states,
                                  const bool allow_extra_modifiers)
{
  DispatchPlan plan;
  PlanDispatchSnapshot(compile, phase, active_layers, key_states, plan, allow_extra_modifiers);

  return plan;
}

std::vector<KeyCode> WatchedKeysForActions(const CompileResult& compile,
                                           const InputPhase phase,
                                           const std::span<const InputActionId> actions)
{
  std::vector<KeyCode> watched_keys;
  std::array<bool, static_cast<size_t>(KeyCode::Max) + 1> seen_keys{};

  watched_keys.reserve(compile.bindings.size());
  for (const auto& binding : compile.bindings) {
    if (std::ranges::find(actions, binding.action) == actions.end()) {
      continue;
    }

    const auto* spec = FindActionSpec(binding.action);
    if (!spec || spec->phase != phase || binding.chord.key == KeyCode::None) {
      continue;
    }

    const auto key_index = static_cast<size_t>(binding.chord.key);
    if (seen_keys[key_index]) {
      continue;
    }

    seen_keys[key_index] = true;
    watched_keys.push_back(binding.chord.key);
  }

  return watched_keys;
}

ExecutionDecision CombineExecutionDecisions(const std::span<const ExecutionDecision> decisions)
{
  bool allow_original = false;

  for (const auto decision : decisions) {
    if (decision == ExecutionDecision::SuppressOriginal) {
      return ExecutionDecision::SuppressOriginal;
    }
    if (decision == ExecutionDecision::AllowOriginal) {
      allow_original = true;
    }
  }

  return allow_original ? ExecutionDecision::AllowOriginal : ExecutionDecision::NoOpinion;
}
} // namespace input_binding