#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "patches/key.h"

TEST_CASE("Key::Parse handles common shortcut names")
{
  CHECK(Key::Parse("SPACE") == KeyCode::Space);
  CHECK(Key::Parse("escape") == KeyCode::Escape);
  CHECK(Key::Parse("pgup") == KeyCode::PageUp);
  CHECK(Key::Parse("RETURN") == KeyCode::Return);
  CHECK(Key::Parse("MOUSE1") == KeyCode::Mouse1);
}

TEST_CASE("Key::Parse handles alphanumeric keys")
{
  CHECK(Key::Parse("A") == KeyCode::A);
  CHECK(Key::Parse("z") == KeyCode::Z);
  CHECK(Key::Parse("0") == KeyCode::Alpha0);
  CHECK(Key::Parse("8") == KeyCode::Alpha8);
}

TEST_CASE("Key::Parse handles function and keypad keys")
{
  CHECK(Key::Parse("F1") == KeyCode::F1);
  CHECK(Key::Parse("f12") == KeyCode::F12);
  CHECK(Key::Parse("KEY0") == KeyCode::Keypad0);
  CHECK(Key::Parse("keyplus") == KeyCode::KeypadPlus);
  CHECK(Key::Parse("KEYENTER") == KeyCode::KeypadEnter);
}

TEST_CASE("Key::Parse handles punctuation keys")
{
  CHECK(Key::Parse("-") == KeyCode::None);
  CHECK(Key::Parse("MINUS") == KeyCode::Minus);
  CHECK(Key::Parse("=") == KeyCode::Equals);
  CHECK(Key::Parse("`") == KeyCode::BackQuote);
  CHECK(Key::Parse("\\") == KeyCode::Backslash);
}

TEST_CASE("Key::Parse returns None for unknown names")
{
  CHECK(Key::Parse("") == KeyCode::None);
  CHECK(Key::Parse("NO_SUCH_KEY") == KeyCode::None);
  CHECK(Key::Parse(" SPACE ") == KeyCode::None);
}

TEST_CASE("Key::IsModifier identifies supported modifiers")
{
  CHECK(Key::IsModifier(KeyCode::LeftAlt));
  CHECK(Key::IsModifier(KeyCode::RightAlt));
  CHECK(Key::IsModifier(KeyCode::LeftControl));
  CHECK(Key::IsModifier(KeyCode::RightControl));
  CHECK(Key::IsModifier(KeyCode::LeftShift));
  CHECK(Key::IsModifier(KeyCode::RightShift));
  CHECK(Key::IsModifier(KeyCode::LeftCommand));
  CHECK(Key::IsModifier(KeyCode::RightCommand));
  CHECK(Key::IsModifier(KeyCode::LeftApple));
  CHECK(Key::IsModifier(KeyCode::RightApple));
  CHECK(Key::IsModifier(KeyCode::LeftWindows));
  CHECK(Key::IsModifier(KeyCode::RightWindows));
  CHECK(Key::IsModifier(KeyCode::AltGr));
}

TEST_CASE("Key::IsModifier rejects non-modifier keys")
{
  CHECK_FALSE(Key::IsModifier(KeyCode::A));
  CHECK_FALSE(Key::IsModifier(KeyCode::Alpha1));
  CHECK_FALSE(Key::IsModifier(KeyCode::Escape));
  CHECK_FALSE(Key::IsModifier(KeyCode::Mouse1));
  CHECK_FALSE(Key::IsModifier(KeyCode::None));
}
