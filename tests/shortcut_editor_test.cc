#include "settings/shortcut_capture.h"
#include "settings/shortcut_catalog.h"
#include "settings/shortcut_draft.h"
#include <cstdlib>
#include <iostream>

using namespace mod_settings;
void Check(bool ok, const char* label)
{
  if (!ok) {
    std::cerr << label << '\n';
    std::exit(1);
  }
}
int main()
{
  Check(DescribeShortcut(ShowInventory).group == ShortcutGroup::Interface
            && DescribeShortcut(ShowArtifacts).group == ShortcutGroup::Interface,
        "opening inventory/artifacts belongs to interface, not map travel");
  Check(DescribeShortcut(ToggleAutoConfirmInstantWarp).group == ShortcutGroup::Travel,
        "instant warp shortcut belongs to map and travel");
  Check(DescribeShortcut(Restart).label == "Clear localization cache and reload",
        "cache-clearing shortcut must not promise a plain restart");
  ShortcutList               live{"LCTRL-G", "F8"};
  int                        writes = 0;
  ValueSetting<ShortcutList> owner({"shortcuts.test", "Test",
                                    [&] { return ValueReadResult<ShortcutList>::Known(live, 1); },
                                    [&](ShortcutList value, std::uint64_t) {
                                      ++writes;
                                      live = value;
                                      return ApplyResult::Applied;
                                    }});
  ShortcutDraft              draft(owner);
  draft.Begin();
  draft.Stage(1, "ALT-I");
  Check(writes == 0 && live == ShortcutList{"LCTRL-G", "F8"}, "recording must not write");
  Check(draft.Apply() == Outcome::AppliedVerified && live == ShortcutList{"LCTRL-G", "ALT-I"},
        "replace preserves alternatives and sided modifiers");
  draft.Begin();
  draft.Stage(2, "F9");
  draft.Cancel();
  Check(draft.Apply() == Outcome::Suppressed && writes == 1, "cancel never writes");
  draft.Begin();
  draft.Stage(2, "F9");
  Check(draft.Apply() == Outcome::AppliedVerified && live.size() == 3, "append preserves previous alternatives");
  draft.Begin();
  live = {"F10"};
  draft.Stage(0, "F7");
  Check(draft.Apply() == Outcome::Conflict && live == ShortcutList{"F10"},
        "change during capture cannot be overwritten");
  draft.Begin();
  draft.Stage(0, {});
  Check(draft.Apply() == Outcome::AppliedVerified && live.empty(), "explicit removal can unbind last key");
  draft.Begin();
  draft.Stage(0, "CTRL-I");
  Check(draft.Apply() == Outcome::AppliedVerified && live == ShortcutList{"CTRL-I"}, "unbound action can be rebound");
  const auto beforeDuplicates = writes;
  draft.Begin();
  Check(draft.Stage(1, "CTRL-I") == ShortcutStage::AlreadyBound && !draft.pending(),
        "append must reject an existing binding");
  Check(draft.Apply() == Outcome::Suppressed && writes == beforeDuplicates,
        "duplicate append must not publish or save");
  draft.Begin();
  Check(draft.Stage(0, "CTRL-I") == ShortcutStage::AlreadyBound && !draft.pending(),
        "re-recording the selected binding is a no-op");
  live = {"SHIFT-I", "ALT-I"};
  draft.Begin();
  Check(draft.Stage(1, "F8") == ShortcutStage::Staged && draft.pending(), "stage another alternative");
  Check(draft.Stage(1, "SHIFT-I") == ShortcutStage::AlreadyBound && !draft.pending(),
        "duplicate replacement clears any preceding draft");
  Check(draft.Apply() == Outcome::Suppressed && writes == beforeDuplicates && live == ShortcutList{"SHIFT-I", "ALT-I"},
        "duplicate replacement preserves all alternatives without writing");
  live = {"SHIFT-I", "SHIFT-I", "ALT-I"};
  draft.Begin();
  Check(writes == beforeDuplicates && live.size() == 3, "opening does not clean up existing duplicates");
  Check(draft.Stage(0, {}) == ShortcutStage::Staged && draft.Apply() == Outcome::AppliedVerified
            && live == ShortcutList{"SHIFT-I", "ALT-I"} && writes == beforeDuplicates + 1,
        "explicit removal can clean up one existing duplicate without losing the shortcut");
  ShortcutCapture       capture;
  ShortcutCapture::Keys held{}, down{};
  auto                  modifier = [](KeyCode key) { return key == KeyCode::LeftControl; };
  auto                  tick     = [&] { return capture.Tick(held, down, true, modifier); };
  capture.Begin();
  held[(int)KeyCode::Mouse0] = true;
  down[(int)KeyCode::Mouse0] = true;
  Check(!tick() && !capture.listening(), "opening click cannot be captured");
  held = {};
  down = {};
  tick();
  Check(capture.listening(), "all opening keys must release");
  held[(int)KeyCode::LeftControl] = true;
  down[(int)KeyCode::LeftControl] = true;
  Check(!tick() && capture.listening(), "modifier alone is not a binding");
  down                  = {};
  held[(int)KeyCode::I] = true;
  down[(int)KeyCode::I] = true;
  Check(tick() == KeyCode::I && capture.active(), "record primary but retain input ownership");
  down = {};
  tick();
  Check(capture.active(), "held captured chord cannot leak to gameplay");
  held[(int)KeyCode::I] = false;
  tick();
  Check(capture.active(), "modifier release also required");
  held = {};
  tick();
  Check(!capture.active(), "release ends ownership");
  capture.Begin();
  tick();
  held[(int)KeyCode::Escape] = true;
  down[(int)KeyCode::Escape] = true;
  Check(!tick() && capture.active(), "escape cancels instead of binding");
  down = {};
  held = {};
  tick();
  Check(!capture.active(), "escape release ends cancellation drain");
  capture.Begin();
  tick();
  held[(int)KeyCode::I] = true;
  down[(int)KeyCode::I] = true;
  Check(!capture.Tick(held, down, false, modifier) && capture.active(), "focus loss cancels and drains");
  held = {};
  down = {};
  tick();
  capture.Begin();
  tick();
  held[(int)KeyCode::I] = held[(int)KeyCode::G] = true;
  down                                          = held;
  Check(!tick() && !capture.listening(), "two simultaneous primary keys are ambiguous");
  std::cout << "Shortcut draft and capture fixtures passed\n";
}
