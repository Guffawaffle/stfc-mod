/**
 * @file windowtitle.h
 * @brief Platform-agnostic interface for reading and setting the game window title.
 *
 * The struct declares Get() and Set(); platform-specific inline implementations
 * live in titlewindows.h (Win32 API), titlelinux.h (X11), and are stubbed on macOS.
 */
#pragma once
#include <string>

/**
 * @brief Platform-dispatched window title accessor.
 *
 * Inline implementations are conditionally included from titlewindows.h
 * (Win32), titlelinux.h (X11).  macOS is currently stubbed.
 */
struct WindowTitle {
  /** @brief Read the current game window title. Returns empty on failure. */
  static std::wstring Get();

  /** @brief Set the game window title. Returns false on failure. */
  static bool Set(const std::wstring& title);
};

#include "titlewindows.h"
#include "titlelinux.h"
