#pragma once

// The legacy host supplied these before the pinned native consumer. Keep this
// compile environment local to that source, not in AAI's analytical core or the
// host's global flags. psapi.h in the optional lab requires Windows declarations.
#if _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include <Windows.h>
#endif
