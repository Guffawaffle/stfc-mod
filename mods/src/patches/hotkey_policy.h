#pragma once

enum class ScopelyShortcutPolicy {
  Off = 0,
  Native,
  Fallback,
};

enum class OriginalFramePolicy {
  Mod = 0,
  FallthroughUnhandled,
  FallthroughAll,
};
