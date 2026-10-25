#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "patches/patches.h"

// Inspected as Mach-O data before starting a named profile; never dlopen this
// runtime into the coordinator, because its constructor owns game hooks.
extern "C" __attribute__((visibility("default"), used, section("__DATA,__stfc_profile")))
const unsigned int STFCProfilesExplicitLaunchContractV1 = 1;

__attribute__((constructor))
void myconstructor(int argc, const char **argv)
{
  syslog(LOG_ERR, "[+] dylib injected in %s\n", argv[0]);
  ApplyPatches();
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
