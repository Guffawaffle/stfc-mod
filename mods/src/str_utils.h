#pragma once

// Re-export pure string utilities (no IL2CPP deps)
#include "str_utils_pure.h"

// Additional dependencies for IL2CPP + platform string conversion
#include <il2cpp/il2cpp_helper.h>

#include <simdutf.h>
#if _WIN32
#include <winrt/base.h>
#endif

inline std::wstring to_wstring(const std::string& str)
{
#if _WIN32
  return winrt::to_hstring(str).c_str();
#else
  size_t                      expected_utf32words = simdutf::utf32_length_from_utf8(str.data(), str.length());
  std::unique_ptr<char32_t[]> utf32_output{new char32_t[expected_utf32words]};
  size_t                      utf16words = simdutf::convert_utf8_to_utf32(str.data(), str.length(), utf32_output.get());
  return std::wstring(utf32_output.get(), utf32_output.get() + utf16words);
#endif
}

inline std::wstring to_wstring(Il2CppString* str)
{
#if _WIN32
  return str->chars;
#else
  size_t                      expected_utf32words = simdutf::utf32_length_from_utf16(str->chars, str->length);
  std::unique_ptr<char32_t[]> utf32_output{new char32_t[expected_utf32words]};
  size_t                      utf16words = simdutf::convert_utf16_to_utf32(str->chars, str->length, utf32_output.get());
  return std::wstring(utf32_output.get(), utf32_output.get() + utf16words);
#endif
}

inline std::string to_string(const std::wstring& str)
{
#if _WIN32
  size_t                     expected_utf16words = simdutf::utf8_length_from_utf16((char16_t*)str.data(), str.length());
  std::unique_ptr<char8_t[]> utf8_output{new char8_t[expected_utf16words]};
  size_t utf16words = simdutf::convert_utf16_to_utf8((char16_t*)str.data(), str.length(), (char*)utf8_output.get());
  return {utf8_output.get(), utf8_output.get() + utf16words};
#else
  size_t                     expected_utf32words = simdutf::utf8_length_from_utf32((char32_t*)str.data(), str.length());
  std::unique_ptr<char8_t[]> utf32_output{new char8_t[expected_utf32words]};
  size_t utf16words = simdutf::convert_utf32_to_utf8((char32_t*)str.data(), str.length(), (char*)utf32_output.get());
  return std::string(utf32_output.get(), utf32_output.get() + utf16words);
#endif
}

inline std::string to_string(Il2CppString* str)
{
  const auto s = to_wstring(str);
  return to_string(s);
}
