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

  for (const auto& candidate : plan.candidates) {
    if (candidate.conflict_group != ConflictGroup::None) {
      const auto group_index = static_cast<size_t>(candidate.conflict_group);
      if (used_groups[group_index]) {
        continue;
      }
      used_groups[group_index] = true;
    }

    plan.winners.push_back(candidate);
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

DispatchPlan PlanDispatch(const CompileResult& compile, const DispatchRequest& request)
{
  DispatchPlan plan;

  append_candidates(compile, request, plan);
  select_winners(plan);
  return plan;
}

DispatchPlan PlanDispatchSnapshot(const CompileResult& compile,
                                  const InputPhase phase,
                                  const ActiveLayers active_layers,
                                  const std::span<const DispatchKeyState> key_states,
                                  const bool allow_extra_modifiers)
{
  DispatchPlan plan;

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

  return plan;
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