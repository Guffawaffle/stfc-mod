/**
 * @file platform_config.h
 * @brief Shared compile-time platform selectors for targeted cross-platform guards.
 */
#pragma once

#if defined(_WIN32)
#define STFCMOD_PLATFORM_WINDOWS 1
#else
#define STFCMOD_PLATFORM_WINDOWS 0
#endif

#if defined(__APPLE__)
#define STFCMOD_PLATFORM_MACOS 1
#else
#define STFCMOD_PLATFORM_MACOS 0
#endif

#if !STFCMOD_PLATFORM_WINDOWS && !STFCMOD_PLATFORM_MACOS
#define STFCMOD_PLATFORM_OTHER 1
#else
#define STFCMOD_PLATFORM_OTHER 0
#endif