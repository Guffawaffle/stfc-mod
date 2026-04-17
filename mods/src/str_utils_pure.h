#pragma once

// Pure string utilities that have ZERO IL2CPP or platform dependencies.
// Split from str_utils.h so the test target can use them without dragging
// in il2cpp_helper.h and the entire game type system.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

inline bool ascii_isspace(unsigned char c)
{
  return std::isspace(static_cast<unsigned char>(c));
}

constexpr std::string_view StripTrailingAsciiWhitespace(const std::string_view str)
{
  const auto it = std::find_if_not(str.rbegin(), str.rend(), ascii_isspace);
  return str.substr(0, static_cast<size_t>(str.rend() - it));
}

constexpr std::string_view StripLeadingAsciiWhitespace(const std::string_view str)
{
  const auto it = std::ranges::find_if_not(str, ascii_isspace);
  return str.substr(static_cast<size_t>(it - str.begin()));
}

constexpr std::string_view StripAsciiWhitespace(const std::string_view str)
{
  return StripTrailingAsciiWhitespace(StripLeadingAsciiWhitespace(str));
}

constexpr std::string AsciiStrToUpper(const std::string_view s)
{
  std::string str = s.data();
  std::ranges::transform(str, str.begin(), ::toupper);
  return str;
}

constexpr std::vector<std::string> StrSplit(const std::string& input, const char delimiter)
{
  std::vector<std::string> result;
  int                      last_pos = 0;
  for (int i = 0; i < input.length(); i++) {
    if (input[i] != delimiter) {
      continue;
    }

    if (i - last_pos > 0) {
      result.emplace_back(input.substr(last_pos, i - last_pos));
    }
    last_pos = i + 1;
  }

  if (last_pos != input.length()) {
    auto sp = input.substr(last_pos, input.length() - last_pos);
    sp      = StripAsciiWhitespace(sp);
    if (!sp.empty()) {
      result.emplace_back(sp);
    }
  }

  return result;
}
