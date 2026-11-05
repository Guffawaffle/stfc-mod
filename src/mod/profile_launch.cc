// Host lifecycle port of downstream 45d1deaffddc3706e5c8232cf746ca46ef67be91. The pinned Profiles core still owns
// catalog/session/store semantics. No profile is selected from settings, account state or a last-used preference.
#include "profile_launch.h"

#include "arguments.h"
#include "mod.h"
#include "profile_bootstrap.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if STFC_PROFILES_HOST
#include <stfc_profiles/catalog.h>
#include <stfc_profiles/community_mod_adapter.h>
#include <stfc_profiles/installation.h>
#include <stfc_profiles/prefs_store.h>
#include <stfc_profiles/session.h>
#endif

#if _WIN32
#include <Windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#endif

namespace profile_launch
{
namespace
{
  profile_bootstrap::ProfileBootstrap lifecycle;
  std::string                         selected_id;
  std::filesystem::path               profile_directory;
#if STFC_PROFILES_HOST
  std::filesystem::path                              catalog_root;
  std::unique_ptr<stfc::profiles::SessionLease>      session;
  std::unique_ptr<stfc::profiles::InstallationLease> installation;

  std::filesystem::path ProcessImage()
  {
#if _WIN32
    std::wstring image(32768, L'\0');
    const auto   size = GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    if (!size || size >= image.size())
      throw std::runtime_error("Could not identify the game installation.");
    image.resize(size);
    return image;
#elif __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> image(size);
    if (image.empty() || _NSGetExecutablePath(image.data(), &size) != 0)
      throw std::runtime_error("Could not identify the game installation.");
    return std::filesystem::u8path(image.data());
#endif
  }
#endif
} // namespace

[[noreturn]] void Abort(const char* reason) noexcept
{
#if STFC_PROFILES_HOST
  if (session) {
    try {
      session->MarkFailed(reason);
    } catch (...) {
    }
  }
#endif
  try {
    ReportStartupFailure(reason);
  } catch (...) {
  }
#if _WIN32
  MessageBoxA(nullptr, reason, "STFC Profiles: launch stopped", MB_OK | MB_ICONERROR);
  TerminateProcess(GetCurrentProcess(), 190);
#endif
  std::_Exit(190);
}

bool Requested() noexcept
{ return !selected_id.empty(); }
std::string_view Id() noexcept
{ return selected_id; }

void Initialize() noexcept
{
  try {
    if (lifecycle.State() == profile_bootstrap::Phase::kNotStarted) {
      const auto&                   owned = ProcessArguments();
      std::vector<std::string_view> views(owned.begin(), owned.end());
      selected_id = profile_bootstrap::ParseProfileArguments(views).value_or("");
    }
    lifecycle.Initialize(Requested(), [] {
#if STFC_PROFILES_HOST
      catalog_root    = stfc::profiles::DefaultCatalogRoot();
      const auto game = std::filesystem::canonical(ProcessImage()).parent_path();
      installation    = std::make_unique<stfc::profiles::InstallationLease>(catalog_root, game, false);
      stfc::profiles::CheckInstallationReady(catalog_root, installation->Directory());
      if (Requested())
        stfc::profiles::community_mod::InstallProcessAdmission(selected_id);
#else
      if (Requested())
        throw std::runtime_error("This foundation build has no Profiles host; the explicit profile launch is refused.");
#endif
    });
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not admit this game installation/profile launch.");
  }
}

void Prepare() noexcept
{
  try {
    lifecycle.Prepare([] {
#if STFC_PROFILES_HOST
      session                       = std::make_unique<stfc::profiles::SessionLease>(catalog_root, selected_id);
      profile_directory             = session->Directory();
      const bool initialized_marker = std::filesystem::exists(profile_directory / "player_prefs.bin.initialized");
      const bool stored             = std::filesystem::exists(profile_directory / "player_prefs.bin");
      const auto mode               = session->PreferencesInitialized() || initialized_marker
                                          ? stfc::profiles::ProfileOpenMode::Existing
                                      : stored ? stfc::profiles::ProfileOpenMode::Resume
                                               : stfc::profiles::ProfileOpenMode::New;
      stfc::profiles::community_mod::PrepareProfile(catalog_root, selected_id, mode, *session);
#endif
    });
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not prepare the requested profile's preferences.");
  }
}

bool RuntimeInitialized(bool success) noexcept
{
  try {
    return lifecycle.RuntimeInitialized(success);
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("The game runtime did not initialize for this profile.");
  }
}

void Install() noexcept
{
  try {
    lifecycle.Install([] {
#if STFC_PROFILES_HOST
      stfc::profiles::community_mod::InstallProfileHooks();
#endif
    });
  } catch (const std::exception& error) {
    Abort(error.what());
  } catch (...) {
    Abort("Could not install the requested profile's isolation hooks.");
  }
}

const std::filesystem::path& Directory()
{
  if (Requested() && (lifecycle.Failed() || profile_directory.empty()))
    Abort("Profile storage was requested before successful session preparation.");
  return profile_directory;
}
} // namespace profile_launch
