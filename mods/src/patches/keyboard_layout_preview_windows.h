#pragma once

#include "keyboard_layout_preview.h"

#if _WIN32
#include "keyboard_layout_windows.h"

namespace keyboard_layout
{
inline ChordCandidate FindWindowsChordCandidate(char character, HKL layout)
{
  ChordCandidate result;
  if (!layout) {
    result.status = "layout_unavailable";
    return result;
  }
  // Config tokens are uppercased by the parser; letters are action names, not uppercase text requests.
  if (character >= 'A' && character <= 'Z')
    character += 'a' - 'A';
  const auto translated = VkKeyScanExW(static_cast<unsigned char>(character), layout);
  if (translated == -1) {
    result.status = "character_unavailable";
    return result;
  }
  const auto vk             = static_cast<unsigned>(translated) & 0xff;
  result.required_modifiers = (static_cast<unsigned>(translated) >> 8) & 0xff;
  if (result.required_modifiers & ~7u) {
    result.status = "unsupported_modifiers";
    return result;
  }
  result.scan_code = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC_EX, layout);
  for (unsigned index = 0; index < std::size(kWindowsLayoutScans); ++index) {
    if (result.scan_code == kWindowsLayoutScans[index])
      result.physical_key = ToLegacyKey(4 + index);
  }
  if (result.physical_key == KeyCode::None) {
    result.status = "unsupported_physical_key";
    return result;
  }
  const auto base         = MapVirtualKeyExW(vk, MAPVK_VK_TO_CHAR, layout);
  result.base_key_is_dead = (base & 0x80000000u) != 0;
  wchar_t   label         = static_cast<wchar_t>(base & 0xffffu);
  char      utf8[8]{};
  const int length =
      label ? WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &label, 1, utf8, sizeof(utf8), nullptr, nullptr) : 0;
  if (length <= 0) {
    result.status       = "label_unavailable";
    result.physical_key = KeyCode::None;
    return result;
  }
  result.base_label.assign(utf8, length);
  result.status = (result.required_modifiers & 6) ? "modifier_policy_required" : "candidate";
  return result;
}

inline ChordCandidate ResolveWindowsChordCandidate(char character, std::string_view unity_layout)
{
  const auto layout = GetKeyboardLayout(0);
  char       name[KL_NAMELENGTH]{};
  if (!layout || !GetKeyboardLayoutNameA(name) || unity_layout != name) {
    ChordCandidate result;
    result.status = "layout_mismatch";
    return result;
  }
  auto result = FindWindowsChordCandidate(character, layout);
  if (GetKeyboardLayout(0) != layout) {
    result        = {};
    result.status = "layout_changed";
  }
  return result;
}
} // namespace keyboard_layout
#endif
