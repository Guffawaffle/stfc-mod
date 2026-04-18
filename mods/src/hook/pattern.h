/**
 * @file pattern.h
 * @brief Byte-pattern scanner for finding functions in loaded modules.
 *
 * Replaces spud::find_in_module().  Scans the executable text section of a
 * loaded module for a byte pattern with '?' wildcards.
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#if _WIN32
#include <Windows.h>
#elif __APPLE__
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#endif

namespace pattern {

namespace detail {

struct PatternByte {
  uint8_t value;
  bool    wildcard;
};

inline std::vector<PatternByte> parse(const char* sig)
{
  std::vector<PatternByte> bytes;
  for (const char* p = sig; *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (*p == '?') {
      bytes.push_back({0, true});
      ++p;
      if (*p == '?') ++p;
    } else {
      auto val = static_cast<uint8_t>(strtoul(p, const_cast<char**>(&p), 16));
      bytes.push_back({val, false});
    }
  }
  return bytes;
}

inline void* scan(const uint8_t* base, size_t size, const std::vector<PatternByte>& pat)
{
  if (pat.empty() || size < pat.size()) return nullptr;
  for (size_t i = 0; i <= size - pat.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < pat.size(); ++j) {
      if (!pat[j].wildcard && base[i + j] != pat[j].value) {
        match = false;
        break;
      }
    }
    if (match) return (void*)(base + i);
  }
  return nullptr;
}

} // namespace detail

/// Scan a loaded module for a byte pattern.
/// @param sig          Hex byte string with '?' wildcards, e.g. "48 89 5C 24 ? 57"
/// @param module_name  Name of the loaded module, e.g. "GameAssembly.dll"
/// @return Address of the first match, or nullptr.
inline void* find_in_module(const char* sig, const char* module_name)
{
  auto pat = detail::parse(sig);
  if (pat.empty()) return nullptr;

#if _WIN32
  auto mod = GetModuleHandleA(module_name);
  if (!mod) return nullptr;

  auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
  auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
  auto sec = IMAGE_FIRST_SECTION(nt);

  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    if (std::memcmp(sec[i].Name, ".text", 5) == 0) {
      auto base = reinterpret_cast<uint8_t*>(mod) + sec[i].VirtualAddress;
      return detail::scan(base, sec[i].Misc.VirtualSize, pat);
    }
  }
  return nullptr;

#elif __APPLE__
  uint32_t count = _dyld_image_count();
  for (uint32_t i = 0; i < count; ++i) {
    const char* name = _dyld_get_image_name(i);
    if (!name || !strstr(name, module_name)) continue;

    auto          hdr  = reinterpret_cast<const struct mach_header_64*>(_dyld_get_image_header(i));
    unsigned long size = 0;
    auto          base = getsectiondata(hdr, "__TEXT", "__text", &size);
    if (base && size > 0) return detail::scan(base, size, pat);
  }
  return nullptr;

#else
  return nullptr;
#endif
}

} // namespace pattern
