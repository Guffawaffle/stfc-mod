#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#else
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#endif

namespace sig {

struct match {
  uintptr_t addr = 0;
  uintptr_t address() const { return addr; }
};

struct matches {
  std::vector<match> results;

  match get(size_t index) const { return results[index]; }
  size_t size() const { return results.size(); }
};

namespace detail {

inline void parse_pattern(std::string_view signature, std::string& bytes, std::string& mask)
{
  bytes.clear();
  mask.clear();
  size_t i = 0;
  while (i < signature.size()) {
    if (signature[i] == ' ') {
      ++i;
      continue;
    }
    if (signature[i] == '?') {
      bytes.push_back(0);
      mask.push_back('?');
      ++i;
      if (i < signature.size() && signature[i] == '?')
        ++i;
    } else {
      auto hex = signature.substr(i, 2);
      bytes.push_back(static_cast<char>(std::stoi(std::string(hex), nullptr, 16)));
      mask.push_back('x');
      i += 2;
    }
  }
}

inline matches scan_region(const uint8_t* base, size_t size, std::string_view signature)
{
  std::string bytes, mask;
  parse_pattern(signature, bytes, mask);

  matches result;
  if (mask.empty() || size < mask.size())
    return result;

  for (size_t i = 0; i <= size - mask.size(); ++i) {
    bool found = true;
    for (size_t j = 0; j < mask.size(); ++j) {
      if (mask[j] == '?')
        continue;
      if (base[i + j] != static_cast<uint8_t>(bytes[j])) {
        found = false;
        break;
      }
    }
    if (found) {
      result.results.push_back({reinterpret_cast<uintptr_t>(base + i)});
    }
  }
  return result;
}

} // namespace detail

inline matches find_in_module(std::string_view signature, std::string_view module_name)
{
#if _WIN32
  HMODULE hmod = GetModuleHandleA(std::string(module_name).c_str());
  if (!hmod)
    return {};
  MODULEINFO mi{};
  GetModuleInformation(GetCurrentProcess(), hmod, &mi, sizeof(mi));
  return detail::scan_region(reinterpret_cast<const uint8_t*>(mi.lpBaseOfDll), mi.SizeOfImage, signature);
#else
  // macOS: iterate loaded images to find the matching dylib
  uint32_t count = _dyld_image_count();
  for (uint32_t i = 0; i < count; ++i) {
    const char* name = _dyld_get_image_name(i);
    if (name && std::string_view(name).find(module_name) != std::string_view::npos) {
      const auto* header = reinterpret_cast<const struct mach_header_64*>(_dyld_get_image_header(i));
      unsigned long size = 0;
      const auto* section = getsegmentdata(header, "__TEXT", &size);
      if (section && size > 0) {
        return detail::scan_region(section, size, signature);
      }
    }
  }
  return {};
#endif
}

} // namespace sig
