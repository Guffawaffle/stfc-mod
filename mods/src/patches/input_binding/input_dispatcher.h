#pragma once

#include "patches/input_binding/input_binding.h"

#include <array>
#include <span>
#include <vector>

namespace input_binding
{
inline constexpr size_t kInputLayerCount = static_cast<size_t>(InputLayer::Zoom) + 1;

struct ActiveLayers {
  std::array<bool, kInputLayerCount> bits{true, true, true, true};

  [[nodiscard]] static constexpr ActiveLayers None();
  [[nodiscard]] static constexpr ActiveLayers All();
  [[nodiscard]] static constexpr ActiveLayers Only(InputLayer layer);

  constexpr void Set(InputLayer layer, bool active = true);
  [[nodiscard]] constexpr bool Contains(InputLayer layer) const;
};

struct DispatchRequest {
  TriggerMode   trigger_mode = TriggerMode::Down;
  InputPhase    phase = InputPhase::Frame;
  KeyCode       key = KeyCode::None;
  ModifierMask  held_modifiers;
  ActiveLayers  active_layers = ActiveLayers::All();
  bool          allow_extra_modifiers = false;
};

struct DispatchCandidate {
  InputActionId action = InputActionId::Max;
  ConflictGroup conflict_group = ConflictGroup::None;
  uint16_t      priority = 0;
  InputPhase    phase = InputPhase::Frame;
  InputLayer    layer = InputLayer::Global;
};

struct DispatchPlan {
  std::vector<DispatchCandidate> candidates;
  std::vector<DispatchCandidate> winners;

  [[nodiscard]] bool empty() const;
};

enum class ExecutionDecision : uint8_t {
  NoOpinion = 0,
  AllowOriginal,
  SuppressOriginal,
};

[[nodiscard]] DispatchPlan PlanDispatch(const CompileResult& compile, const DispatchRequest& request);
[[nodiscard]] ExecutionDecision CombineExecutionDecisions(std::span<const ExecutionDecision> decisions);

} // namespace input_binding

constexpr input_binding::ActiveLayers input_binding::ActiveLayers::None()
{ return {{{false, false, false, false}}}; }

constexpr input_binding::ActiveLayers input_binding::ActiveLayers::All()
{ return {{{true, true, true, true}}}; }

constexpr input_binding::ActiveLayers input_binding::ActiveLayers::Only(const InputLayer layer)
{
  auto active = None();
  active.Set(layer, true);
  return active;
}

constexpr void input_binding::ActiveLayers::Set(const InputLayer layer, const bool active)
{ bits[static_cast<size_t>(layer)] = active; }

constexpr bool input_binding::ActiveLayers::Contains(const InputLayer layer) const
{ return bits[static_cast<size_t>(layer)]; }