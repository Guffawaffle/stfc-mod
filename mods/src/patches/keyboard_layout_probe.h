#pragma once

namespace keyboard_layout::probe
{
#if defined(_WIN32) && defined(_MODDBG)
// Study branch only. Called once per queried input frame, in layout mode only.
void Tick();
void Cancel();
#else
inline void Tick() {}
inline void Cancel() {}
#endif
}
