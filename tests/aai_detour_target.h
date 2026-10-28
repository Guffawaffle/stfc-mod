#pragma once

struct AaiDetourProbe {
  unsigned calls{}, hooks{}, checksum{};
};

extern "C" void AaiDetourTarget(AaiDetourProbe* probe);
