#include "arguments.h"

#include <stdexcept>
#include <utility>

#if _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif __APPLE__
#include <crt_externs.h>
#endif

const std::vector<std::string>& ProcessArguments()
{
  static const auto arguments = [] {
    std::vector<std::string> result;
#if _WIN32
    int    count  = 0;
    auto** values = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!values)
      throw std::runtime_error("Could not read process launch arguments.");
    struct ArgvOwner {
      wchar_t** values;
      ~ArgvOwner()
      { LocalFree(values); }
    } owner{values};
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
      const auto bytes =
          WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[i], -1, nullptr, 0, nullptr, nullptr);
      if (bytes <= 0)
        throw std::runtime_error("Could not decode process launch arguments.");
      std::string value(static_cast<std::size_t>(bytes), '\0');
      if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[i], -1, value.data(), bytes, nullptr, nullptr)
          != bytes)
        throw std::runtime_error("Could not decode process launch arguments.");
      value.pop_back();
      result.push_back(std::move(value));
    }
#elif __APPLE__
    const auto count  = *_NSGetArgc();
    auto**     values = *_NSGetArgv();
    result.assign(values, values + count);
#endif
    return result;
  }();
  return arguments;
}
