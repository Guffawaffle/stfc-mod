#include "patches/input_binding/input_dispatcher.h"

namespace input_binding
{
bool DispatchPlan::empty() const
{ return winners.empty(); }

DispatchPlan PlanDispatch(const CompileResult& compile, const DispatchRequest& request)
{
  DispatchPlan plan;
  auto         matched_actions = compile.index.Match(request.trigger_mode, request.key, request.held_modifiers,
                                                     request.allow_extra_modifiers);
  std::array<bool, static_cast<size_t>(ConflictGroup::Zoom) + 1> used_groups{};

  plan.candidates.reserve(matched_actions.size());
  plan.winners.reserve(matched_actions.size());

  for (const auto action : matched_actions) {
    const auto* spec = FindActionSpec(action);
    if (!spec || spec->phase != request.phase || !request.active_layers.Contains(spec->layer)) {
      continue;
    }

    DispatchCandidate candidate{action, spec->conflict_group, spec->priority, spec->phase, spec->layer};
    plan.candidates.push_back(candidate);

    if (candidate.conflict_group != ConflictGroup::None) {
      const auto group_index = static_cast<size_t>(candidate.conflict_group);
      if (used_groups[group_index]) {
        continue;
      }
      used_groups[group_index] = true;
    }

    plan.winners.push_back(candidate);
  }

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