#include "keyboard_layout_test_host.h"
#include "shortcuts.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <toml++/toml.h>

namespace
{
std::array<bool, std::to_underlying(unity::KeyCode::Max)> held{};
void                                                      Check(bool value, const char* message)
{
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void Hold(std::initializer_list<unity::KeyCode> keys)
{
  held.fill(false);
  for (auto key : keys)
    held[std::to_underlying(key)] = true;
}
void Bind(const char* values)
{
  toml::table parsed;
  Shortcut::Set(Shortcut::Parse(toml::parse(values), parsed, true, false));
  held.fill(false);
}
} // namespace

namespace unity::Key
{
bool Pressed(KeyCode key)
{ return held[std::to_underlying(key)]; }
bool Down(KeyCode key)
{ return Pressed(key); }
bool IsModified()
{
  constexpr auto modifiers =
      std::array{KeyCode::LeftShift,    KeyCode::RightShift,  KeyCode::LeftControl, KeyCode::RightControl,
                 KeyCode::LeftAlt,      KeyCode::RightAlt,    KeyCode::AltGr,       KeyCode::LeftCommand,
                 KeyCode::RightCommand, KeyCode::LeftWindows, KeyCode::RightWindows};
  return std::ranges::any_of(modifiers, Pressed);
}
} // namespace unity::Key

int main()
{
  using unity::KeyCode;
  namespace fixture = keyboard_layout::fixture;
  fixture::Reset();
  Bind("[shortcuts]\nshow_galaxy='A'\n");
  Hold({KeyCode::A});
  Check(Shortcut::IsDown(Action::kShowGalaxy), "physical mode keeps the existing chord identity");
  keyboard_layout::Configure("layout");
  fixture::Map(KeyCode::A, KeyCode::Q);
  Bind("[shortcuts]\nshow_galaxy='A'\n");
  Check(fixture::Registered(KeyCode::A), "accepted bindings register their configured characters");
  Hold({KeyCode::Q});
  Check(Shortcut::IsDown(Action::kShowGalaxy), "existing matcher reads the resolved physical key");
  Hold({KeyCode::A});
  Check(!Shortcut::IsDown(Action::kShowGalaxy), "configured text does not also fire at its old physical position");
  Check(Shortcut::Describe(Action::kShowGalaxy) == "A", "mapping never mutates persisted or displayed chord text");

  fixture::Map(KeyCode::Slash, KeyCode::Alpha7, true);
  Bind("[shortcuts]\nshow_galaxy='/'\nshow_chat='7'\n");
  Hold({KeyCode::Alpha7});
  Check(!Shortcut::IsPressed(Action::kShowGalaxy), "inferred Shift is required for the mapped character");
  Hold({KeyCode::Alpha7, KeyCode::LeftShift});
  Check(Shortcut::IsPressed(Action::kShowGalaxy) && Shortcut::IsPressed(Action::kShowGalaxy),
        "inferred-Shift actions continue to match while held");
  Check(Shortcut::Owns(KeyCode::Alpha7) && !Shortcut::Owns(KeyCode::Slash), "ownership tracks the physical key");
  Hold({KeyCode::Alpha7});
  Check(!Shortcut::IsPressed(Action::kShowChat), "dropping Shift cannot turn the held chord into a plain action");
  Hold({});
  Check(!Shortcut::Owns(KeyCode::Alpha7), "release ends physical ownership");
  Hold({KeyCode::Alpha7});
  Check(Shortcut::IsPressed(Action::kShowChat), "a later plain key press works after release");
  Hold({KeyCode::Alpha7, KeyCode::LeftShift, KeyCode::LeftControl});
  Check(!Shortcut::IsDown(Action::kShowGalaxy), "inferred Shift never admits extra Control to a bare binding");

  Bind("[shortcuts]\nshow_galaxy='CTRL-/'\n");
  Hold({KeyCode::Alpha7, KeyCode::LeftControl});
  Check(!Shortcut::IsPressed(Action::kShowGalaxy), "explicit Control does not replace inferred Shift");
  Hold({KeyCode::Alpha7, KeyCode::LeftControl, KeyCode::LeftShift});
  Check(Shortcut::IsPressed(Action::kShowGalaxy), "explicit and inferred modifiers both apply");

  Bind("[shortcuts]\nmove_right='/'\n");
  Hold({KeyCode::Alpha7, KeyCode::LeftShift});
  Check(Shortcut::IsPressed(Action::kMoveRight) && Shortcut::IsPressed(Action::kMoveRight)
            && !Shortcut::Owns(KeyCode::Alpha7),
        "movement keeps reading its direction without claiming itself");
  fixture::Map(KeyCode::Slash, KeyCode::None);
  Check(!Shortcut::IsPressed(Action::kMoveRight), "an unavailable mapped character cannot fall back to a guess");
  keyboard_layout::Configure("physical");
  Hold({KeyCode::Slash});
  Check(Shortcut::IsPressed(Action::kMoveRight), "physical mode remains independent of unavailable layout mappings");
  std::cout << "Keyboard shortcut adapter passed: physical identity, inferred modifiers, ownership and held release\n";
}
