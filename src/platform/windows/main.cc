// The Windows entry point: the game loads this as version.dll from its folder.

#include <Windows.h>

#include <string>
#include <string_view>

#include "mod.h"
#include "profile_bootstrap.h"

#if STFC_PROFILES_HOST
// This data marker is exported only by the build that links the required profile host lifecycle and adapters.
extern "C" __declspec(dllexport) const unsigned int STFCProfilesExplicitLaunchContractV1 = 1;
#endif

namespace
{
// /MT needs loader thread notifications. Keep all process-only storage behind a noinline boundary so small-stack
// threads return directly from DllMain without reserving a process-initialization frame.
__declspec(noinline) BOOL ProcessAttach()
{
  try {
    std::wstring image(32768, L'\0');
    const auto   size = GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    if (!size || size >= image.size()) {
      ReportStartupFailure("could not identify the host executable; refusing initialization");
      return FALSE;
    }
    image.resize(size);
    if (profile_bootstrap::IsPrimeExecutable(image))
      StartMod();
    return TRUE;
  } catch (...) {
    try {
      ReportStartupFailure("could not initialize the native bootstrap");
    } catch (...) {
    }
    return FALSE;
  }
}
} // namespace

// Before the mod's log exists, failures go to the debugger output (DebugView shows it without a debugger): the
// game has no console, so its stderr goes nowhere.
void ReportStartupFailure(const char* message)
{ OutputDebugStringA((std::string("stfc-community-mod: ") + message + "\n").c_str()); }

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
  if (reason == DLL_PROCESS_ATTACH)
    return ProcessAttach();
  return TRUE;
}
