/**
 * @file str_utils.h
 * @brief String utilities: whitespace stripping, case conversion, splitting,
 *        and cross-platform IL2CPP string conversion.
 *
 * Provides helpers to convert between IL2CPP's Il2CppString (UTF-16 on
 * Windows, UTF-32 on macOS/Linux), std::wstring, and std::string (UTF-8).
 * Uses simdutf for fast SIMD-accelerated encoding conversion on non-Windows
 * platforms; on Windows, delegates to WinRT / WideCharToMultiByte.
 */
#pragma once

// Re-export pure string utilities (no IL2CPP deps)
#include "str_utils_pure.h"

// Additional dependencies for IL2CPP + platform string conversion
#include <il2cpp/il2cpp_helper.h>

#include <simdutf.h>
#if _WIN32
#include <winrt/base.h>
#endif

// ─── IL2CPP / Platform String Conversions ───────────────────────────────────────
// IL2CPP: wchar_t is UTF-16 on Windows, UTF-32 on macOS/Linux.
// These overloads abstract that difference using simdutf on non-Windows.

/** @brief Convert a UTF-8 std::string to std::wstring (platform-aware encoding). */
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

/** @brief Convert an IL2CPP string to std::wstring. */
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

/** @brief Convert std::wstring to UTF-8 std::string. */
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

/** @brief Convert an IL2CPP string to UTF-8 std::string (convenience overload). */
inline std::string to_string(Il2CppString* str)
{
  const auto s = to_wstring(str);
  return to_string(s);
}
