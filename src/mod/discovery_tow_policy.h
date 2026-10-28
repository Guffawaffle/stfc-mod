#pragma once

// Copied observations only. Unity owns coroutine advancement and nested yields.
#include <cstdint>
#include <optional>

namespace discovery_tow
{
struct Request {
  std::uint64_t  generation = 0;
  std::int64_t   selected = 0, discovery = 0;
  std::int32_t   selected_index = -1, discovery_index = -1, section = 0;
  std::uintptr_t owner = 0, selected_object = 0, discovery_object = 0, baseline = 0;
  std::int64_t   deadline_ms = 0;
};
struct Evidence {
  std::uint64_t                 generation = 0;
  std::int64_t                  now_ms     = 0;
  bool                          input_mode = false, owner_alive = false;
  std::optional<std::int32_t>   section;
  std::optional<std::int64_t>   selected, discovery;
  std::uintptr_t                owner = 0, selected_object = 0, discovery_object = 0;
  std::optional<bool>           destination_matches, can_tow, can_be_towed;
  std::optional<std::int32_t>   iterator_state;
  std::optional<std::uintptr_t> course;
  std::optional<std::int64_t>   course_fleet;
  std::optional<bool>           course_valid, course_in_use, course_destination_matches;
};
enum class Decision { Cancel, Wait, Submit };

// Planning and scheduling need the same identity/input fence as submission, but
// do not yet have an iterator. Keep that qualification independent of its state.
inline bool SourceMatches(const Request& request, const Evidence& observed) noexcept
{
  if (!request.generation || request.generation != observed.generation || request.selected <= 0
      || request.discovery <= 0 || request.selected == request.discovery || request.selected_index < 0
      || request.discovery_index < 0 || request.selected_index == request.discovery_index || !request.owner
      || !request.selected_object || !request.discovery_object || request.selected_object == request.discovery_object
      || observed.now_ms >= request.deadline_ms || !observed.input_mode || !observed.owner_alive
      || observed.section != request.section || observed.owner != request.owner || observed.selected != request.selected
      || observed.discovery != request.discovery || observed.selected_object != request.selected_object
      || observed.discovery_object != request.discovery_object || observed.destination_matches != true
      || observed.can_tow != true || observed.can_be_towed != true)
    return false;
  return true;
}

inline Decision Evaluate(const Request& request, const Evidence& observed) noexcept
{
  if (!SourceMatches(request, observed) || !observed.iterator_state)
    return Decision::Cancel;
  // Retained client273's exact PlanCourse player-fleet iterator yields in states
  // 1/2 and completes at -1. Never run MoveNext here, including while state=0.
  if (*observed.iterator_state >= 0 && *observed.iterator_state <= 2)
    return Decision::Wait;
  if (*observed.iterator_state != -1 || !observed.course || !*observed.course || *observed.course == request.baseline
      || observed.course_fleet != request.selected || observed.course_valid != true || observed.course_in_use != false
      || observed.course_destination_matches != true)
    return Decision::Cancel;
  return Decision::Submit;
}
} // namespace discovery_tow
