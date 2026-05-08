#include "patches/input_binding/input_binding.h"

#include "str_utils_pure.h"

#include <algorithm>
#include <utility>

namespace input_binding
{
namespace
{
  constexpr std::array<InputActionSpec, 71> kActionSpecs{{
      {InputActionId::FleetPrimary, "fleet_primary", "SPACE|MOUSE1", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 100},
      {InputActionId::FleetSecondary, "fleet_secondary", "TAB|MOUSE4", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 90},
      {InputActionId::FleetService, "fleet_service", "R|MOUSE3", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 80},
      {InputActionId::FleetViewInfo, "fleet_view_info", "V|MOUSE2", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 70},
      {InputActionId::FleetQueueClear, "fleet_queue_clear", "CTRL-C", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 110},
      {InputActionId::FleetQueueToggle, "fleet_queue_toggle", "CTRL-Q", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 800},
      {InputActionId::SelectShip1, "select_ship1", "1", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 160},
      {InputActionId::SelectShip2, "select_ship2", "2", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 159},
      {InputActionId::SelectShip3, "select_ship3", "3", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 158},
      {InputActionId::SelectShip4, "select_ship4", "4", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 157},
      {InputActionId::SelectShip5, "select_ship5", "5", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 156},
      {InputActionId::SelectShip6, "select_ship6", "6", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 155},
      {InputActionId::SelectShip7, "select_ship7", "7", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 154},
      {InputActionId::SelectShip8, "select_ship8", "8", TriggerMode::Down, InputPhase::Frame, InputLayer::Fleet,
       ConflictGroup::FleetAction, 153},
      {InputActionId::SelectCurrent, "select_current", "CTRL-SPACE", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Fleet, ConflictGroup::FleetAction, 152},
      {InputActionId::ShowChatSide1, "show_chatside1", "ALT-C", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::ChatOpen, 790},
      {InputActionId::ShowChatSide2, "show_chatside2", "`", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::ChatOpen, 789},
      {InputActionId::ShowChat, "show_chat", "C", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::ChatOpen, 780},
      {InputActionId::SelectChatGlobal, "select_chatglobal", "CTRL-1", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::ChatChannel, 760},
      {InputActionId::SelectChatAlliance, "select_chatalliance", "CTRL-2", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::ChatChannel, 750},
      {InputActionId::SelectChatPrivate, "select_chatprivate", "CTRL-3", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::ChatChannel, 740},
      {InputActionId::MoveLeft, "move_left", "A", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::OfficerCanvas, 730},
      {InputActionId::MoveRight, "move_right", "D", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::OfficerCanvas, 720},
      {InputActionId::HotkeysDisable, "hotkeys_disable", "CTRL-ALT-MINUS", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::GlobalControl, 1000},
      {InputActionId::HotkeysEnable, "hotkeys_enable", "CTRL-ALT-=", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::GlobalControl, 1000},
      {InputActionId::Quit, "quit", "F10", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::GlobalControl, 900},
      {InputActionId::UiScaleUp, "ui_scale_up", "PGUP", TriggerMode::Pressed, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 340},
      {InputActionId::UiScaleDown, "ui_scale_down", "PGDOWN", TriggerMode::Pressed, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 330},
      {InputActionId::UiViewerScaleUp, "ui_viewer_scale_up", "SHIFT-PGUP", TriggerMode::Pressed, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 320},
      {InputActionId::UiViewerScaleDown, "ui_viewer_scale_down", "SHIFT-PGDOWN", TriggerMode::Pressed,
       InputPhase::Frame, InputLayer::Global, ConflictGroup::FrameDispatch, 310},
      {InputActionId::LogOff, "log_off", "CTRL-SHIFT-F12", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 230},
      {InputActionId::LogError, "log_error", "CTRL-SHIFT-F11", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 220},
      {InputActionId::LogWarn, "log_warn", "CTRL-SHIFT-F10", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 210},
      {InputActionId::LogInfo, "log_info", "CTRL-SHIFT-F8", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 200},
      {InputActionId::LogDebug, "log_debug", "CTRL-SHIFT-F9", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 190},
      {InputActionId::LogTrace, "log_trace", "CTRL-SHIFT-F7", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Diagnostics, ConflictGroup::FrameDispatch, 180},
      {InputActionId::ShowQTrials, "show_qtrials", "SHIFT-Q", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 590},
      {InputActionId::ShowBookmarks, "show_bookmarks", "B", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 580},
      {InputActionId::ShowLookup, "show_lookup", "L", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 570},
      {InputActionId::ShowRefinery, "show_refinery", "SHIFT-F", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 560},
      {InputActionId::ShowFactions, "show_factions", "F", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 550},
      {InputActionId::ShowStationExterior, "show_stationexterior", "SHIFT-G", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 540},
      {InputActionId::ShowGalaxy, "show_galaxy", "G", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 530},
      {InputActionId::ShowStationInterior, "show_stationinterior", "SHIFT-H", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 520},
      {InputActionId::ShowSystem, "show_system", "H", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 510},
      {InputActionId::ShowArtifacts, "show_artifacts", "SHIFT-I", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 500},
      {InputActionId::ShowInventory, "show_inventory", "I", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 490},
      {InputActionId::ShowMissions, "show_missions", "M", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 480},
      {InputActionId::ShowResearch, "show_research", "U", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 470},
      {InputActionId::ShowScrapYard, "show_scrapyard", "Y", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 460},
      {InputActionId::ShowOfficers, "show_officers", "SHIFT-O", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 450},
      {InputActionId::ShowCommander, "show_commander", "O", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 440},
      {InputActionId::ShowAwayTeam, "show_awayteam", "T", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 430},
      {InputActionId::ShowEvents, "show_events", "SHIFT-E", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 420},
      {InputActionId::ShowExoComp, "show_exocomp", "X", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 410},
      {InputActionId::ShowDaily, "show_daily", "Z", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 400},
      {InputActionId::ShowGifts, "show_gifts", "/", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 390},
      {InputActionId::ShowAlliance, "show_alliance", "ALT-'", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 380},
      {InputActionId::ShowAllianceHelp, "show_alliance_help", "SHIFT-'", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 370},
      {InputActionId::ShowAllianceArmada, "show_alliance_armada", "CTRL-'", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 360},
      {InputActionId::ShowSettings, "show_settings", "SHIFT-S", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 350},
      {InputActionId::TogglePreviewLocate, "toggle_preview_locate", "CTRL-R", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 300},
      {InputActionId::TogglePreviewRecall, "toggle_preview_recall", "CTRL-T", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 290},
      {InputActionId::ToggleCargoDefault, "toggle_cargo_default", "ALT-1", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 280},
      {InputActionId::ToggleCargoPlayer, "toggle_cargo_player", "ALT-2", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 270},
      {InputActionId::ToggleCargoStation, "toggle_cargo_station", "ALT-3", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 260},
      {InputActionId::ToggleCargoHostile, "toggle_cargo_hostile", "ALT-4", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 250},
      {InputActionId::ToggleCargoArmada, "toggle_cargo_armada", "ALT-5", TriggerMode::Down, InputPhase::Frame,
       InputLayer::Global, ConflictGroup::FrameDispatch, 240},
      {InputActionId::ShowShips, "show_ships", "N", TriggerMode::Down, InputPhase::Frame, InputLayer::Global,
       ConflictGroup::FrameDispatch, 170},
      {InputActionId::ZoomIn, "zoom_in", "Q", TriggerMode::Pressed, InputPhase::NavigationZoomUpdate, InputLayer::Zoom,
       ConflictGroup::Zoom, 100},
      {InputActionId::ZoomOut, "zoom_out", "E", TriggerMode::Pressed, InputPhase::NavigationZoomUpdate,
       InputLayer::Zoom, ConflictGroup::Zoom, 100},
  }};

  struct KeyName {
    std::string_view name;
    KeyCode          key;
  };

  constexpr auto kKeyNames = std::to_array<KeyName>({
      {"LALT", KeyCode::LeftAlt},
      {"LAPPLE", KeyCode::LeftApple},
      {"LCOM", KeyCode::LeftCommand},
      {"LCMD", KeyCode::LeftCommand},
      {"LCTRL", KeyCode::LeftControl},
      {"ALTGR", KeyCode::AltGr},
      {"END", KeyCode::End},
      {"HOME", KeyCode::Home},
      {"PGDOWN", KeyCode::PageDown},
      {"PGUP", KeyCode::PageUp},
      {"DOWN", KeyCode::DownArrow},
      {"LEFT", KeyCode::LeftArrow},
      {"RIGHT", KeyCode::RightArrow},
      {"UP", KeyCode::UpArrow},
      {"BACKSPACE", KeyCode::Backspace},
      {"BREAK", KeyCode::Break},
      {"CAPS", KeyCode::CapsLock},
      {"CLEAR", KeyCode::Clear},
      {"DELETE", KeyCode::Delete},
      {"ESCAPE", KeyCode::Escape},
      {"HELP", KeyCode::Help},
      {"INSERT", KeyCode::Insert},
      {"LSHIFT", KeyCode::LeftShift},
      {"LWIN", KeyCode::LeftWindows},
      {"MENU", KeyCode::Menu},
      {"PAUSE", KeyCode::Pause},
      {"PRINT", KeyCode::Print},
      {"RALT", KeyCode::RightAlt},
      {"RAPPLE", KeyCode::RightApple},
      {"RCOM", KeyCode::RightCommand},
      {"RCMD", KeyCode::RightCommand},
      {"RCTRL", KeyCode::RightControl},
      {"RETURN", KeyCode::Return},
      {"RSHIFT", KeyCode::RightShift},
      {"RWIN", KeyCode::RightWindows},
      {"SCROLL", KeyCode::ScrollLock},
      {"SYSREQ", KeyCode::SysReq},
      {"TAB", KeyCode::Tab},
      {"MOUSE0", KeyCode::Mouse0},
      {"MOUSE1", KeyCode::Mouse1},
      {"MOUSE2", KeyCode::Mouse2},
      {"MOUSE3", KeyCode::Mouse3},
      {"MOUSE4", KeyCode::Mouse4},
      {"MOUSE5", KeyCode::Mouse5},
      {"MOUSE6", KeyCode::Mouse6},
      {"SPACE", KeyCode::Space},
      {"MINUS", KeyCode::Minus},
      {"-", KeyCode::Minus},
      {"_", KeyCode::Underscore},
      {",", KeyCode::Comma},
      {";", KeyCode::Semicolon},
      {":", KeyCode::Colon},
      {"!", KeyCode::Exclaim},
      {"?", KeyCode::Question},
      {".", KeyCode::Period},
      {"'", KeyCode::Quote},
      {"[", KeyCode::LeftBracket},
      {"]", KeyCode::RightBracket},
      {"/", KeyCode::Slash},
      {"\\", KeyCode::Backslash},
      {"`", KeyCode::BackQuote},
      {"+", KeyCode::Plus},
      {"PLUS", KeyCode::Plus},
      {"=", KeyCode::Equals},
      {"0", KeyCode::Alpha0},
      {"1", KeyCode::Alpha1},
      {"2", KeyCode::Alpha2},
      {"3", KeyCode::Alpha3},
      {"4", KeyCode::Alpha4},
      {"5", KeyCode::Alpha5},
      {"6", KeyCode::Alpha6},
      {"7", KeyCode::Alpha7},
      {"8", KeyCode::Alpha8},
      {"9", KeyCode::Alpha9},
      {"A", KeyCode::A},
      {"B", KeyCode::B},
      {"C", KeyCode::C},
      {"D", KeyCode::D},
      {"E", KeyCode::E},
      {"F", KeyCode::F},
      {"G", KeyCode::G},
      {"H", KeyCode::H},
      {"I", KeyCode::I},
      {"J", KeyCode::J},
      {"K", KeyCode::K},
      {"L", KeyCode::L},
      {"M", KeyCode::M},
      {"N", KeyCode::N},
      {"O", KeyCode::O},
      {"P", KeyCode::P},
      {"Q", KeyCode::Q},
      {"R", KeyCode::R},
      {"S", KeyCode::S},
      {"T", KeyCode::T},
      {"U", KeyCode::U},
      {"V", KeyCode::V},
      {"W", KeyCode::W},
      {"X", KeyCode::X},
      {"Y", KeyCode::Y},
      {"Z", KeyCode::Z},
  });

  std::optional<ModifierMask> lookup_modifier(std::string_view token)
  {
    if (token == "SHIFT") {
      return ModifierMask::Logical(ModifierGroup::Shift);
    }
    if (token == "CTRL") {
      return ModifierMask::Logical(ModifierGroup::Ctrl);
    }
    if (token == "ALT") {
      return ModifierMask::Logical(ModifierGroup::Alt);
    }
    if (token == "WIN") {
      return ModifierMask::Logical(ModifierGroup::Win);
    }
    if (token == "CMD") {
      return ModifierMask::Logical(ModifierGroup::Command);
    }
    if (token == "APPLE") {
      return ModifierMask::Logical(ModifierGroup::Command);
    }
    if (auto key = LookupKey(token); key && IsModifierKey(*key)) {
      return ModifierMask::FromPressedKey(*key);
    }
    return std::nullopt;
  }

  bool modifiers_can_match_same_input(const ParsedChord& lhs, const ParsedChord& rhs)
  { return lhs.modifiers.effective_logical_bits() == rhs.modifiers.effective_logical_bits(); }

  bool conflicts_with(const ConflictGroup lhs, const ConflictGroup rhs)
  { return lhs != ConflictGroup::None && lhs == rhs; }

  std::optional<std::string_view> find_override(const std::span<const BindingOverride> overrides,
                                                const InputActionId                    action)
  {
    const auto found = std::ranges::find_if(overrides, [action](const auto& entry) { return entry.action == action; });
    if (found == overrides.end()) {
      return std::nullopt;
    }
    return found->binding;
  }
} // namespace

bool ParsedChord::Matches(const KeyCode pressed_key, const ModifierMask held_modifiers,
                          const bool allow_extra_modifiers) const
{
  if (!valid || key != pressed_key || !modifiers.IsSatisfiedBy(held_modifiers)) {
    return false;
  }
  return allow_extra_modifiers || modifiers.IsExactMatch(held_modifiers);
}

bool ParsedBinding::has_valid_chord() const
{
  return std::ranges::any_of(chords, [](const auto& chord) { return chord.valid; });
}

bool ParsedBinding::has_warnings() const
{
  return std::ranges::any_of(diagnostics,
                             [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Warning; });
}

bool ParsedBinding::has_errors() const
{
  return std::ranges::any_of(diagnostics,
                             [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
}

std::string ParsedBinding::DisplayString() const
{
  if (unbound) {
    return "NONE";
  }

  std::string display;
  for (const auto& chord : chords) {
    if (!chord.valid) {
      continue;
    }
    if (!display.empty()) {
      display.append(" | ");
    }
    display.append(chord.display);
  }
  return display;
}

bool CompileResult::has_warnings() const
{
  return std::ranges::any_of(diagnostics,
                             [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Warning; });
}

bool CompileResult::has_errors() const
{
  return std::ranges::any_of(diagnostics,
                             [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
}

bool CompileResult::has_conflicts() const
{ return !conflicts.empty(); }

void BindingIndex::Register(const InputActionId action, ParsedChord chord, const TriggerMode trigger_mode,
                            const uint16_t priority)
{
  if (!chord.valid || chord.key == KeyCode::None) {
    return;
  }
  auto& bucket = trigger_mode == TriggerMode::Pressed ? pressed_ : down_;
  bucket[static_cast<size_t>(chord.key)].push_back({action, std::move(chord), trigger_mode, priority});
  std::ranges::sort(bucket[static_cast<size_t>(chord.key)],
                    [](const auto& lhs, const auto& rhs) { return lhs.priority > rhs.priority; });
  ++size_;
}

std::vector<InputActionId> BindingIndex::Match(const TriggerMode trigger_mode, const KeyCode key,
                                               const ModifierMask held_modifiers,
                                               const bool         allow_extra_modifiers) const
{
  std::vector<InputActionId> matches;
  const auto&                bucket = buckets_for(trigger_mode)[static_cast<size_t>(key)];
  for (const auto& binding : bucket) {
    if (binding.chord.Matches(key, held_modifiers, allow_extra_modifiers)) {
      matches.push_back(binding.action);
    }
  }
  return matches;
}

size_t BindingIndex::size() const
{ return size_; }

const BindingIndex::BindingBuckets& BindingIndex::buckets_for(const TriggerMode trigger_mode) const
{ return trigger_mode == TriggerMode::Pressed ? pressed_ : down_; }

std::span<const InputActionSpec> ActionSpecs()
{ return kActionSpecs; }

const InputActionSpec* FindActionSpec(const InputActionId id)
{
  const auto specs = ActionSpecs();
  const auto found = std::ranges::find_if(specs, [id](const auto& spec) { return spec.id == id; });
  return found == specs.end() ? nullptr : &*found;
}

const InputActionSpec* FindActionSpec(const std::string_view canonical_key)
{
  const auto specs = ActionSpecs();
  const auto found =
      std::ranges::find_if(specs, [canonical_key](const auto& spec) { return spec.canonical_key == canonical_key; });
  return found == specs.end() ? nullptr : &*found;
}

std::optional<KeyCode> LookupKey(const std::string_view key_name)
{
  const auto normalized = AsciiStrToUpper(StripAsciiWhitespace(key_name));
  const auto found =
      std::ranges::find_if(kKeyNames, [&normalized](const auto& item) { return item.name == normalized; });
  if (found != kKeyNames.end()) {
    return found->key;
  }

  if (normalized.size() >= 2 && normalized[0] == 'F') {
    const auto number = normalized.substr(1);
    if (number == "1") {
      return KeyCode::F1;
    }
    if (number == "2") {
      return KeyCode::F2;
    }
    if (number == "3") {
      return KeyCode::F3;
    }
    if (number == "4") {
      return KeyCode::F4;
    }
    if (number == "5") {
      return KeyCode::F5;
    }
    if (number == "6") {
      return KeyCode::F6;
    }
    if (number == "7") {
      return KeyCode::F7;
    }
    if (number == "8") {
      return KeyCode::F8;
    }
    if (number == "9") {
      return KeyCode::F9;
    }
    if (number == "10") {
      return KeyCode::F10;
    }
    if (number == "11") {
      return KeyCode::F11;
    }
    if (number == "12") {
      return KeyCode::F12;
    }
  }

  return std::nullopt;
}

bool IsModifierKey(const KeyCode key)
{
  switch (key) {
    case KeyCode::LeftShift:
    case KeyCode::RightShift:
    case KeyCode::LeftControl:
    case KeyCode::RightControl:
    case KeyCode::LeftAlt:
    case KeyCode::RightAlt:
    case KeyCode::LeftWindows:
    case KeyCode::RightWindows:
    case KeyCode::LeftCommand:
    case KeyCode::RightCommand:
    case KeyCode::AltGr:
      return true;
    default:
      return false;
  }
}

ParsedChord ParseChord(const std::string_view chord_text)
{
  ParsedChord chord;
  const auto  normalized = AsciiStrToUpper(StripAsciiWhitespace(chord_text));
  chord.display          = normalized;

  if (normalized.empty()) {
    return chord;
  }

  if (auto key = LookupKey(normalized); key && !IsModifierKey(*key)) {
    chord.key   = *key;
    chord.valid = true;
    return chord;
  }

  const auto tokens = StrSplit(normalized, '-');
  for (size_t index = 0; index < tokens.size(); ++index) {
    const auto& token = tokens[index];
    if (auto modifier = lookup_modifier(token)) {
      chord.modifiers.Merge(*modifier);
      continue;
    }

    if (auto key = LookupKey(token); key && !IsModifierKey(*key)) {
      chord.key = *key;
      continue;
    }

    chord.diagnostics.push_back({DiagnosticSeverity::Warning, "Unknown chord token", index});
  }

  chord.valid = chord.key != KeyCode::None;
  return chord;
}

ParsedBinding ParseBinding(const std::string_view binding_text)
{
  ParsedBinding binding;
  const auto    normalized = AsciiStrToUpper(StripAsciiWhitespace(binding_text));
  if (normalized == "NONE") {
    binding.unbound = true;
    return binding;
  }

  const auto tokens = StrSplit(normalized, '|');
  for (size_t index = 0; index < tokens.size(); ++index) {
    auto chord = ParseChord(tokens[index]);
    for (auto diagnostic : chord.diagnostics) {
      diagnostic.token_index = index;
      binding.diagnostics.push_back(std::move(diagnostic));
    }
    if (!chord.valid && chord.diagnostics.empty()) {
      binding.diagnostics.push_back({DiagnosticSeverity::Warning, "Invalid chord token", index});
    }
    binding.chords.push_back(std::move(chord));
  }

  if (!binding.has_valid_chord()) {
    binding.diagnostics.push_back({DiagnosticSeverity::Error, "Binding has no valid chord", 0});
  }

  return binding;
}

CompileResult CompileBindingSet(const std::span<const BindingOverride> overrides)
{
  struct RegisteredChord {
    InputActionId action = InputActionId::Max;
    ParsedChord   chord;
    TriggerMode   trigger_mode   = TriggerMode::Down;
    ConflictGroup conflict_group = ConflictGroup::None;
  };

  CompileResult                result;
  std::vector<RegisteredChord> registered_chords;
  result.bindings.reserve(ActionSpecs().size());

  for (const auto& spec : ActionSpecs()) {
    const auto binding_text = find_override(overrides, spec.id).value_or(spec.default_bind);
    auto       parsed       = ParseBinding(binding_text);

    for (auto diagnostic : parsed.diagnostics) {
      diagnostic.action = spec.id;
      result.diagnostics.push_back(std::move(diagnostic));
    }

    if (parsed.unbound) {
      continue;
    }

    for (auto& chord : parsed.chords) {
      if (!chord.valid) {
        continue;
      }

      for (const auto& registered : registered_chords) {
        if (registered.trigger_mode != spec.trigger_mode || registered.chord.key != chord.key
            || !modifiers_can_match_same_input(registered.chord, chord)
            || !conflicts_with(registered.conflict_group, spec.conflict_group)) {
          continue;
        }

        result.conflicts.push_back({registered.action, spec.id, chord, spec.trigger_mode, spec.conflict_group});
        result.diagnostics.push_back(
            {DiagnosticSeverity::Error, "Binding conflict", result.bound_chord_count, spec.id});
      }

      result.bindings.push_back({spec.id, chord, spec.trigger_mode, spec.priority});
      result.index.Register(spec.id, chord, spec.trigger_mode, spec.priority);
      registered_chords.push_back({spec.id, std::move(chord), spec.trigger_mode, spec.conflict_group});
      ++result.bound_chord_count;
    }
  }

  return result;
}
} // namespace input_binding