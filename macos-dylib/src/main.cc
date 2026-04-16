/**
 * @file main.cc
 * @brief macOS dylib entry point for the STFC community patch.
 *
 * This shared library is injected into the game process via
 * DYLD_INSERT_LIBRARIES (set by the macOS loader/launcher). The
 * __attribute__((constructor)) function runs before the game's main(),
 * giving us the chance to apply mod hooks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "patches/patches.h"

/**
 * @brief Constructor — runs automatically when the dylib is loaded into the game process.
 *
 * Logs injection to syslog and applies all mod patches.
 */
__attribute__((constructor))
void myconstructor(int argc, const char **argv)
{
  syslog(LOG_ERR, "[+] dylib injected in %s\n", argv[0]);
  ApplyPatches();
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
