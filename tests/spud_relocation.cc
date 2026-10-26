#include "patches/spud_install.h"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Match the gallery constructor's sub/cmp/jne/lea/call prologue. Both the
// RIP-relative cmp and lea expand in the trampoline before the relative call.
// The helper increments a counter, so both branch paths and call-through are
// checked by executing the installed detour, without a game installation.
using Function = int (*)(void*);

int PermanentHook(auto original, void* value)
{ return original(value) + 200; }

auto InstallPermanentOnce(Function target)
{ return SPUD_STATIC_DETOUR(target, PermanentHook); }

auto InstallFailedOnce(Function target)
{ return SPUD_STATIC_DETOUR(target, PermanentHook); }

bool FatalPolicy(spud::detour_error error)
{
#if _WIN32
  char executable[32768]{};
  if (!GetModuleFileNameA(nullptr, executable, sizeof(executable)))
    return false;
  std::string  command = std::string("\"") + executable + "\" --fatal " + std::to_string(int(error));
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessA(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                      &process))
    return false;
  const auto waited = WaitForSingleObject(process.hProcess, 10000);
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 125);
    WaitForSingleObject(process.hProcess, INFINITE);
  }
  DWORD      code = 0;
  const bool read = GetExitCodeProcess(process.hProcess, &code) != FALSE;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return waited == WAIT_OBJECT_0 && read && code == 73;
#else
  const auto child = fork();
  if (child < 0)
    return false;
  if (child == 0) {
    std::signal(SIGABRT, +[](int) { std::_Exit(73); });
    native_hooks::ReportSpudInstall(error, 0, {}, __FILE__, __LINE__, "fatal-policy-fixture");
    std::_Exit(99);
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73;
#endif
}

int main(int argc, char** argv)
{
  if (argc == 3 && std::strcmp(argv[1], "--fatal") == 0) {
    std::signal(SIGABRT, +[](int) { std::_Exit(73); });
    native_hooks::ReportSpudInstall(static_cast<spud::detour_error>(std::atoi(argv[2])), 0, {}, __FILE__, __LINE__,
                                    "fatal-policy-fixture");
    return 99;
  }
  for (const auto error : {spud::detour_error::kProtectionFailed, spud::detour_error::kSuspensionUnavailable,
                           spud::detour_error::kRollbackFailed, static_cast<spud::detour_error>(999)}) {
    if (!native_hooks::SpudInstallRequiresStop(error) || !FatalPolicy(error))
      return 3;
  }
  for (const auto error : {spud::detour_error::kOk, spud::detour_error::kNotFound,
                           spud::detour_error::kInsufficientBytes, spud::detour_error::kUnwindFailed}) {
    if (native_hooks::SpudInstallRequiresStop(error))
      return 4;
  }
  constexpr size_t size = 4096;
#if _WIN32
  auto* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
#else
  auto* code =
      static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0));
  if (code == MAP_FAILED)
    code = nullptr;
#endif
  if (!code)
    return 1;

  const uint8_t body[]   = {0x48, 0x83, 0xec, 0x28,                // sub rsp, 40
                            0x80, 0x3d, 0x75, 0x00, 0x00, 0x00, 0, // cmp byte [code+128], 0
                            0x75, 0x18,                            // jne code+37
                            0x48, 0x8d, 0x0d, 0x70, 0,    0,    0, // lea rcx, [code+132]
                            0xe8, 0x27, 0,    0,    0,             // call code+64
                            0x8b, 0x05, 0x65, 0,    0,    0,       // mov eax, [code+132]
                            0x48, 0x83, 0xc4, 0x28,                // add rsp, 40
                            0xc3, 0x90,                            // ret; nop
                            0x8b, 0x05, 0x59, 0,    0,    0,       // mov eax, [code+132]
                            0x48, 0x83, 0xc4, 0x28, 0xc3};         // add rsp, 40; ret
  const uint8_t helper[] = {0xff, 0x05, 0x3e, 0, 0, 0, 0xc3};      // inc dword [code+132]; ret
  std::memcpy(code, body, sizeof(body));
  std::memcpy(code + 64, helper, sizeof(helper));
#if _WIN32
  FlushInstructionCache(GetCurrentProcess(), code, size);
#else
  __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(code + size));
#endif

  auto function = reinterpret_cast<Function>(code);
  bool passed   = function(nullptr) == 1;
  {
    auto hook = spud::detour<int(Function, void*)>::create(
        function, +[](Function original, void* value) { return original(value) + 100; });
    if (hook.try_install() != spud::detour_error::kOk) {
      std::fprintf(stderr, "SPUD install refused: %.*s\n", int(spud::to_string(hook.last_error()).size()),
                   spud::to_string(hook.last_error()).data());
      return 2;
    }
    passed    = passed && function(nullptr) == 102;
    code[128] = 1;
    passed    = passed && function(nullptr) == 102;
    code[128] = 0;
    passed    = passed && function(nullptr) == 103;
  }
  passed = passed && function(nullptr) == 4;
  // A failed site remains failed; it must not later hook an unrelated target.
  passed               = passed && InstallFailedOnce(nullptr) == nullptr;
  passed               = passed && InstallFailedOnce(function) == nullptr;
  const auto permanent = InstallPermanentOnce(function);
  passed               = passed && permanent != nullptr && function(nullptr) == 205;
  passed               = passed && InstallPermanentOnce(function) == permanent && function(nullptr) == 206;
  native_hooks::ReportSpudSummary();
  // Permanent hooks intentionally live until process exit. Do not release their
  // target page while the installed chain still owns it.
#if _WIN32
  if (!permanent)
    VirtualFree(code, 0, MEM_RELEASE);
#else
  if (!permanent)
    munmap(code, size);
#endif
  std::puts(passed ? "SPUD multi-relocation call/branch regression passed" : "SPUD relocation regression FAILED");
  return passed ? 0 : 1;
}
