#include "patches/keyboard_layout_mapping.h"
#include "patches/keyboard_layout_refresh.h"

#include <cstdlib>
#include <iostream>

using namespace keyboard_layout;

void Check(bool condition, const char* name)
{
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}

int main()
{
  Check(ToLegacyKey(39) == KeyCode::Y && ToLegacyKey(40) == KeyCode::Z, "distinct enum translation Y/Z");
  Check(ToLegacyKey(6) == KeyCode::Semicolon, "French M punctuation position");
  Check(ToLegacyKey(-1) == KeyCode::None && ToLegacyKey(0) == KeyCode::None && ToLegacyKey(51) == KeyCode::None
            && ToLegacyKey(9999) == KeyCode::None,
        "unsupported codes fail closed, including modifiers");

  LetterKeys us{};
  for (int i = 0; i < 26; ++i) {
    us[i] = ToLegacyKey(15 + i);
    Check(us[i] == static_cast<KeyCode>(static_cast<int>(KeyCode::A) + i), "all 26 US positions");
  }
  const auto   released  = [](KeyCode) { return false; };
  const auto   holding_y = [](KeyCode key) { return key == KeyCode::Y; };
  BindingState state;
  Check(state.Resolve(KeyCode::Z) == KeyCode::None, "pending lookup cannot fire");
  Check(state.Resolve(KeyCode::UpArrow) == KeyCode::UpArrow, "arrows unchanged");
  Check(state.Resolve(KeyCode::LeftControl) == KeyCode::LeftControl, "modifiers unchanged");
  Check(state.Resolve(KeyCode::Alpha1) == KeyCode::Alpha1, "configured nonletters unchanged");
  state.Replace(us, released);
  Check(state.Resolve(KeyCode::Z) == KeyCode::None, "initial transition frame suppressed");
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::Z) == KeyCode::Z && state.Resolve(KeyCode::Y) == KeyCode::Y, "US mapping");

  auto de = us;
  de[24]  = KeyCode::Z;
  de[25]  = KeyCode::Y;
  state.Replace(de, holding_y);
  Check(state.Resolve(KeyCode::Y) == KeyCode::None && state.Resolve(KeyCode::Z) == KeyCode::None,
        "German transition suppresses all letters");
  state.BeginFrame(holding_y);
  Check(state.Resolve(KeyCode::Y) == KeyCode::Z, "German Y uses US Z");
  Check(state.Resolve(KeyCode::Z) == KeyCode::None, "old held US Y cannot become new Z action");
  state.BeginFrame(holding_y);
  Check(state.Resolve(KeyCode::Z) == KeyCode::None, "hold suppression persists");
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::Z) == KeyCode::Y, "release enables German Z at US Y");
  state.Replace(us, released);
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::Z) == KeyCode::Z && state.Resolve(KeyCode::Y) == KeyCode::Y, "US restoration");

  auto fr = us;
  fr[0]   = KeyCode::Q;
  fr[16]  = KeyCode::A;
  fr[22]  = KeyCode::Z;
  fr[25]  = KeyCode::W;
  fr[12]  = ToLegacyKey(6);
  state.Replace(fr, released);
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::A) == KeyCode::Q && state.Resolve(KeyCode::Q) == KeyCode::A, "French A/Q");
  Check(state.Resolve(KeyCode::W) == KeyCode::Z && state.Resolve(KeyCode::Z) == KeyCode::W, "French W/Z");
  Check(state.Resolve(KeyCode::M) == KeyCode::Semicolon, "letter at punctuation position");

  fr[24] = KeyCode::None;
  state.Replace(fr, released);
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::Y) == KeyCode::None, "missing letter has no physical fallback");
  Check(state.Resolve(KeyCode::A) == KeyCode::Q, "partial lookup retains resolved letters");
  state.Replace(LetterKeys{}, released);
  state.BeginFrame(released);
  Check(state.Resolve(KeyCode::A) == KeyCode::None && state.Resolve(KeyCode::Z) == KeyCode::None,
        "device loss invalidates old mapping");

  RefreshState refresh;
  auto tick = refresh.Begin(10);
  Check(tick.new_frame && tick.invalidated && tick.CheckKeyboard(), "initial lookup required");
  tick = refresh.Begin(10);
  Check(!tick.new_frame && !tick.invalidated && !tick.CheckKeyboard(), "repeat query uses cache");
  tick = refresh.Begin(11);
  Check(tick.new_frame && !tick.invalidated && tick.CheckKeyboard(), "new frame still checks current keyboard");
  state.Replace(us, released);
  state.BeginFrame(released);
  refresh.Invalidate();
  refresh.Invalidate();
  tick = refresh.Begin(11);
  Check(!tick.new_frame && tick.invalidated && tick.CheckKeyboard(), "same-frame notifications coalesce but refresh");
  if (tick.new_frame)
    state.BeginFrame(holding_y);
  state.Replace(de, holding_y);
  tick = refresh.Begin(11);
  if (tick.new_frame)
    state.BeginFrame(holding_y);
  Check(!tick.CheckKeyboard() && state.Resolve(KeyCode::Y) == KeyCode::None,
        "another same-frame query cannot clear transition suppression");
  refresh.Invalidate(); // A notification during the preceding lookup must survive it.
  tick = refresh.Begin(11);
  Check(tick.invalidated && !tick.new_frame, "notification during refresh is not lost");
  state.Replace(de, holding_y); // Same-name/same-map notifications still rebuild.
  tick = refresh.Begin(12);
  if (tick.new_frame)
    state.BeginFrame(holding_y);
  Check(state.Resolve(KeyCode::Y) == KeyCode::Z && state.Resolve(KeyCode::Z) == KeyCode::None,
        "next frame clears transition but preserves held key after notification");
  tick = refresh.Begin(13);
  if (tick.new_frame)
    state.BeginFrame(released);
  Check(state.Resolve(KeyCode::Z) == KeyCode::Y, "release recovers mapping after notification");
  std::cout
      << "PASS: enum conversion, US/DE/FR, transitions, held keys, missing letters/device, nonletters, refresh timing\n";
}
