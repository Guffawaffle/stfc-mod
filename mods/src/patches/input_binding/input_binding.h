#pragma once

#include "prime/KeyCode.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace input_binding
{
enum class InputActionId : uint16_t {
  FleetPrimary = 0,
  FleetSecondary,
  FleetService,
  FleetViewInfo,
  FleetQueueClear,
  FleetQueueToggle,
  HotkeysDisable,
  HotkeysEnable,
  LogDebug,
  ZoomIn,
  ZoomOut,
  Max,
};

enum class TriggerMode : uint8_t {
  Down = 0,
  Pressed,
};

enum class InputPhase : uint8_t {
  Frame = 0,
  NavigationZoomUpdate,
};

enum class InputLayer : uint8_t {
  Global = 0,
  Fleet,
  Diagnostics,
  Zoom,
};

enum class ConflictGroup : uint8_t {
  None = 0,
  FleetAction,
  GlobalControl,
  Diagnostics,
  Zoom,
};

struct InputActionSpec {
  InputActionId    id;
  std::string_view canonical_key;
  std::string_view default_bind;
  TriggerMode      trigger_mode;
  InputPhase       phase;
  InputLayer       layer;
  ConflictGroup    conflict_group;
  uint16_t         priority;
};

enum class ModifierGroup : uint8_t {
  Shift = 0,
  Ctrl,
  Alt,
  Win,
  Command,
  AltGr,
};

enum class PhysicalModifier : uint8_t {
  LeftShift = 0,
  RightShift,
  LeftControl,
  RightControl,
  LeftAlt,
  RightAlt,
  LeftWindows,
  RightWindows,
  LeftCommand,
  RightCommand,
  AltGr,
};

class ModifierMask
{
public:
  constexpr ModifierMask() = default;

  [[nodiscard]] static constexpr ModifierMask Logical(ModifierGroup group);
  [[nodiscard]] static constexpr ModifierMask Physical(PhysicalModifier modifier);
  [[nodiscard]] static constexpr ModifierMask FromPressedKey(KeyCode key);

  constexpr void AddLogical(ModifierGroup group);
  constexpr void AddPhysical(PhysicalModifier modifier);
  constexpr void Merge(ModifierMask other);

  [[nodiscard]] constexpr bool empty() const;
  [[nodiscard]] constexpr bool IsSatisfiedBy(ModifierMask held) const;
  [[nodiscard]] constexpr bool IsExactMatch(ModifierMask held) const;
  [[nodiscard]] constexpr uint32_t logical_bits() const;
  [[nodiscard]] constexpr uint32_t physical_bits() const;
  [[nodiscard]] constexpr uint32_t effective_logical_bits() const;

private:
  uint32_t logical_bits_ = 0;
  uint32_t physical_bits_ = 0;
};

enum class DiagnosticSeverity : uint8_t {
  Info = 0,
  Warning,
  Error,
};

struct BindingDiagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Info;
  std::string        message;
  size_t             token_index = 0;
};

struct ParsedChord {
  KeyCode      key = KeyCode::None;
  ModifierMask modifiers;
  std::string  display;
  std::vector<BindingDiagnostic> diagnostics;
  bool         valid = false;

  [[nodiscard]] bool Matches(KeyCode pressed_key, ModifierMask held_modifiers,
                             bool allow_extra_modifiers = false) const;
};

struct ParsedBinding {
  std::vector<ParsedChord>      chords;
  std::vector<BindingDiagnostic> diagnostics;
  bool                          unbound = false;

  [[nodiscard]] bool has_valid_chord() const;
  [[nodiscard]] bool has_warnings() const;
  [[nodiscard]] bool has_errors() const;
  [[nodiscard]] std::string DisplayString() const;
};

struct CompiledBinding {
  InputActionId action = InputActionId::Max;
  ParsedChord   chord;
  TriggerMode   trigger_mode = TriggerMode::Down;
  uint16_t      priority = 0;
};

class BindingIndex
{
public:
  void Register(InputActionId action, ParsedChord chord, TriggerMode trigger_mode, uint16_t priority = 0);

  [[nodiscard]] std::vector<InputActionId> Match(TriggerMode trigger_mode, KeyCode key, ModifierMask held_modifiers,
                                                 bool allow_extra_modifiers = false) const;
  [[nodiscard]] size_t size() const;

private:
  static constexpr size_t kKeyCodeCount = static_cast<size_t>(KeyCode::Max) + 1;
  using BindingBuckets = std::array<std::vector<CompiledBinding>, kKeyCodeCount>;

  [[nodiscard]] const BindingBuckets& buckets_for(TriggerMode trigger_mode) const;

  BindingBuckets down_;
  BindingBuckets pressed_;
  size_t         size_ = 0;
};

[[nodiscard]] std::span<const InputActionSpec> ActionSpecs();
[[nodiscard]] const InputActionSpec* FindActionSpec(InputActionId id);
[[nodiscard]] const InputActionSpec* FindActionSpec(std::string_view canonical_key);
[[nodiscard]] std::optional<KeyCode> LookupKey(std::string_view key_name);
[[nodiscard]] bool IsModifierKey(KeyCode key);
[[nodiscard]] ParsedChord ParseChord(std::string_view chord_text);
[[nodiscard]] ParsedBinding ParseBinding(std::string_view binding_text);

} // namespace input_binding

constexpr input_binding::ModifierMask input_binding::ModifierMask::Logical(const ModifierGroup group)
{
  ModifierMask mask;
  mask.AddLogical(group);
  return mask;
}

constexpr input_binding::ModifierMask input_binding::ModifierMask::Physical(const PhysicalModifier modifier)
{
  ModifierMask mask;
  mask.AddPhysical(modifier);
  return mask;
}

constexpr input_binding::ModifierMask input_binding::ModifierMask::FromPressedKey(const KeyCode key)
{
  switch (key) {
    case KeyCode::LeftShift:
      return Physical(PhysicalModifier::LeftShift);
    case KeyCode::RightShift:
      return Physical(PhysicalModifier::RightShift);
    case KeyCode::LeftControl:
      return Physical(PhysicalModifier::LeftControl);
    case KeyCode::RightControl:
      return Physical(PhysicalModifier::RightControl);
    case KeyCode::LeftAlt:
      return Physical(PhysicalModifier::LeftAlt);
    case KeyCode::RightAlt:
      return Physical(PhysicalModifier::RightAlt);
    case KeyCode::LeftWindows:
      return Physical(PhysicalModifier::LeftWindows);
    case KeyCode::RightWindows:
      return Physical(PhysicalModifier::RightWindows);
    case KeyCode::LeftCommand:
      return Physical(PhysicalModifier::LeftCommand);
    case KeyCode::RightCommand:
      return Physical(PhysicalModifier::RightCommand);
    case KeyCode::AltGr:
      return Physical(PhysicalModifier::AltGr);
    default:
      return {};
  }
}

constexpr void input_binding::ModifierMask::AddLogical(const ModifierGroup group)
{ logical_bits_ |= (1u << static_cast<uint8_t>(group)); }

constexpr void input_binding::ModifierMask::AddPhysical(const PhysicalModifier modifier)
{ physical_bits_ |= (1u << static_cast<uint8_t>(modifier)); }

constexpr void input_binding::ModifierMask::Merge(const ModifierMask other)
{
  logical_bits_ |= other.logical_bits_;
  physical_bits_ |= other.physical_bits_;
}

constexpr bool input_binding::ModifierMask::empty() const
{ return logical_bits_ == 0 && physical_bits_ == 0; }

constexpr bool input_binding::ModifierMask::IsSatisfiedBy(const ModifierMask held) const
{
  return (logical_bits_ & held.effective_logical_bits()) == logical_bits_
         && (physical_bits_ & held.physical_bits_) == physical_bits_;
}

constexpr bool input_binding::ModifierMask::IsExactMatch(const ModifierMask held) const
{ return IsSatisfiedBy(held) && effective_logical_bits() == held.effective_logical_bits(); }

constexpr uint32_t input_binding::ModifierMask::logical_bits() const
{ return logical_bits_; }

constexpr uint32_t input_binding::ModifierMask::physical_bits() const
{ return physical_bits_; }

constexpr uint32_t input_binding::ModifierMask::effective_logical_bits() const
{
  auto bits = logical_bits_;
  if (physical_bits_ & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftShift))
                        | (1u << static_cast<uint8_t>(PhysicalModifier::RightShift)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Shift));
  }
  if (physical_bits_ & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftControl))
                        | (1u << static_cast<uint8_t>(PhysicalModifier::RightControl)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Ctrl));
  }
  if (physical_bits_ & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftAlt))
                        | (1u << static_cast<uint8_t>(PhysicalModifier::RightAlt)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Alt));
  }
  if (physical_bits_ & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftWindows))
                        | (1u << static_cast<uint8_t>(PhysicalModifier::RightWindows)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Win));
  }
  if (physical_bits_ & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftCommand))
                        | (1u << static_cast<uint8_t>(PhysicalModifier::RightCommand)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Command));
  }
  if (physical_bits_ & (1u << static_cast<uint8_t>(PhysicalModifier::AltGr))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::AltGr));
  }
  return bits;
}