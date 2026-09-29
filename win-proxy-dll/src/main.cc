#include <Windows.h>

#include <filesystem>
#include <vector>

#include "patches/patches.h"

void VersionDllInit();

// Bridge inspects this export without loading the DLL; launch-time hook failure remains fail-closed.
extern "C" unsigned int STFCModProfileIsolationContractV1()
{
  return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpReserved*/)
{
  std::filesystem::path game_path;

  switch (fdwReason) {
    case DLL_PROCESS_ATTACH: {
      DisableThreadLibraryCalls(hinstDLL);

      std::vector<wchar_t> module_path(MAX_PATH);
      for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
        if (!length) {
          TerminateProcess(GetCurrentProcess(), 190);
          return FALSE;
        }
        if (length < module_path.size()) {
          game_path = std::filesystem::path(std::wstring(module_path.data(), length));
          break;
        }
        if (module_path.size() >= 32768) {
          TerminateProcess(GetCurrentProcess(), 190);
          return FALSE;
        }
        module_path.resize(module_path.size() * 2);
      }

      if (CompareStringOrdinal(game_path.filename().c_str(), -1, L"prime.exe", -1, TRUE) != CSTR_EQUAL) {
        return TRUE;
      }

      // Since we are replacing version.dll, need the proper forwards
      VersionDllInit();
      ApplyPatches();
      break;
    }
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
  }
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
