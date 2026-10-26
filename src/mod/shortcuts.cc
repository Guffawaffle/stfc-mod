// Owns the shortcut catalog from shortcuts.def: parsing [shortcuts] into key chords, the live bindings, and
// matching them against this frame's input.

#include "shortcuts.h"
#include "keyboard_layout.h"
#include "settings/shortcut_capture_guard.h"

#include "text.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.h>
#include <unity/input.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
// --- Key names ---

// A key name from the settings file, and its keybind badge text when that differs from the name. The badge font
// has few glyphs, so long names are shortened and the symbols the modifier notation uses are spelled out.
struct KeyName {
  std::string_view name;
  unity::KeyCode   key;
  std::string_view hint = {};
};

constexpr KeyName kKeys[] = {
    {"END", unity::KeyCode::End},
    {"HOME", unity::KeyCode::Home, "HM"},
    {"PGDOWN", unity::KeyCode::PageDown, "PD"},
    {"PGUP", unity::KeyCode::PageUp, "PU"},
    {"DOWN", unity::KeyCode::DownArrow, "DN"},
    {"LEFT", unity::KeyCode::LeftArrow, "LT"},
    {"RIGHT", unity::KeyCode::RightArrow, "RT"},
    {"UP", unity::KeyCode::UpArrow},
    {"BACKSPACE", unity::KeyCode::Backspace, "BS"},
    {"BREAK", unity::KeyCode::Break},
    {"CAPS", unity::KeyCode::CapsLock},
    {"CLEAR", unity::KeyCode::Clear},
    {"DELETE", unity::KeyCode::Delete, "DEL"},
    {"ESCAPE", unity::KeyCode::Escape, "ESC"},
    {"HELP", unity::KeyCode::Help},
    {"INSERT", unity::KeyCode::Insert},
    {"MENU", unity::KeyCode::Menu},
    {"PAUSE", unity::KeyCode::Pause},
    {"PRINT", unity::KeyCode::Print},
    {"RETURN", unity::KeyCode::Return, "ENT"},
    {"SCROLL", unity::KeyCode::ScrollLock},
    {"SYSREQ", unity::KeyCode::SysReq},
    {"TAB", unity::KeyCode::Tab},
    {"MOUSE0", unity::KeyCode::Mouse0, "M0"},
    {"MOUSE1", unity::KeyCode::Mouse1, "M1"},
    {"MOUSE2", unity::KeyCode::Mouse2, "M2"},
    {"MOUSE3", unity::KeyCode::Mouse3, "M3"},
    {"MOUSE4", unity::KeyCode::Mouse4, "M4"},
    {"MOUSE5", unity::KeyCode::Mouse5, "M5"},
    {"MOUSE6", unity::KeyCode::Mouse6, "M6"},
    {"SPACE", unity::KeyCode::Space, "SPC"},
    // Chords are split on '-', so the minus key needs a name.
    {"MINUS", unity::KeyCode::Minus, "-"},
    {"_", unity::KeyCode::Underscore},
    {",", unity::KeyCode::Comma},
    {";", unity::KeyCode::Semicolon},
    {":", unity::KeyCode::Colon},
    {"!", unity::KeyCode::Exclaim, "EXC"},
    {"?", unity::KeyCode::Question},
    {".", unity::KeyCode::Period},
    {"'", unity::KeyCode::Quote},
    {"(", unity::KeyCode::LeftParen},
    {")", unity::KeyCode::RightParen},
    {"[", unity::KeyCode::LeftBracket},
    {"]", unity::KeyCode::RightBracket},
    {"{", unity::KeyCode::LeftCurlyBracket},
    {"}", unity::KeyCode::RightCurlyBracket},
    {"@", unity::KeyCode::At},
    {"*", unity::KeyCode::Asterisk},
    {"/", unity::KeyCode::Slash},
    {"\"", unity::KeyCode::DoubleQuote},
    {"\\", unity::KeyCode::Backslash},
    {"&", unity::KeyCode::Ampersand},
    {"#", unity::KeyCode::Hash, "HSH"},
    {"%", unity::KeyCode::Percent},
    {"`", unity::KeyCode::BackQuote},
    {"^", unity::KeyCode::Caret, "CAR"},
    {"+", unity::KeyCode::Plus, "PLS"},
    {"<", unity::KeyCode::Less},
    {"=", unity::KeyCode::Equals},
    {">", unity::KeyCode::Greater},
    {"~", unity::KeyCode::Tilde},
    {"$", unity::KeyCode::Dollar},
    {"NUMLOCK", unity::KeyCode::Numlock},
    {"KEYDIVIDE", unity::KeyCode::KeypadDivide},
    {"KEYENTER", unity::KeyCode::KeypadEnter},
    {"KEYEQUAL", unity::KeyCode::KeypadEquals},
    {"KEYMINUS", unity::KeyCode::KeypadMinus},
    {"KEYMULTI", unity::KeyCode::KeypadMultiply},
    {"KEYPERIOD", unity::KeyCode::KeypadPeriod},
    {"KEYPLUS", unity::KeyCode::KeypadPlus},
};

// Modifier names. The unsided ones match either side of the keyboard. Badges use AutoHotkey's ASCII notation.
constexpr KeyChord::Modifier kModifiers[] = {
    {"SHIFT", {unity::KeyCode::LeftShift, unity::KeyCode::RightShift}, "+"},
    {"CTRL", {unity::KeyCode::LeftControl, unity::KeyCode::RightControl}, "^"},
    {"ALT", {unity::KeyCode::LeftAlt, unity::KeyCode::RightAlt}, "!"},
    {"APPLE", {unity::KeyCode::LeftApple, unity::KeyCode::RightApple}, "#"},
    {"CMD", {unity::KeyCode::LeftCommand, unity::KeyCode::RightCommand}, "#"},
    {"WIN", {unity::KeyCode::LeftWindows, unity::KeyCode::RightWindows}, "#"},
    {"LSHIFT", {unity::KeyCode::LeftShift, unity::KeyCode::LeftShift}, "<+"},
    {"RSHIFT", {unity::KeyCode::RightShift, unity::KeyCode::RightShift}, ">+"},
    {"LCTRL", {unity::KeyCode::LeftControl, unity::KeyCode::LeftControl}, "<^"},
    {"RCTRL", {unity::KeyCode::RightControl, unity::KeyCode::RightControl}, ">^"},
    {"LALT", {unity::KeyCode::LeftAlt, unity::KeyCode::LeftAlt}, "<!"},
    {"RALT", {unity::KeyCode::RightAlt, unity::KeyCode::RightAlt}, ">!"},
    {"ALTGR", {unity::KeyCode::AltGr, unity::KeyCode::AltGr}, "!"},
    {"LAPPLE", {unity::KeyCode::LeftApple, unity::KeyCode::LeftApple}, "<#"},
    {"LCOM", {unity::KeyCode::LeftCommand, unity::KeyCode::LeftCommand}, "<#"},
    {"LWIN", {unity::KeyCode::LeftWindows, unity::KeyCode::LeftWindows}, "<#"},
    {"RAPPLE", {unity::KeyCode::RightApple, unity::KeyCode::RightApple}, ">#"},
    {"RCOM", {unity::KeyCode::RightCommand, unity::KeyCode::RightCommand}, ">#"},
    {"RWIN", {unity::KeyCode::RightWindows, unity::KeyCode::RightWindows}, ">#"},
};

// Unity assigns contiguous codes to letters, digits, function keys and keypad digits.
std::optional<KeyName> FindKey(std::string_view name)
{
  if (name.size() == 1) {
    const char c = name[0];
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      return KeyName{name, static_cast<unity::KeyCode>(c >= 'A' ? c + ('a' - 'A') : c)};
  }
  struct Range {
    std::string_view prefix;
    int32_t          first, last;
    unity::KeyCode   key;
  };
  constexpr Range ranges[] = {{"F", 1, 15, unity::KeyCode::F1}, {"KEY", 0, 9, unity::KeyCode::Keypad0}};
  for (const auto& range : ranges) {
    if (!name.starts_with(range.prefix))
      continue;
    const auto digits       = name.substr(range.prefix.size());
    int32_t    number       = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
    if (error == std::errc{} && end == digits.data() + digits.size() && number >= range.first && number <= range.last
        && digits == std::to_string(number))
      return KeyName{name, static_cast<unity::KeyCode>(std::to_underlying(range.key) + number - range.first)};
  }
  const auto it = std::ranges::find(kKeys, name, &KeyName::name);
  return it == std::end(kKeys) ? std::nullopt : std::optional(*it);
}

std::string DescribeChords(const std::vector<KeyChord>& chords)
{
  std::string text;
  for (const auto& chord : chords)
    text += (text.empty() ? "" : " | ") + chord.Text();
  return text;
}

// --- Matching ---

// The live bindings, replaced whole by Set. Game thread only, like the input they are matched against.
Shortcut::Bindings                 bindings;
mod_settings::ShortcutCaptureGuard capture_guard;
uint64_t                           binding_generation = 0;

const std::vector<KeyChord>& ChordsOf(Action action)
{ return bindings[std::to_underlying(action)]; }

// Keys that completed a binding with modifiers, until released. A plain binding on the same key waits for the
// release, so letting go of CTRL before PGUP does not turn a held CTRL-PGUP into PGUP. At most one entry per key.
std::vector<unity::KeyCode> used_keys;

bool IsUsed(unity::KeyCode key)
{
  std::erase_if(used_keys, [](unity::KeyCode used) { return !unity::Key::Pressed(used); });
  return std::ranges::contains(used_keys, key);
}

bool ModifiersHeld(const KeyChord& chord)
{
  return std::ranges::all_of(
      chord.modifiers, [](const auto* modifier) { return std::ranges::any_of(modifier->keys, unity::Key::Pressed); });
}

template <bool (*KeyState)(unity::KeyCode), bool ExtraShift = false> bool Matches(Action action)
{
  for (const auto& chord : ChordsOf(action)) {
    const auto resolved = keyboard_layout::ResolveChord(chord.key);
    if (resolved.key == unity::KeyCode::None || !KeyState(resolved.key))
      continue;
    if (resolved.shift && !unity::Key::Pressed(unity::KeyCode::LeftShift)
        && !unity::Key::Pressed(unity::KeyCode::RightShift))
      continue;
    if (chord.modifiers.empty()) {
      bool modified = unity::Key::IsModified();
      if (resolved.shift || ExtraShift) {
        // Inferred Shift types the configured character; other modifiers still
        // prevent a bare action from stealing a Ctrl/Alt/Command shortcut.
        constexpr auto other_modifiers =
            std::array{unity::KeyCode::LeftControl,  unity::KeyCode::RightControl, unity::KeyCode::LeftAlt,
                       unity::KeyCode::RightAlt,     unity::KeyCode::AltGr,        unity::KeyCode::LeftCommand,
                       unity::KeyCode::RightCommand, unity::KeyCode::LeftWindows,  unity::KeyCode::RightWindows};
        modified = std::ranges::any_of(other_modifiers, unity::Key::Pressed);
      }
      if (modified || (!resolved.shift && IsUsed(resolved.key)))
        continue;
    } else {
      if (!ModifiersHeld(chord))
        continue;
    }
    const bool movement = action == Action::kMoveLeft || action == Action::kMoveRight || action == Action::kMoveUp
                          || action == Action::kMoveDown;
    if ((!chord.modifiers.empty() || resolved.shift || ExtraShift) && !movement
        && !std::ranges::contains(used_keys, resolved.key))
      used_keys.push_back(resolved.key);
    return true;
  }
  return false;
}

// --- Settings ---

// Which [control] switches a shortcut needs: hotkeys_extended, or that and enable_experimental.
enum class Tier { kAlways, kExtended, kExperimental };

struct Setting {
  Action                          action;
  std::string_view                key;
  std::string_view                fallback;
  Tier                            tier    = Tier::kAlways;
  std::array<std::string_view, 2> aliases = {};
};

// Order is the order of the debug log; the TOML output is sorted by key anyway.
constexpr Setting kSettings[] = {
#define SHORTCUT(action, ...) {Action::action, __VA_ARGS__},
#include "shortcuts.def"
#undef SHORTCUT
};
static_assert(std::size(kSettings) == std::to_underlying(Action::kCount));

// Binds every chord in `value` ("CTRL-G|F8"). Reports whether any was valid.
bool BindChords(std::vector<KeyChord>& chords, std::string_view key, std::string_view value)
{
  bool bound = false;
  for (const auto& text : StrSplit(value, '|')) {
    if (auto chord = KeyChord::Parse(text); chord.key != unity::KeyCode::None) {
      chords.push_back(std::move(chord));
      bound = true;
    } else {
      spdlog::warn("invalid config value shortcuts.{}: '{}'; ignoring the chord '{}'", key, value, text);
    }
  }
  return bound;
}

// Binds one [shortcuts] setting into `chords`, and records the binding and where it came from (default, config,
// alias:<key> or invalid) for the vars file.
void BindSetting(const toml::table* shortcuts, toml::table& effective, toml::table& sources, const Setting& setting,
                 std::vector<KeyChord>& chords)
{
  // The setting itself, else the first deprecated alias present, else the default.
  std::string_view  key  = setting.key;
  const toml::node* node = shortcuts ? shortcuts->get(key) : nullptr;
  for (const auto alias : setting.aliases) {
    const auto* alias_node = shortcuts && !alias.empty() ? shortcuts->get(alias) : nullptr;
    if (!alias_node)
      continue;
    if (node) {
      spdlog::warn("deprecated config value shortcuts.{}; ignored because shortcuts.{} is set", alias, setting.key);
      continue;
    }
    spdlog::warn("deprecated config value shortcuts.{}; use shortcuts.{}", alias, setting.key);
    key  = alias;
    node = alias_node;
    break;
  }
  const std::optional<std::string> configured = node ? node->value<std::string>() : std::nullopt;
  if (node && !configured)
    spdlog::warn("invalid config value shortcuts.{}: not a string; using the default {}", key, setting.fallback);
  std::string label = !configured ? "default" : key == setting.key ? "config" : std::format("alias:{}", key);

  const auto record = [&](std::string value, std::string source) {
    spdlog::debug("config shortcuts.{} = {}", setting.key, value);
    effective.insert_or_assign(setting.key, std::move(value));
    sources.insert_or_assign(setting.key, std::move(source));
  };

  // An explicit empty string disables the binding, just like NONE.
  const auto value = AsciiStrToUpper(StripAsciiWhitespace(configured.value_or(std::string(setting.fallback))));
  if (value.empty() || value == "NONE")
    return record("NONE", label);

  if (!BindChords(chords, key, value)) {
    if (configured)
      spdlog::warn("invalid config value shortcuts.{}: '{}'; using the default {}", key, *configured, setting.fallback);
    if (!configured || !BindChords(chords, setting.key, setting.fallback)) {
      // A broken entry in shortcuts.def, not a user mistake.
      spdlog::error("default shortcut shortcuts.{} = '{}' has no valid chord; the action is unbound", setting.key,
                    setting.fallback);
      return record("NONE", "invalid");
    }
    label = "default";
  }
  record(DescribeChords(chords), label);
}
} // namespace

KeyChord KeyChord::Parse(std::string_view text)
{
  KeyChord chord;
  for (auto& token : StrSplit(AsciiStrToUpper(text), '-')) {
    if (const auto* modifier = std::ranges::find(kModifiers, token, &KeyChord::Modifier::name);
        modifier != std::end(kModifiers)) {
      chord.modifiers.push_back(modifier);
    } else if (const auto key = FindKey(token)) {
      // A chord has one key; a later one replaces an earlier one, so the text shows what is bound.
      chord.key       = key->key;
      chord.key_name_ = std::move(token);
    }
  }
  return chord;
}

std::optional<KeyChord> KeyChord::ParseExact(std::string_view text)
{
  const auto normalized = AsciiStrToUpper(StripAsciiWhitespace(text));
  auto       chord      = Parse(normalized);
  if (chord.key == unity::KeyCode::None || chord.Text() != normalized)
    return {};
  for (size_t index = 0; index < chord.modifiers.size(); ++index)
    if (std::find(chord.modifiers.begin(), chord.modifiers.begin() + index, chord.modifiers[index])
        != chord.modifiers.begin() + index)
      return {};
  return chord;
}

std::string KeyChord::Text() const
{
  std::string text;
  for (const auto* modifier : modifiers) {
    text += modifier->name;
    text += '-';
  }
  text += key_name_;
  if (key_name_.empty() && !text.empty())
    text.pop_back();
  return text;
}

std::string KeyChord::Hint() const
{
  std::string hint;
  for (const auto* modifier : modifiers)
    hint += modifier->hint;
  const auto key = FindKey(key_name_);
  hint += key && !key->hint.empty() ? key->hint : std::string_view(key_name_);
  return hint;
}

namespace Shortcut
{
std::span<const ActionInfo> Catalog() noexcept
{
  static constexpr auto catalog = [] {
    std::array<ActionInfo, std::size(kSettings)> result{};
    for (size_t index = 0; index < result.size(); ++index) {
      const auto& source = kSettings[index];
      result[index]      = {source.action, source.key, source.fallback, source.tier != Tier::kAlways,
                            source.tier == Tier::kExperimental};
    }
    return result;
  }();
  return catalog;
}

std::string KeyToken(unity::KeyCode key)
{
  using unity::KeyCode;
  if (key >= KeyCode::A && key <= KeyCode::Z)
    return std::string(1, static_cast<char>('A' + std::to_underlying(key) - std::to_underlying(KeyCode::A)));
  if (key >= KeyCode::Alpha0 && key <= KeyCode::Alpha9)
    return std::string(1, static_cast<char>('0' + std::to_underlying(key) - std::to_underlying(KeyCode::Alpha0)));
  if (key >= KeyCode::F1 && key <= KeyCode::F15)
    return "F" + std::to_string(1 + std::to_underlying(key) - std::to_underlying(KeyCode::F1));
  if (key >= KeyCode::Keypad0 && key <= KeyCode::Keypad9)
    return "KEY" + std::to_string(std::to_underlying(key) - std::to_underlying(KeyCode::Keypad0));
  const auto found = std::ranges::find(kKeys, key, &KeyName::key);
  return found == std::end(kKeys) ? std::string{} : std::string(found->name);
}

BindingSnapshot Read(Action action)
{
  if (std::to_underlying(action) >= bindings.size())
    return {};
  BindingSnapshot result{{}, binding_generation};
  result.alternatives.reserve(ChordsOf(action).size());
  for (const auto& chord : ChordsOf(action))
    result.alternatives.push_back(chord.Text());
  return result;
}

static bool ReplaceAlternatives(Action action, std::span<const std::string> alternatives, uint64_t expected_generation,
                                bool preserve_duplicates)
{
  if (std::to_underlying(action) >= bindings.size() || expected_generation == 0
      || expected_generation != binding_generation)
    return false;
  std::vector<KeyChord> parsed;
  parsed.reserve(alternatives.size());
  for (const auto& text : alternatives) {
    auto chord = KeyChord::ParseExact(text);
    if (!chord || (!preserve_duplicates && std::ranges::any_of(parsed, [&](const auto& previous) {
          return previous.Text() == chord->Text();
        })))
      return false;
    parsed.push_back(std::move(*chord));
  }
  // Validation is complete before any live state or input ownership is changed.
  auto next                        = bindings;
  next[std::to_underlying(action)] = std::move(parsed);
  Set(std::move(next));
  return true;
}

bool Replace(Action action, std::span<const std::string> alternatives, uint64_t expected_generation)
{ return ReplaceAlternatives(action, alternatives, expected_generation, false); }

bool Restore(Action action, const BindingSnapshot& previous, uint64_t expected_generation)
{ return previous.generation != 0 && ReplaceAlternatives(action, previous.alternatives, expected_generation, true); }

std::optional<bool> MayOverlap(const KeyChord& first, const KeyChord& second)
{
  if (first.key == unity::KeyCode::None || second.key == unity::KeyCode::None)
    return false;
  const auto a = keyboard_layout::DescribeChord(first.key);
  const auto b = keyboard_layout::DescribeChord(second.key);
  if (a.key == unity::KeyCode::None || b.key == unity::KeyCode::None)
    return {};
  if (a.key != b.key)
    return false;
  // Modified chords accept additional modifiers. A bare chord admits none, except the Shift needed to type
  // its printable character. This is the same restriction used by Matches, without polling/claiming keys.
  const auto admits = [](const KeyChord& bare, bool inferred_shift, const KeyChord& other, bool other_shift) {
    if (!bare.modifiers.empty())
      return true;
    if (!inferred_shift)
      return other.modifiers.empty() && !other_shift;
    return std::ranges::all_of(other.modifiers, [](const auto* modifier) {
      return std::ranges::any_of(modifier->keys, [](auto key) {
        return key == unity::KeyCode::LeftShift || key == unity::KeyCode::RightShift;
      });
    });
  };
  return admits(first, a.shift, second, b.shift) && admits(second, b.shift, first, a.shift);
}

Bindings Parse(const toml::table& config, toml::table& parsed, bool extended, bool experimental)
{
  Bindings    result;
  const auto* shortcuts = config["shortcuts"].as_table();
  auto&       effective = *parsed.emplace<toml::table>("shortcuts").first->second.as_table();
  auto&       sources   = *parsed.emplace<toml::table>("shortcuts_source").first->second.as_table();

  // A shortcut whose tier is off stays unbound and is left out of the vars file.
  for (const auto& setting : kSettings) {
    if (setting.tier == Tier::kExtended && !extended)
      continue;
    if (setting.tier == Tier::kExperimental && !(extended && experimental))
      continue;
    BindSetting(shortcuts, effective, sources, setting, result[std::to_underlying(setting.action)]);
  }
  return result;
}

void Set(Bindings next) noexcept
{
  bindings = std::move(next);
  if (++binding_generation == 0)
    ++binding_generation;
  used_keys.clear();
  for (const auto& chords : bindings)
    for (const auto& chord : chords)
      keyboard_layout::RegisterShortcut(chord.key);
}

CaptureToken ClaimCapture() noexcept
{ return capture_guard.Claim(); }

bool CaptureOwnedBy(CaptureToken token) noexcept
{ return capture_guard.Owns(token); }

bool CaptureReady(CaptureToken token) noexcept
{ return capture_guard.Ready(token); }

bool ReleaseCapture(CaptureToken token) noexcept
{ return capture_guard.Release(token); }

bool CaptureBlocksShortcuts() noexcept
{ return capture_guard.BlocksShortcuts(); }

void UpdateCapture(bool focused, bool held) noexcept
{ capture_guard.BeforeDispatch(focused, held); }

bool Owns(unity::KeyCode key)
{ return capture_guard.BlocksShortcuts() ? unity::Key::Pressed(key) : IsUsed(key); }

bool HasBinding(Action action)
{
  return std::ranges::any_of(ChordsOf(action), [](const auto& chord) { return chord.key != unity::KeyCode::None; });
}

bool IsPressed(Action action)
{ return !capture_guard.BlocksShortcuts() && Matches<unity::Key::Pressed>(action); }

bool IsDown(Action action)
{ return !capture_guard.BlocksShortcuts() && Matches<unity::Key::Down>(action); }

bool IsDownWithShift(Action action)
{
  return !capture_guard.BlocksShortcuts()
         && (unity::Key::Pressed(unity::KeyCode::LeftShift) || unity::Key::Pressed(unity::KeyCode::RightShift))
         && Matches<unity::Key::Down, true>(action);
}

std::string Describe(Action action)
{ return DescribeChords(ChordsOf(action)); }

std::string Hint(Action action)
{
  // The game's badges have room for one binding.
  const auto& chords = ChordsOf(action);
  return chords.empty() ? std::string{} : chords.front().Hint();
}
} // namespace Shortcut
