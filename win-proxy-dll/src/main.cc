#include <Windows.h>

#include <filesystem>
#include <vector>
#include <string>

#include "patches/patches.h"

void VersionDllInit();

// Explicit-ID launch isolation is wired into this bootstrap and its one IL2CPP hook.
extern "C" __declspec(dllexport) const unsigned int STFCProfilesExplicitLaunchContractV1 = 1;

namespace {
// /MT needs thread notifications. Keep process-only storage and initialization
// behind a noinline boundary so small-stack game threads return immediately.
__declspec(noinline) BOOL ProcessAttach()
{
  try {
    std::vector<wchar_t> image(32768);
    const auto length = GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    if (!length || length >= image.size()) {
      MessageBoxW(nullptr, L"Could not identify the host executable. The launch has stopped.",
                  L"STFC Profiles: launch stopped", MB_OK | MB_ICONERROR);
      return FALSE;
    }
    const auto game_path = std::filesystem::path(std::wstring(image.data(), length));
    if (CompareStringOrdinal(game_path.filename().c_str(), -1, L"prime.exe", -1, TRUE) != CSTR_EQUAL) {
      return TRUE;
    }
    // Since we are replacing version.dll, need the proper forwards.
    VersionDllInit();
    ApplyPatches();
    return TRUE;
  } catch (...) {
    MessageBoxW(nullptr, L"Could not initialize the game bootstrap. The launch has stopped.",
                L"STFC Profiles: launch stopped", MB_OK | MB_ICONERROR);
    return FALSE;
  }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
  if (reason == DLL_PROCESS_ATTACH) return ProcessAttach();
  return TRUE;
}
void* operator new[](size_t size, const char* /*name*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/,
                     int /*line*/)
{
  return malloc(size);
}

void* operator new[](size_t size, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*name*/, int /*flags*/,
                     unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
  return malloc(size);
}
