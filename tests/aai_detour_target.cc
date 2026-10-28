#include "aai_detour_target.h"

#if _WIN32
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void AaiDetourTarget(AaiDetourProbe* probe)
{
  // A substantive native function, not a wrapper smaller than SPUD's patch.
  volatile unsigned salt = probe->checksum;
  for (unsigned i = 0; i < 32; ++i)
    salt = salt * 31 + i;
  probe->checksum = salt;
  ++probe->calls;
}

// LC_FUNCTION_STARTS bounds a function by the next recorded entry. Keep an
// exported successor; the native test link orders these two symbols explicitly.
extern "C" void AaiDetourTargetEnd() {}
