// Explicit profile admission for the full mod. The optional shared component
// owns preference/browser hooks and publishes readiness after every hook is in.

#include "profile_launch.h"

#if STFC_PROFILES_ENABLED
#include <stfc_profiles/catalog.h>
#include <stfc_profiles/community_mod_adapter.h>
#include <stfc_profiles/installation.h>
#include <stfc_profiles/prefs_store.h>
#include <stfc_profiles/session.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <shellapi.h>
#else
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

namespace profile_launch
{
namespace
{
  struct LaunchState {
    std::string           requested_id;
    std::filesystem::path profile_directory;
    bool                  initialized = false;
#if STFC_PROFILES_ENABLED
    std::unique_ptr<stfc::profiles::SessionLease>      session;
    std::unique_ptr<stfc::profiles::InstallationLease> installation;
#endif
  };

  LaunchState& State()
  {
    // macOS enters from a dylib constructor; do not rely on another translation unit's initialization order.
    static LaunchState state;
    return state;
  }

  void ConsumeArgument(std::string_view value)
  {
    auto& requested_id = State().requested_id;
    if (!requested_id.empty())
      throw std::runtime_error("Specify -stfc-profile exactly once.");
    if (value.size() != 32 || value.find_first_not_of("0123456789abcdef") != std::string_view::npos)
      throw std::runtime_error("-stfc-profile requires the immutable 32-character ID from stfc-profiles list.");
    requested_id = value;
  }

  void ParseArguments()
  {
#if _WIN32
    int  argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
      throw std::runtime_error("Could not read the game launch arguments.");
    struct ReleaseArguments {
      LPWSTR* argv;
      ~ReleaseArguments()
      { LocalFree(argv); }
    } release{argv};
    for (int i = 1; i < argc; ++i) {
      if (std::wstring_view(argv[i]) != L"-stfc-profile")
        continue;
      if (++i == argc)
        throw std::runtime_error("-stfc-profile requires an immutable profile ID.");
      std::string value;
      for (const auto ch : std::wstring_view(argv[i])) {
        if (ch > 127)
          throw std::runtime_error("The profile ID must contain only lowercase hexadecimal characters.");
        value.push_back(static_cast<char>(ch));
      }
      ConsumeArgument(value);
    }
#else
    const int argc = *_NSGetArgc();
    char**    argv = *_NSGetArgv();
    for (int i = 1; i < argc; ++i) {
      if (std::string_view(argv[i]) != "-stfc-profile")
        continue;
      if (++i == argc)
        throw std::runtime_error("-stfc-profile requires an immutable profile ID.");
      ConsumeArgument(argv[i]);
    }
#endif
  }

#if STFC_PROFILES_ENABLED
  std::filesystem::path GameDirectory()
  {
#if _WIN32
    std::vector<wchar_t> path(32768);
    const auto           length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
      throw std::runtime_error("Could not identify the game installation.");
    const auto executable = std::filesystem::path(std::wstring(path.data(), length));
#else
    std::vector<char> path(32768);
    auto              size = static_cast<uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) != 0)
      throw std::runtime_error("Could not identify the game installation.");
    const auto executable = std::filesystem::path(path.data());
#endif
    return std::filesystem::canonical(executable).parent_path();
  }
#endif
} // namespace

[[noreturn]] void Abort(const char* reason) noexcept
{
#if STFC_PROFILES_ENABLED
  auto& session = State().session;
  if (session) {
    try {
      session->MarkFailed(reason);
    } catch (...) {
    }
  }
#endif
  std::fprintf(stderr, "STFC Profiles: %s\nUse stfc-profiles list, then launch --profile <id>.\n", reason);
#if _WIN32
  MessageBoxA(nullptr, reason, "STFC Profiles: launch stopped", MB_OK | MB_ICONERROR);
  TerminateProcess(GetCurrentProcess(), 190);
  std::_Exit(190);
#else
  _exit(190);
#endif
}

void Initialize()
{
  auto& state = State();
  if (state.initialized)
    return;
  state.initialized = true;
  try {
    ParseArguments();
#if STFC_PROFILES_ENABLED
    const auto root    = stfc::profiles::DefaultCatalogRoot();
    state.installation = std::make_unique<stfc::profiles::InstallationLease>(root, GameDirectory(), false);
    stfc::profiles::CheckInstallationReady(root, state.installation->Directory());
    if (Requested())
      stfc::profiles::community_mod::InstallProcessAdmission(state.requested_id);
#else
    if (Requested())
      Abort(
          "This mod was built without profile isolation. Install the Profiles test build before selecting a profile.");
#endif
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not prepare the requested profile launch.");
  }
}

bool Requested()
{ return !State().requested_id.empty(); }

void Prepare()
{
#if STFC_PROFILES_ENABLED
  if (!Requested())
    return;
  try {
    auto& state = State();
    if (state.session)
      throw std::runtime_error("The requested profile was prepared more than once.");
    const auto root               = stfc::profiles::DefaultCatalogRoot();
    state.session                 = std::make_unique<stfc::profiles::SessionLease>(root, state.requested_id);
    state.profile_directory       = state.session->Directory();
    const bool initialized_store  = std::filesystem::exists(state.profile_directory / "player_prefs.bin.initialized");
    const bool stored_preferences = std::filesystem::exists(state.profile_directory / "player_prefs.bin");
    const auto mode               = state.session->PreferencesInitialized() || initialized_store
                                        ? stfc::profiles::ProfileOpenMode::Existing
                                    : stored_preferences ? stfc::profiles::ProfileOpenMode::Resume
                                                         : stfc::profiles::ProfileOpenMode::New;
    stfc::profiles::community_mod::PrepareProfile(root, state.requested_id, mode, *state.session);
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not open the requested profile's preferences.");
  }
#endif
}

void Install()
{
#if STFC_PROFILES_ENABLED
  if (!Requested())
    return;
  try {
    stfc::profiles::community_mod::InstallProfileHooks();
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not install the requested profile's preference isolation.");
  }
#endif
}

const std::filesystem::path& Directory()
{
  const auto& directory = State().profile_directory;
  if (Requested() && directory.empty())
    Abort("Profile storage was requested before session admission.");
  return directory;
}
} // namespace profile_launch
