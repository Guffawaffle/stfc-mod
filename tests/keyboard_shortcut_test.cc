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

  Bind("[shortcuts]\nselect_ship1='A'\n");
  Hold({KeyCode::A});
  Check(!Shortcut::IsDownWithShift(Action::kSelectShip1), "towing requires a held Shift");
  Hold({KeyCode::A, KeyCode::RightShift});
  Check(!Shortcut::IsDown(Action::kSelectShip1) && Shortcut::IsDownWithShift(Action::kSelectShip1),
        "towing adds Shift to the configured selection binding");
  Check(Shortcut::Owns(KeyCode::A), "towing claims the resolved physical key");
  Hold({KeyCode::A});
  Check(!Shortcut::IsDown(Action::kSelectShip1), "releasing Shift cannot turn a held tow key into ordinary selection");
  Hold({});
  Check(!Shortcut::Owns(KeyCode::A), "releasing the tow key retires ownership");
  Hold({KeyCode::A});
  Check(Shortcut::IsDown(Action::kSelectShip1), "ordinary selection resumes on a fresh key press");
  Hold({KeyCode::A, KeyCode::LeftShift, KeyCode::LeftControl});
  Check(!Shortcut::IsDownWithShift(Action::kSelectShip1), "bare towing bindings never steal Control chords");
  const auto capture = Shortcut::ClaimCapture();
  Hold({KeyCode::A, KeyCode::LeftShift});
  Check(!Shortcut::IsDownWithShift(Action::kSelectShip1), "Settings capture blocks towing too");
  Check(Shortcut::ReleaseCapture(capture), "tow fixture releases its capture owner");
  Hold({});
  Shortcut::UpdateCapture(true, false);
  Shortcut::UpdateCapture(true, false);
  Shortcut::UpdateCapture(true, false);

  keyboard_layout::Configure("layout");
  fixture::Map(KeyCode::A, KeyCode::Q);
  Bind("[shortcuts]\nselect_ship1='A'\n");
  Hold({KeyCode::Q, KeyCode::LeftShift});
  Check(Shortcut::IsDownWithShift(Action::kSelectShip1) && Shortcut::Owns(KeyCode::Q),
        "towing shares printable-layout resolution and physical ownership");
  Hold({KeyCode::A, KeyCode::LeftShift});
  Check(!Shortcut::IsDownWithShift(Action::kSelectShip1),
        "towing never falls back to the configured key's old position");
  Bind("[shortcuts]\nselect_ship1='CTRL-A'\n");
  Hold({KeyCode::Q, KeyCode::LeftShift});
  Check(!Shortcut::IsDownWithShift(Action::kSelectShip1), "towing does not replace configured required modifiers");
  Hold({KeyCode::Q, KeyCode::LeftShift, KeyCode::LeftControl});
  Check(Shortcut::IsDownWithShift(Action::kSelectShip1), "towing preserves configured modifier precedence");
  keyboard_layout::Configure("physical");
  Bind("[shortcuts]\nshow_galaxy='G'\nshow_chat='C'\n");
  const auto source = Shortcut::Read(Action::kShowGalaxy);
  Check(source.generation != 0 && source.alternatives == std::vector<std::string>{"G"},
        "per-action drafts retain configured alternatives and an accepted generation");
  const std::array replacements{std::string("ctrl-f8"), std::string("F9")};
  Check(Shortcut::Replace(Action::kShowGalaxy, replacements, source.generation)
            && Shortcut::Describe(Action::kShowGalaxy) == "CTRL-F8 | F9"
            && Shortcut::Describe(Action::kShowChat) == "C",
        "one action updates atomically through the existing parser without replacing other actions");
  Check(!Shortcut::Replace(Action::kShowGalaxy, source.alternatives, source.generation),
        "a stale draft cannot overwrite a more recent edit");
  auto current = Shortcut::Read(Action::kShowGalaxy);
  for (const auto* invalid : {"UNKNOWN-F8", "CTRL-A-B", "CTRL-CTRL-A", "CTRL-F8|F9", "NONE", "CTRL"}) {
    const std::array value{std::string(invalid)};
    Check(!Shortcut::Replace(Action::kShowGalaxy, value, current.generation)
              && Shortcut::Read(Action::kShowGalaxy).generation == current.generation,
          "editor rejection leaves both native bindings and the draft generation unchanged");
  }
  const std::array duplicates{std::string("F8"), std::string("f8")};
  Check(!Shortcut::Replace(Action::kShowGalaxy, duplicates, current.generation),
        "case-normalized duplicate alternatives do not get published");
  Check(Shortcut::Replace(Action::kShowGalaxy, {}, current.generation) && !Shortcut::HasBinding(Action::kShowGalaxy),
        "an explicit empty alternative list disables one action");
  current = Shortcut::Read(Action::kShowChat);
  Bind("[shortcuts]\nshow_chat='F12'\n");
  Check(!Shortcut::Replace(Action::kShowChat, current.alternatives, current.generation),
        "whole configuration reload invalidates a pending per-action edit");
  Check(Shortcut::Catalog().size() == std::to_underlying(Action::kCount)
            && std::ranges::any_of(Shortcut::Catalog(),
                                   [](const auto& item) {
                                     return item.action == Action::kShowGalaxy && item.key == "show_galaxy"
                                            && !item.defaults.empty();
                                   }),
        "editor catalog comes from the same action definitions as dispatch and configuration");
  Check(Shortcut::Read(Action::kCount).generation == 0
            && !Shortcut::Replace(Action::kCount, {}, Shortcut::Read(Action::kShowChat).generation),
        "invalid action identities cannot index outside the native binding table");
  Bind("[shortcuts]\nshow_chat='F8|F8'\n");
  const auto duplicate_source = Shortcut::Read(Action::kShowChat);
  Check(Shortcut::Replace(Action::kShowChat, {}, duplicate_source.generation)
            && Shortcut::Restore(Action::kShowChat, duplicate_source, Shortcut::Read(Action::kShowChat).generation)
            && Shortcut::Read(Action::kShowChat).alternatives == duplicate_source.alternatives,
        "undo preserves an observed duplicate list without allowing duplicate new alternatives");
  Check(!Shortcut::Restore(Action::kShowChat, duplicate_source, duplicate_source.generation),
        "undo still refuses a stale global generation");
  for (const auto& pair : {std::pair{"A", "A"}, {"CTRL-A", "ALT-A"}, {"LSHIFT-A", "RSHIFT-A"}})
    Check(Shortcut::MayOverlap(KeyChord::Parse(pair.first), KeyChord::Parse(pair.second)) == true,
          "owner overlap reports chords that can match simultaneously");
  for (const auto& pair : {std::pair{"A", "SHIFT-A"}, {"A", "CTRL-A"}, {"CTRL-A", "CTRL-B"}})
    Check(Shortcut::MayOverlap(KeyChord::Parse(pair.first), KeyChord::Parse(pair.second)) == false,
          "owner overlap preserves bare-key restrictions and physical key identity");
  keyboard_layout::Configure("layout");
  fixture::Map(KeyCode::Slash, KeyCode::Alpha7, true);
  Check(Shortcut::MayOverlap(KeyChord::Parse("/"), KeyChord::Parse("SHIFT-7")) == true
            && Shortcut::MayOverlap(KeyChord::Parse("/"), KeyChord::Parse("7")) == false,
        "inferred Shift is shared by overlap and dispatch semantics");
  fixture::Map(KeyCode::Slash, KeyCode::None);
  Check(!Shortcut::MayOverlap(KeyChord::Parse("/"), KeyChord::Parse("7")).has_value(),
        "unavailable printable mappings do not become a false no-overlap claim");
  for (auto key : {KeyCode::A, KeyCode::Z, KeyCode::Alpha0, KeyCode::Alpha9, KeyCode::F1, KeyCode::F15,
                   KeyCode::Keypad0, KeyCode::Keypad9, KeyCode::Slash, KeyCode::Minus, KeyCode::Mouse2}) {
    const auto parsed = KeyChord::ParseExact(Shortcut::KeyToken(key));
    Check(parsed && parsed->key == key, "raw capture tokens round-trip through the existing parser catalog");
  }
  Check(Shortcut::KeyToken(KeyCode::None).empty() && Shortcut::KeyToken(KeyCode::LeftShift).empty(),
        "unsupported and modifier keys cannot become an invented standalone capture token");
  std::cout << "Keyboard shortcut adapter passed: mapping, towing, capture and atomic per-action editing\n";
}
