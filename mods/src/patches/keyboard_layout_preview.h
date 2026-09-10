#pragma once

#include <prime/KeyCode.h>
#include <string>
#include <string_view>

namespace keyboard_layout
{
struct ResolvedChord {
  KeyCode key = KeyCode::None;
  bool shift = false;
};

// Native translation used by dispatch and diagnostics. Only status="candidate"
// with no modifiers beyond Shift is executable; Ctrl/Alt recipes remain diagnostic.
struct ChordCandidate {
  KeyCode     physical_key       = KeyCode::None;
  unsigned    required_modifiers = 0; // Windows Shift=1, Ctrl=2, Alt=4; not an executable modifier policy.
  unsigned    scan_code          = 0;
  bool        base_key_is_dead   = false;
  std::string base_label;
  std::string status = "not_queried";
};

inline std::string CandidateModifiers(unsigned mask)
{
  std::string result;
  if (mask & 2)
    result += "Ctrl+";
  if (mask & 4)
    result += "Alt+";
  if (mask & 1)
    result += "Shift+";
  if (!result.empty())
    result.pop_back();
  return result;
}

inline std::string CandidatePhysicalLabel(KeyCode key)
{
  auto code = static_cast<int>(key);
  if (code >= 'a' && code <= 'z')
    code -= 'a' - 'A';
  return code >= 33 && code <= 126 ? std::string(1, static_cast<char>(code)) : std::string{};
}

struct ChordPreview {
  std::string explicit_modifiers;
  std::string required_press;
  std::string suggested_press;
  std::string status;
};

// Use parsed modifier objects, not the position of tokens in the configured chord.
template <class Modifiers> std::string PreviewModifierTokens(const Modifiers& modifiers)
{
  std::string result;
  for (const auto& modifier : modifiers) {
    const auto values = modifier.GetParsedValues();
    if (values.empty())
      continue;
    if (!result.empty())
      result += '-';
    // Generic modifiers accept two sides, represented by repeated configured tokens.
    result += values.substr(0, values.find('-'));
  }
  return result;
}

inline ChordPreview PreviewChord(const ChordCandidate& candidate, std::string_view explicit_modifiers = {})
{
  ChordPreview preview;
  preview.status             = candidate.status;
  preview.explicit_modifiers = explicit_modifiers;
  if (candidate.physical_key == KeyCode::None)
    return preview;
  preview.required_press = CandidateModifiers(candidate.required_modifiers);
  if (!preview.required_press.empty())
    preview.required_press += '+';
  preview.required_press += candidate.base_label;
  if (candidate.required_modifiers & ~1u) {
    preview.status = "modifier_policy_required";
    return preview;
  }
  bool        has_shift = false;
  std::string token;
  for (std::size_t i = 0; i <= preview.explicit_modifiers.size(); ++i) {
    if (i < preview.explicit_modifiers.size() && preview.explicit_modifiers[i] != '-') {
      char c = preview.explicit_modifiers[i];
      token += c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
      continue;
    }
    if (!token.empty()) {
      if (!preview.suggested_press.empty())
        preview.suggested_press += '+';
      preview.suggested_press += token;
      has_shift |= token == "SHIFT" || token == "LSHIFT" || token == "RSHIFT";
      token.clear();
    }
  }
  if ((candidate.required_modifiers & 1) && !has_shift) {
    if (!preview.suggested_press.empty())
      preview.suggested_press += '+';
    preview.suggested_press += "Shift";
  }
  if (!preview.suggested_press.empty())
    preview.suggested_press += '+';
  preview.suggested_press += candidate.base_label;
  return preview;
}
} // namespace keyboard_layout
