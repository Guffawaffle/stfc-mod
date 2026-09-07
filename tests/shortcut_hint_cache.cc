// Link production MapKey/ModifierKey parsing, action dispatch and hint caching from mods.lib.
// Key token parsing and input are test fixtures; layout lookup is injected below.
// These tests do not exercise Unity lookup, native notifications or legacy input caching.
#include "patches/mapkey.h"
#include "patches/keyboard_layout_mapping.h"

#include <cstdlib>
#include <iostream>
#include <utility>

static std::array<bool, static_cast<int>(KeyCode::Max)> pressed{};
static std::array<bool, static_cast<int>(KeyCode::Max)> down{};
static keyboard_layout::BindingState layout_bindings;
static bool layout_enabled = false;

KeyCode Key::Parse(std::string_view key)
{
  static constexpr std::pair<std::string_view, KeyCode> tokens[] = {
      {"F7", KeyCode::F7}, {"F8", KeyCode::F8}, {"G", KeyCode::G}, {"+", KeyCode::Plus},
      {"/", KeyCode::Slash}, {"(", KeyCode::LeftParen}, {"1", KeyCode::Alpha1},
  };
  for (const auto& [token, code] : tokens) {
    if (key == token)
      return code;
  }
  return KeyCode::None;
}
bool Key::IsModifier(KeyCode) { return false; }
bool Key::Pressed(KeyCode key) { return pressed[static_cast<int>(key)]; }
bool Key::Down(KeyCode key) { return down[static_cast<int>(key)]; }
bool Key::IsModified() { return Key::Pressed(KeyCode::LeftShift); }
void Key::ClaimDirectionalInput(KeyCode) {}

namespace keyboard_layout
{
KeyCode Resolve(KeyCode configured) { return layout_enabled ? layout_bindings.Resolve(configured, [] { return 1; }, Key::Pressed) : configured; }
} // namespace keyboard_layout

void Check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main()
{
  constexpr auto toggle = GameFunction::ToggleShortcutHints;
  constexpr auto galaxy = GameFunction::ShowGalaxy;
  Check(!MapKey::HasBinding(toggle), "Absent binding must not enable hints");

  // Match config registration: only keys with a primary key are accepted.
  for (const auto* text : {"NONE", "", "INVALID", "CTRL"}) {
    auto parsed = MapKey::Parse(text);
    Check(parsed.Key == KeyCode::None, "Disabled/invalid binding acquired a key");
    if (parsed.Key != KeyCode::None) MapKey::AddMappedKey(toggle, std::move(parsed));
    Check(!MapKey::HasBinding(toggle), "Disabled/invalid binding enabled hints");
  }

  MapKey::AddMappedKey(galaxy, MapKey::Parse("CTRL-G"));
  MapKey::AddMappedKey(galaxy, MapKey::Parse("F8"));
  Check(MapKey::GetShortcutHint(galaxy).empty(), "Parsing must not populate hint cache");
  Check(MapKey::GetShortcuts(galaxy) == "CTRL-G | F8", "Normal shortcut labels changed");

  // The toggle may be parsed after bindings whose badges it enables.
  MapKey::AddMappedKey(toggle, MapKey::Parse("SHIFT-F7"));
  Check(MapKey::HasBinding(toggle), "Valid chord must enable hints");
  Check(MapKey::GetShortcutHint(toggle).empty(), "Binding registration must not populate hint cache");
  Check(MapKey::GetShortcutHint(galaxy).empty(), "Registering toggle must not format earlier bindings");
  MapKey::CacheShortcutHints();
  Check(MapKey::GetShortcutHint(galaxy) == "^G", "Cache must include earlier binding, first alternative only");
  Check(MapKey::GetShortcutHint(toggle) == "+F7", "Toggle badge missing");
  Check(MapKey::GetShortcutHint(GameFunction::ShowResearch).empty(), "Unbound action acquired a badge");
  MapKey::CacheShortcutHints();
  Check(MapKey::GetShortcutHint(galaxy) == "^G", "Repeated cache preparation changed label");

  // Supply a mapping fixture to test action dispatch through BindingState.
  // This does not assert that Unity returns these mappings on any real layout.
  keyboard_layout::LayoutKeys keys{};
  keys[static_cast<int>(KeyCode::Plus)] = KeyCode::RightBracket;
  keys[static_cast<int>(KeyCode::Slash)] = KeyCode::Alpha7;
  keys[static_cast<int>(KeyCode::Alpha1)] = KeyCode::Alpha1;
  layout_bindings.Replace(keys, Key::Pressed, 0);
  layout_enabled = true;
  MapKey::AddMappedKey(GameFunction::ShowDaily, MapKey::Parse("+"));
  MapKey::AddMappedKey(GameFunction::ShowScrapYard, MapKey::Parse("SHIFT-/"));
  MapKey::AddMappedKey(GameFunction::ShowResearch, MapKey::Parse("("));
  MapKey::AddMappedKey(GameFunction::ShowInventory, MapKey::Parse("1"));
  pressed[static_cast<int>(KeyCode::RightBracket)] = true;
  down[static_cast<int>(KeyCode::RightBracket)] = true;
  Check(MapKey::IsDown(GameFunction::ShowDaily) && MapKey::IsPressed(GameFunction::ShowDaily),
        "Punctuation action did not use resolved physical key");
  down[static_cast<int>(KeyCode::RightBracket)] = false;
  Check(!MapKey::IsDown(GameFunction::ShowDaily) && MapKey::IsPressed(GameFunction::ShowDaily),
        "Held action was mistaken for a new key-down edge");
  down[static_cast<int>(KeyCode::RightBracket)] = true;
  pressed[static_cast<int>(KeyCode::LeftShift)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowDaily), "Unmodified symbol unexpectedly accepts Shift");
  pressed[static_cast<int>(KeyCode::Alpha7)] = true;
  down[static_cast<int>(KeyCode::Alpha7)] = true;
  Check(MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit Shift chord did not use resolved symbol key");
  pressed[static_cast<int>(KeyCode::LeftShift)] = false;
  Check(!MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit modifier requirement was lost");
  pressed[static_cast<int>(KeyCode::LeftParen)] = true;
  down[static_cast<int>(KeyCode::LeftParen)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowResearch), "Unresolved symbol silently fell back to physical");
  pressed[static_cast<int>(KeyCode::Alpha1)] = true;
  down[static_cast<int>(KeyCode::Alpha1)] = true;
  Check(MapKey::IsDown(GameFunction::ShowInventory), "Digit action did not use resolved mapping");
  Check(MapKey::GetShortcuts(GameFunction::ShowScrapYard) == "SHIFT-/", "Resolution rewrote configured chord");
  layout_enabled = false;
  pressed.fill(false);
  down.fill(false);
  pressed[static_cast<int>(KeyCode::Plus)] = true;
  down[static_cast<int>(KeyCode::Plus)] = true;
  Check(MapKey::IsDown(GameFunction::ShowDaily), "Physical mode no longer uses configured key");
  std::cout << "Shortcut hint cache tests passed\n";
}
