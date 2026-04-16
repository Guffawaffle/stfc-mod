/**
 * @file main.cc
 * @brief Windows DLL entry point for the STFC community patch.
 *
 * This module is compiled as version.dll and placed in the game directory.
 * Windows loads it as a DLL proxy: the game thinks it is the real version.dll,
 * but DllMain bootstraps the patch (forwarding real version API calls to the
 * system DLL via VersionDllInit, then applying mod hooks via ApplyPatches).
 */

#include <Windows.h>

#include <filesystem>

#include "patches/patches.h"

void VersionDllInit();

/**
 * @brief DLL entry point — bootstraps the community patch on process attach.
 *
 * Only activates when the host executable name starts with "prime" (the STFC
 * game binary). Loads the real system version.dll forwards, then applies all
 * mod patches.
 *
 * @param hinstDLL  Handle to this DLL instance.
 * @param fdwReason Reason the entry point was called (attach/detach).
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpReserved*/)
{
  std::filesystem::path game_path;

  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hinstDLL);

      TCHAR szFileName[MAX_PATH];
      GetModuleFileName(NULL, szFileName, MAX_PATH);

      game_path = szFileName;

      if (!game_path.filename().generic_wstring().starts_with(L"prime")) {
        return TRUE;
      }

      // Since we are replacing version.dll, need the proper forwards
      VersionDllInit();
      ApplyPatches();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}

// ─── EASTL operator new[] Overloads ──────────────────────────────────────────
// EASTL requires custom operator new[] with extended signatures for its
// allocator. These forward to malloc, satisfying the linker.

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
