#include "profile_launch.h"

#include "stfc_profiles/catalog.h"
#include "stfc_profiles/installation.h"
#include "stfc_profiles/community_mod_adapter.h"
#include "stfc_profiles/prefs_store.h"
#include "stfc_profiles/session.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#if _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif __APPLE__
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <vector>
#endif

namespace profile_launch {
namespace {
std::string requested_id;
std::unique_ptr<stfc::profiles::SessionLease> session;
std::unique_ptr<stfc::profiles::InstallationLease> installation;
std::filesystem::path profile_directory;
bool initialized = false;

void ConsumeArgument(std::string_view argument, std::string_view value)
{
  if (argument != "-stfc-profile")
    return;
  if (!requested_id.empty())
    throw std::runtime_error("Specify -stfc-profile exactly once.");
  if (value.size() != 32 || value.find_first_not_of("0123456789abcdef") != std::string_view::npos)
    throw std::runtime_error("-stfc-profile requires the immutable 32-character profile ID from stfc-profiles list.");
  requested_id = value;
}

#if _WIN32
std::string AsciiArgument(std::wstring_view input)
{
  std::string result;
  result.reserve(input.size());
  for (const auto ch : input) {
    if (ch > 127)
      return {};
    result.push_back(static_cast<char>(ch));
  }
  return result;
}
#endif
} // namespace

[[noreturn]] void Abort(const char* reason)
{
  if (session) {
    try { session->MarkFailed(reason); } catch (...) {}
  }
  std::fprintf(stderr, "STFC Profiles: %s\n", reason);
  if (Requested())
    std::fprintf(stderr, "Use stfc-profiles list, then launch --profile <id>.\n");
  else
    std::fprintf(stderr, "Inspect the installation with stfc-profiles game status --game <directory>.\n");
#if _WIN32
  MessageBoxA(nullptr, reason, "STFC Profiles: launch stopped", MB_OK | MB_ICONERROR);
  TerminateProcess(GetCurrentProcess(), 190);
#endif
  std::abort();
}

void Initialize()
{
  if (initialized)
    return;
  initialized = true;
  try {
#if _WIN32
    int argc{};
    auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
      throw std::runtime_error("Could not read the game launch arguments.");
    try {
      for (int i = 1; i < argc; ++i) {
        if (std::wstring_view(argv[i]) != L"-stfc-profile")
          continue;
        const auto value = i + 1 < argc ? AsciiArgument(argv[i + 1]) : std::string{};
        ConsumeArgument("-stfc-profile", value);
        ++i;
      }
    } catch (...) {
      LocalFree(argv);
      throw;
    }
    LocalFree(argv);
#elif __APPLE__
    const auto argc = *_NSGetArgc();
    auto** argv = *_NSGetArgv();
    for (int i = 1; i < argc; ++i) {
      if (std::string_view(argv[i]) != "-stfc-profile")
        continue;
      ConsumeArgument(argv[i], i + 1 < argc ? std::string_view(argv[i + 1]) : std::string_view{});
      ++i;
    }
#endif
    std::filesystem::path executable;
#if _WIN32
    std::wstring image(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    if (!length || length >= image.size())
      throw std::runtime_error("Could not identify the game installation.");
    image.resize(length);
    executable = image;
#elif __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> image(size);
    if (image.empty() || _NSGetExecutablePath(image.data(), &size) != 0)
      throw std::runtime_error("Could not identify the game installation.");
    executable = std::filesystem::u8path(image.data());
#endif
    const auto root = stfc::profiles::DefaultCatalogRoot();
    const auto game = std::filesystem::canonical(executable).parent_path();
    installation = std::make_unique<stfc::profiles::InstallationLease>(root, game, false);
    stfc::profiles::CheckInstallationReady(root, installation->Directory());
    if (Requested())
      stfc::profiles::community_mod::InstallProcessAdmission(requested_id);
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not prepare the requested profile launch.");
  }
}

bool Requested()
{
  return !requested_id.empty();
}

void Prepare()
{
  if (!Requested())
    return;
  try {
    if (session)
      throw std::runtime_error("The requested profile was prepared more than once.");
    const auto root = stfc::profiles::DefaultCatalogRoot();
    session = std::make_unique<stfc::profiles::SessionLease>(root, requested_id);
    profile_directory = session->Directory();
    const auto initialized_store = std::filesystem::exists(profile_directory / "player_prefs.bin.initialized");
    const auto stored_preferences = std::filesystem::exists(profile_directory / "player_prefs.bin");
    const auto mode = session->PreferencesInitialized() || initialized_store
                         ? stfc::profiles::ProfileOpenMode::Existing
                         : stored_preferences ? stfc::profiles::ProfileOpenMode::Resume
                                              : stfc::profiles::ProfileOpenMode::New;
    stfc::profiles::community_mod::PrepareProfile(root, requested_id, mode, *session);
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not open the requested profile's preferences.");
  }
}

void Install()
{
  if (!Requested())
    return;
  try {
    stfc::profiles::community_mod::InstallProfileHooks();
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not install the requested profile's preference isolation.");
  }
}

const std::filesystem::path& Directory()
{
  if (Requested() && profile_directory.empty())
    Abort("Profile storage was requested before session admission.");
  return profile_directory;
}

} // namespace profile_launch
