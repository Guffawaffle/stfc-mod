#include "settings/shortcut_capture_guard.h"
#include "shortcuts.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

namespace
{
std::array<bool, std::to_underlying(unity::KeyCode::Max)> held_keys{}, down_keys{};

void Check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void SetKeys(std::initializer_list<unity::KeyCode> held, std::initializer_list<unity::KeyCode> down = {})
{
  held_keys.fill(false);
  down_keys.fill(false);
  for (auto key : held)
    held_keys[std::to_underlying(key)] = true;
  for (auto key : down)
    down_keys[std::to_underlying(key)] = true;
}
void PollCapture(bool focused = true)
{ Shortcut::UpdateCapture(focused, std::ranges::any_of(held_keys, [](bool held) { return held; })); }
} // namespace

namespace unity::Key
{
bool Pressed(KeyCode key)
{ return held_keys[std::to_underlying(key)]; }
bool Down(KeyCode key)
{ return down_keys[std::to_underlying(key)]; }
bool IsModified()
{
  constexpr std::array modifiers{KeyCode::LeftAlt,      KeyCode::LeftControl,  KeyCode::LeftShift,
                                 KeyCode::RightAlt,     KeyCode::RightControl, KeyCode::RightShift,
                                 KeyCode::AltGr,        KeyCode::LeftCommand,  KeyCode::RightCommand,
                                 KeyCode::LeftWindows,  KeyCode::RightWindows};
  return std::ranges::any_of(modifiers, Pressed);
}
} // namespace unity::Key

void GuardPolicy()
{
  using mod_settings::ShortcutCaptureGuard;
  ShortcutCaptureGuard guard;
  Check(!guard.Owns(0) && !guard.Ready(0) && !guard.Release(0), "zero never identifies a capture");
  Check(!guard.BlocksShortcuts() && !guard.BeforeDispatch(true, false), "idle input reaches ordinary dispatch");

  const auto first = guard.Claim();
  Check(first != 0 && guard.Owns(first) && !guard.Ready(first), "claim owns input before recording is ready");
  Check(guard.BlocksShortcuts(), "a claim after the frame poll immediately blocks later shortcut matching");
  for (int i = 0; i < 3; ++i)
    Check(guard.BeforeDispatch(true, true) && !guard.Ready(first), "opening keys and held modifiers cannot record");
  Check(guard.BeforeDispatch(true, false) && !guard.Ready(first), "the first released frame still cannot record");
  Check(guard.BeforeDispatch(true, true) && !guard.Ready(first), "another held key resets initial release");
  Check(guard.BeforeDispatch(true, false) && !guard.Ready(first), "release starts again after the held key");
  Check(guard.BeforeDispatch(true, false) && guard.Ready(first), "a focused release boundary enables recording");
  Check(guard.BeforeDispatch(true, true) && guard.Ready(first),
        "recording may read raw held input while dispatch blocks");

  Check(guard.BeforeDispatch(false, true) && !guard.Ready(first), "blur revokes recording readiness");
  Check(guard.Owns(first), "blur leaves cancellation and draft ownership with the editor");
  Check(guard.BeforeDispatch(false, false) && !guard.Ready(first), "unfocused idle cannot arm capture");
  Check(guard.BeforeDispatch(true, true) && !guard.Ready(first), "refocus with a held key cannot arm capture");
  Check(guard.BeforeDispatch(true, false) && !guard.Ready(first), "refocus needs a fresh release boundary");
  Check(guard.BeforeDispatch(true, false) && guard.Ready(first), "recording resumes only after focused release");

  const auto replacement = guard.Claim();
  Check(replacement != first && guard.Owns(replacement), "replacement gets a distinct capture identity");
  Check(!guard.Ready(first) && !guard.Release(first), "stale unbind cannot change replacement ownership");
  Check(guard.Owns(replacement) && guard.BlocksShortcuts(), "replacement remains blocked after stale cancellation");
  Check(!guard.Ready(replacement), "replacement cannot inherit previous recording readiness");

  Check(guard.Release(replacement), "cancel releases the current editor");
  Check(!guard.Owns(replacement) && !guard.Ready(replacement), "released editor cannot record");
  Check(!guard.Release(replacement), "repeated unbind cannot restart the release boundary");
  for (int i = 0; i < 3; ++i)
    Check(guard.BeforeDispatch(true, true) && guard.BlocksShortcuts(), "popup Enter and held modifiers remain owned");
  Check(guard.BeforeDispatch(false, false), "closing while unfocused never returns input to gameplay");
  Check(guard.BeforeDispatch(true, false), "first focused release frame remains blocked");
  Check(guard.BeforeDispatch(true, false) && guard.BlocksShortcuts(),
        "final release frame stays blocked throughout dispatch");
  Check(!guard.BeforeDispatch(true, false) && !guard.BlocksShortcuts(), "a later frame can return to gameplay");

  const auto closing = guard.Claim();
  Check(guard.Release(closing), "an editor may unbind before its first input frame");
  Check(guard.BeforeDispatch(true, false), "immediate unbind still drains input");
  const auto reopened = guard.Claim();
  Check(!guard.Release(closing) && guard.Owns(reopened), "reopen during drain rejects an earlier unbind");
  Check(guard.BeforeDispatch(true, false) && !guard.Ready(reopened),
        "reopened capture starts its own release boundary");
  Check(guard.BeforeDispatch(true, false) && guard.Ready(reopened), "reopened capture can become ready");

  // Model dispatch order: claim/cancel can happen in earlier UI frame work. The gate runs before even a hotkey
  // mode toggle or awaited button, and does not depend on the old shortcut engine having matched a chord.
  int dispatches = 0;
  const auto dispatch = [&](bool focused, bool held) {
    if (!guard.BeforeDispatch(focused, held))
      ++dispatches;
  };
  dispatch(true, true);
  Check(dispatches == 0, "capture prevents all configured shortcut dispatch");
  Check(guard.Release(reopened), "accepting a capture starts the same drain as cancellation");
  dispatch(true, true);
  dispatch(true, true);
  dispatch(true, false);
  dispatch(true, false);
  Check(dispatches == 0, "accepted Enter cannot leak through the release boundary");
  dispatch(true, false);
  Check(dispatches == 1, "normal dispatch resumes after capture input is fully released");

}

void HostPolicy()
{
  using unity::KeyCode;
  Shortcut::Bindings bindings;
  bindings[std::to_underlying(Action::kShowGalaxy)]     = {KeyChord::Parse("RETURN")};
  bindings[std::to_underlying(Action::kShowChat)]       = {KeyChord::Parse("A")};
  bindings[std::to_underlying(Action::kMoveRight)]      = {KeyChord::Parse("RIGHT")};
  bindings[std::to_underlying(Action::kDisableHotKeys)] = {KeyChord::Parse("CTRL-A")};
  Shortcut::Set(bindings);
  Check(!Shortcut::CaptureBlocksShortcuts() && !Shortcut::ReleaseCapture(0), "production capture starts inactive");
  SetKeys({KeyCode::LeftControl, KeyCode::A}, {KeyCode::A});
  Check(Shortcut::IsDown(Action::kDisableHotKeys) && Shortcut::Owns(KeyCode::A),
        "inactive capture preserves modified-chord ownership");
  SetKeys({KeyCode::A});
  Check(!Shortcut::IsPressed(Action::kShowChat), "inactive capture preserves held modifier-release suppression");
  SetKeys({});
  Check(!Shortcut::Owns(KeyCode::A), "inactive ordinary ownership ends on physical release");

  SetKeys({KeyCode::Return}, {KeyCode::Return});
  Check(Shortcut::IsDown(Action::kShowGalaxy), "inactive capture preserves plain shortcut matching");
  const auto capture = Shortcut::ClaimCapture();
  Check(Shortcut::CaptureBlocksShortcuts() && !Shortcut::CaptureReady(capture),
        "production claim blocks immediately before its first frame update");
  Check(!Shortcut::IsDown(Action::kShowGalaxy) && unity::Key::Down(KeyCode::Return),
        "captured Enter remains readable by the editor without matching gameplay");
  Check(Shortcut::Owns(KeyCode::Return) && !Shortcut::Owns(KeyCode::RightArrow),
        "capture ownership follows held physical keys rather than inventing held keys");
  PollCapture();
  Check(!Shortcut::CaptureReady(capture), "opening Enter cannot arm production recording");
  SetKeys({KeyCode::LeftShift});
  PollCapture();
  Check(!Shortcut::CaptureReady(capture), "an unbound held modifier still prevents production recording");
  SetKeys({});
  PollCapture();
  Check(!Shortcut::CaptureReady(capture), "first production release frame does not arm recording");
  PollCapture();
  Check(Shortcut::CaptureReady(capture), "production recording becomes ready after a focused release boundary");

  SetKeys({KeyCode::RightArrow}, {KeyCode::RightArrow});
  PollCapture();
  Check(Shortcut::Owns(KeyCode::RightArrow) && !Shortcut::IsPressed(Action::kMoveRight),
        "capture owns camera arrows before held movement matching");
  SetKeys({KeyCode::LeftControl, KeyCode::A}, {KeyCode::A});
  Check(!Shortcut::IsDown(Action::kDisableHotKeys), "capture blocks even hotkey mode-toggle matching");
  Shortcut::Set(bindings);
  Check(!Shortcut::IsDown(Action::kDisableHotKeys) && Shortcut::CaptureReady(capture),
        "replacing live bindings neither releases capture nor duplicates its input owner");
  PollCapture(false);
  Check(!Shortcut::CaptureReady(capture) && Shortcut::CaptureBlocksShortcuts(), "production blur revokes readiness");

  const auto replacement = Shortcut::ClaimCapture();
  Check(!Shortcut::ReleaseCapture(capture) && Shortcut::CaptureBlocksShortcuts(),
        "a stale native unbind cannot release the replacement production capture");
  SetKeys({KeyCode::Return}, {KeyCode::Return});
  Check(Shortcut::ReleaseCapture(replacement), "current production editor releases by its token");
  PollCapture();
  Check(!Shortcut::IsDown(Action::kShowGalaxy) && Shortcut::Owns(KeyCode::Return),
        "accepted popup Enter remains owned after the editor unbinds");
  SetKeys({});
  PollCapture(false);
  Check(Shortcut::CaptureBlocksShortcuts(), "unfocused release cannot return production input");
  PollCapture();
  PollCapture();
  Check(Shortcut::CaptureBlocksShortcuts(), "the final production release frame is still gated");
  PollCapture();
  Check(!Shortcut::CaptureBlocksShortcuts(), "production capture relinquishes a later input frame");
  SetKeys({KeyCode::Return}, {KeyCode::Return});
  Check(Shortcut::IsDown(Action::kShowGalaxy), "a fresh Enter matches normally after capture release");
  SetKeys({});
  Shortcut::Set({});
}

int main()
{
  GuardPolicy();
  HostPolicy();
  std::cout << "PASS shortcut capture policy and production matching/release integration\n";
}
