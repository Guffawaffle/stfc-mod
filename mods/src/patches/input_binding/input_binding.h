#pragma once

#include "patches/gamefunctions.h"

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
  FleetQueueAdd,
  FleetRecallCancel,
  FleetRecall,
  FleetRepair,
  FleetViewInfo,
  FleetQueueClear,
  FleetQueueToggle,
  ManualNavigationRefresh,
  SelectShip1,
  SelectShip2,
  SelectShip3,
  SelectShip4,
  SelectShip5,
  SelectShip6,
  SelectShip7,
  SelectShip8,
  SelectCurrent,
  ShowChat,
  ShowChatSide1,
  ShowChatSide2,
  SelectChatGlobal,
  SelectChatAlliance,
  SelectChatPrivate,
  MoveLeft,
  MoveRight,
  HotkeysDisable,
  HotkeysEnable,
  LogDebug,
  ZoomIn,
  ZoomOut,
  ZoomPreset1,
  ZoomPreset2,
  ZoomPreset3,
  ZoomPreset4,
  ZoomPreset5,
  ZoomMin,
  ZoomMax,
  ZoomReset,
  SetZoomPreset1,
  SetZoomPreset2,
  SetZoomPreset3,
  SetZoomPreset4,
  SetZoomPreset5,
  SetZoomDefault,
  Quit,
  UiScaleUp,
  UiScaleDown,
  UiViewerScaleUp,
  UiViewerScaleDown,
  LogOff,
  LogError,
  LogWarn,
  LogInfo,
  LogTrace,
  ShowQTrials,
  ShowBookmarks,
  ShowLookup,
  ShowRefinery,
  ShowFactions,
  ShowStationExterior,
  ShowGalaxy,
  ShowStationInterior,
  ShowSystem,
  ShowArtifacts,
  ShowInventory,
  ShowMissions,
  ShowResearch,
  ShowScrapYard,
  ShowOfficers,
  ShowCommander,
  ShowAwayTeam,
  ShowEvents,
  ShowExoComp,
  ShowDaily,
  ShowGifts,
  ShowAlliance,
  ShowAllianceHelp,
  ShowAllianceArmada,
  ShowSettings,
  TogglePreviewLocate,
  TogglePreviewRecall,
  ToggleCargoDefault,
  ToggleCargoPlayer,
  ToggleCargoStation,
  ToggleCargoHostile,
  ToggleCargoArmada,
  ShowShips,
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
  FrameDispatch,
  ChatOpen,
  ChatChannel,
  OfficerCanvas,
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

enum class ActionCategory : uint8_t {
  Fleet = 0,
  Ships,
  Chat,
  Navigation,
  Panels,
  Interface,
  Diagnostics,
  System,
  Camera,
};

enum class ActionExecutor : uint8_t {
  None = 0,
  FleetSpace,
  FleetSimple,
  FleetQueue,
  ShipSelection,
  SelectCurrent,
  ChatOpen,
  ChatChannel,
  OfficerCanvas,
  GlobalControl,
  TableDispatch,
  Zoom,
};

enum class NativeConsumePolicy : uint8_t {
  Never = 0,
  WhenHandled,
  WhenChordMatches,
};

enum class ActionOutcome : uint8_t {
  Ignored = 0,
  Handled,
};

enum class OriginalInputOverride : uint8_t {
  None = 0,
  AllowOriginal,
  SuppressOriginal,
};

struct ActionHandlerResult {
  ActionOutcome         outcome        = ActionOutcome::Ignored;
  OriginalInputOverride original_input = OriginalInputOverride::None;
};

using ActionHandler = ActionHandlerResult (*)();

struct ActionSpec : InputActionSpec {
  ActionCategory      category       = ActionCategory::System;
  ActionExecutor      executor       = ActionExecutor::None;
  GameFunction        game_function  = GameFunction::Max;
  NativeConsumePolicy native_consume = NativeConsumePolicy::WhenHandled;
  ActionHandler       handler        = nullptr;
  bool                rebindable     = true;
  bool                visible_in_ui  = true;
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

  [[nodiscard]] constexpr bool     empty() const;
  [[nodiscard]] constexpr bool     IsSatisfiedBy(ModifierMask held) const;
  [[nodiscard]] constexpr bool     IsExactMatch(ModifierMask held) const;
  [[nodiscard]] constexpr uint32_t logical_bits() const;
  [[nodiscard]] constexpr uint32_t physical_bits() const;
  [[nodiscard]] constexpr uint32_t effective_logical_bits() const;

private:
  uint32_t logical_bits_  = 0;
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
  InputActionId      action      = InputActionId::Max;
};

struct ParsedChord {
  KeyCode                        key = KeyCode::None;
  ModifierMask                   modifiers;
  std::string                    display;
  std::vector<BindingDiagnostic> diagnostics;
  bool                           valid = false;

  [[nodiscard]] bool Matches(KeyCode pressed_key, ModifierMask held_modifiers,
                             bool allow_extra_modifiers = false) const;
};

struct ParsedBinding {
  std::vector<ParsedChord>       chords;
  std::vector<BindingDiagnostic> diagnostics;
  bool                           unbound = false;

  [[nodiscard]] bool        has_valid_chord() const;
  [[nodiscard]] bool        has_warnings() const;
  [[nodiscard]] bool        has_errors() const;
  [[nodiscard]] std::string DisplayString() const;
};

struct CompiledBinding {
  InputActionId action = InputActionId::Max;
  ParsedChord   chord;
  TriggerMode   trigger_mode = TriggerMode::Down;
  uint16_t      priority     = 0;
};

class BindingIndex
{
public:
  void Register(InputActionId action, ParsedChord chord, TriggerMode trigger_mode, uint16_t priority = 0);

  [[nodiscard]] std::vector<InputActionId> Match(TriggerMode trigger_mode, KeyCode key, ModifierMask held_modifiers,
                                                 bool allow_extra_modifiers = false) const;
  [[nodiscard]] size_t                     size() const;

private:
  static constexpr size_t kKeyCodeCount = static_cast<size_t>(KeyCode::Max) + 1;
  using BindingBuckets                  = std::array<std::vector<CompiledBinding>, kKeyCodeCount>;

  [[nodiscard]] const BindingBuckets& buckets_for(TriggerMode trigger_mode) const;

  BindingBuckets down_;
  BindingBuckets pressed_;
  size_t         size_ = 0;
};

struct BindingOverride {
  InputActionId action = InputActionId::Max;
  std::string   binding;
};

struct BindingConflict {
  InputActionId action_a = InputActionId::Max;
  InputActionId action_b = InputActionId::Max;
  ParsedChord   chord;
  TriggerMode   trigger_mode   = TriggerMode::Down;
  ConflictGroup conflict_group = ConflictGroup::None;
};

struct CompileResult {
  BindingIndex                   index;
  std::vector<CompiledBinding>   bindings;
  std::vector<BindingDiagnostic> diagnostics;
  std::vector<BindingConflict>   conflicts;
  size_t                         bound_chord_count = 0;

  [[nodiscard]] bool has_warnings() const;
  [[nodiscard]] bool has_errors() const;
  [[nodiscard]] bool has_conflicts() const;
};

[[nodiscard]] std::span<const ActionSpec> ActionSpecs();
[[nodiscard]] const ActionSpec*           FindActionSpec(InputActionId id);
[[nodiscard]] const ActionSpec*           FindActionSpec(std::string_view canonical_key);
[[nodiscard]] std::optional<KeyCode>      LookupKey(std::string_view key_name);
[[nodiscard]] bool                        IsModifierKey(KeyCode key);
[[nodiscard]] ParsedChord                 ParseChord(std::string_view chord_text);
[[nodiscard]] ParsedBinding               ParseBinding(std::string_view binding_text);
[[nodiscard]] CompileResult               CompileBindingSet(std::span<const BindingOverride> overrides = {});

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
  if (physical_bits_
      & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftShift))
         | (1u << static_cast<uint8_t>(PhysicalModifier::RightShift)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Shift));
  }
  if (physical_bits_
      & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftControl))
         | (1u << static_cast<uint8_t>(PhysicalModifier::RightControl)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Ctrl));
  }
  if (physical_bits_
      & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftAlt))
         | (1u << static_cast<uint8_t>(PhysicalModifier::RightAlt)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Alt));
  }
  if (physical_bits_
      & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftWindows))
         | (1u << static_cast<uint8_t>(PhysicalModifier::RightWindows)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Win));
  }
  if (physical_bits_
      & ((1u << static_cast<uint8_t>(PhysicalModifier::LeftCommand))
         | (1u << static_cast<uint8_t>(PhysicalModifier::RightCommand)))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::Command));
  }
  if (physical_bits_ & (1u << static_cast<uint8_t>(PhysicalModifier::AltGr))) {
    bits |= (1u << static_cast<uint8_t>(ModifierGroup::AltGr));
  }
  return bits;
}
