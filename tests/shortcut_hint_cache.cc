// Link against the built mods library. Unity-dependent Key and layout operations are stubbed;
// MapKey parsing, ModifierKey parsing, binding detection, and hint caching are production code.
#include "patches/mapkey.h"
#include "patches/keyboard_layout_mapping.h"

#include <cstdlib>
#include <iostream>

static std::array<bool, static_cast<int>(KeyCode::Max)> pressed{};
static keyboard_layout::BindingState layout_bindings;
static bool layout_enabled = false;

KeyCode Key::Parse(std::string_view key)
{
  if (key == "F7") return KeyCode::F7;
  if (key == "F8") return KeyCode::F8;
  if (key == "G") return KeyCode::G;
  if (key == "+") return KeyCode::Plus;
  if (key == "/") return KeyCode::Slash;
  if (key == "(") return KeyCode::LeftParen;
  if (key == "1") return KeyCode::Alpha1;
  return KeyCode::None;
}
bool Key::IsModifier(KeyCode) { return false; }
bool Key::Pressed(KeyCode key) { return pressed[static_cast<int>(key)]; }
bool Key::Down(KeyCode key) { return Key::Pressed(key); }
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

  // Exercise production action lookup and explicit modifier matching with the
  // production mapping state. Only the Unity input boundary is simulated.
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
  Check(MapKey::IsDown(GameFunction::ShowDaily) && MapKey::IsPressed(GameFunction::ShowDaily),
        "Punctuation action did not use resolved physical key");
  pressed[static_cast<int>(KeyCode::LeftShift)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowDaily), "Unmodified symbol unexpectedly accepts Shift");
  pressed[static_cast<int>(KeyCode::Alpha7)] = true;
  Check(MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit Shift chord did not use resolved symbol key");
  pressed[static_cast<int>(KeyCode::LeftShift)] = false;
  Check(!MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit modifier requirement was lost");
  pressed[static_cast<int>(KeyCode::LeftParen)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowResearch), "Unresolved symbol silently fell back to physical");
  pressed[static_cast<int>(KeyCode::Alpha1)] = true;
  Check(MapKey::IsDown(GameFunction::ShowInventory), "Digit action did not use resolved mapping");
  Check(MapKey::GetShortcuts(GameFunction::ShowScrapYard) == "SHIFT-/", "Resolution rewrote configured chord");
  layout_enabled = false;
  pressed.fill(false);
  pressed[static_cast<int>(KeyCode::Plus)] = true;
  Check(MapKey::IsDown(GameFunction::ShowDaily), "Physical mode no longer uses configured key");
  std::cout << "Shortcut hint cache tests passed\n";
}
