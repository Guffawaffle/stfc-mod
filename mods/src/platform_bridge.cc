/**
 * @file platform_bridge.cc
 * @brief Platform adapters for command-line and mod file storage paths.
 */
#include "platform_bridge.h"

#if _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include "folder_manager.h"
#endif

#include <string>
#include <string_view>
#include <system_error>

namespace {
#if _WIN32
std::string wide_to_utf8(std::wstring_view value)
{
  if (value.empty()) {
    return {};
  }

  const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
                                               nullptr, nullptr);
  std::string result(static_cast<size_t>(size_needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size_needed, nullptr,
                      nullptr);
  return result;
}
#endif
}

namespace platform_bridge {

CommandLineOptions ReadCommandLineOptions()
{
  CommandLineOptions options;

#if _WIN32
  const auto command_line = GetCommandLineW();

  int argument_count = 0;
  auto arguments = CommandLineToArgvW(command_line, &argument_count);
  if (arguments == nullptr) {
    return options;
  }

  for (int index = 0; index < argument_count; ++index) {
    const auto argument = std::wstring_view(arguments[index]);

    if (argument == L"-debug") {
      options.debug = true;
      continue;
    }

    if (argument == L"-trace") {
      options.trace = true;
      continue;
    }

    if (argument == L"-ccm" && index + 1 < argument_count) {
      options.config_override = std::filesystem::path(wide_to_utf8(arguments[index + 1]));
      break;
    }
  }

  LocalFree(arguments);
#endif

  return options;
}

std::filesystem::path ModStoragePath(std::string_view filename, bool create_dir, bool old_path)
{
#if _WIN32
  (void)create_dir;
  (void)old_path;
  return std::filesystem::path(filename);
#else
  const std::filesystem::path library_path =
      fm::FolderManager::pathForDirectory(fm::NSLibraryDirectory, fm::NSUserDomainMask);
  const auto package_name = old_path ? "com.tashcan.startrekpatch" : "com.stfcmod.startrekpatch";
  const auto config_dir = library_path / "Preferences" / package_name;

  if (create_dir) {
    std::error_code error;
    std::filesystem::create_directories(config_dir, error);
  }

  return config_dir / filename;
#endif
}

std::string PathToUtf8String(const std::filesystem::path& path)
{
#if _WIN32
  return path.string();
#else
  const auto value = path.u8string();
  return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#endif
}

}
