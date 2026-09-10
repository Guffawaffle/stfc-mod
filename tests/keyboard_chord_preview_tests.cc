#include "patches/keyboard_layout_preview_windows.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace keyboard_layout;
void Check(bool condition, const char* name)
{
  if (!condition)
    throw std::runtime_error(name);
}

#if _WIN32
struct LayoutScope {
  HKL original = GetKeyboardLayout(0);
  HKL existing[256]{};
  int count    = GetKeyboardLayoutList(256, existing);
  HKL german   = nullptr;
  HKL american = nullptr;
  LayoutScope()
  {
    if (count <= 0 || count > 256)
      return;
    for (int i = 0; i < count; ++i) {
      const auto id = reinterpret_cast<uintptr_t>(existing[i]) & 0xffffffffu;
      if (id == 0x04070407u)
        german = existing[i];
      if (id == 0x04090409u)
        american = existing[i];
    }
  }
  ~LayoutScope()
  { ActivateKeyboardLayout(original, 0); }
};
#endif

int main()
{
  try {
#if _WIN32
    const auto original = GetKeyboardLayout(0);
    {
      LayoutScope layouts;
      Check(layouts.count > 0, "test layouts available");
      if (!layouts.german || !layouts.american) {
        std::cout << "SKIP: native fixtures require already-loaded standard US and German layouts; no layouts loaded "
                     "by test\n";
        return 0;
      }
      auto de = [&](char c) { return FindWindowsChordCandidate(c, layouts.german); };
      auto us = [&](char c) { return FindWindowsChordCandidate(c, layouts.american); };
      Check(de('/').physical_key == KeyCode::Alpha7 && de('/').required_modifiers == 1, "German slash");
      Check(de('=').physical_key == KeyCode::Alpha0 && de('=').required_modifiers == 1, "German equals");
      Check(de('`').physical_key == KeyCode::Equals && de('`').required_modifiers == 1, "German grave");
      Check(de('`').base_key_is_dead && de('`').base_label == "\xC2\xB4", "German accent label");
      Check(de('^').physical_key == KeyCode::BackQuote && de('^').required_modifiers == 0, "German caret");
      Check(de('Z').physical_key == KeyCode::Y && de('Z').required_modifiers == 0, "uppercase config Z has no Shift");
      Check(de('@').required_modifiers == 6 && de('@').status == "modifier_policy_required",
            "AltGr not silently inferred");
      Check(PreviewChord(de('@')).suggested_press.empty(), "AltGr has no executable-looking suggestion");
      Check(PreviewChord(de('='), "CTRL").suggested_press == "CTRL+Shift+0", "configured Ctrl retained");
      Check(PreviewChord(de('='), "SHIFT").suggested_press == "SHIFT+0", "Shift is deduplicated");
      Check(PreviewChord(de('='), "LSHIFT").suggested_press == "LSHIFT+0", "side-specific Shift retained");
      Check(us('/').physical_key == KeyCode::Slash && us('/').required_modifiers == 0, "US slash");
      Check(us('=').physical_key == KeyCode::Equals && us('=').required_modifiers == 0, "US equals");
      Check(us('`').physical_key == KeyCode::BackQuote && us('`').required_modifiers == 0, "US grave");
      Check(FindWindowsChordCandidate('/', nullptr).physical_key == KeyCode::None, "missing layout");
      // Switch only this process's test thread. The live game is not involved.
      for (const auto layout : {layouts.american, layouts.german, layouts.american}) {
        Check(ActivateKeyboardLayout(layout, 0) != nullptr, "activate test layout");
        char name[KL_NAMELENGTH]{};
        Check(GetKeyboardLayoutNameA(name), "active layout name");
        const auto candidate = ResolveWindowsChordCandidate('/', name);
        Check(candidate.physical_key == (layout == layouts.german ? KeyCode::Alpha7 : KeyCode::Slash),
              "active-layout lookup follows switch");
        Check(ResolveWindowsChordCandidate('/', "not-the-active-layout").physical_key == KeyCode::None,
              "layout mismatch");
      }
      ActivateKeyboardLayout(layouts.german, 0);
      BYTE       state[256]{};
      wchar_t    output[8]{};
      const auto accent_vk = MapVirtualKeyExW(0x29, MAPVK_VSC_TO_VK_EX, layouts.german);
      Check(ToUnicodeEx(accent_vk, 0x29, state, output, 8, 0, layouts.german) < 0, "seed pending accent");
      for (char symbol : {'/', '=', '`', '^', '@', 'Z'})
        Check(de(symbol).physical_key != KeyCode::None, "candidate with pending accent");
      Check(ToUnicodeEx('A', 0x1e, state, output, 8, 0, layouts.german) == 1 && output[0] == L'\u00e2',
            "candidate lookup preserves accent composition");
      std::cout
          << "layout | configured | physical US position | native modifiers | base label | suggested press | status\n";
      for (const auto layout : {layouts.american, layouts.german}) {
        for (const auto chord : {"/", "=", "CTRL-=", "`", "ALT-^", "@", "Z"}) {
          const std::string text(chord);
          const auto        candidate = FindWindowsChordCandidate(text.back(), layout);
          const auto        preview   = PreviewChord(candidate, text.starts_with("CTRL-")  ? "CTRL"
                                                                : text.starts_with("ALT-") ? "ALT"
                                                                                           : "");
          std::cout << (layout == layouts.german ? "DE" : "US") << " | " << text << " | "
                    << CandidatePhysicalLabel(candidate.physical_key) << " | "
                    << CandidateModifiers(candidate.required_modifiers) << " | " << candidate.base_label << " | "
                    << preview.suggested_press << " | " << preview.status << '\n';
        }
      }
    }
    Check(GetKeyboardLayout(0) == original, "original test-thread layout restored");
#endif
    std::cout << "PASS: chord preview tests; no live dispatch or game changes\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
