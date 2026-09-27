#pragma once

void ApplyPatches();

#if _WIN32
bool IsolatedProfileRequested();
[[noreturn]] void AbortIsolatedProfileLaunch();
#endif
