#pragma once

// The one ScreenManager.Update owner. Call only after runtime initialization, from the game/bootstrap thread.
// Registration is independent of anchor readiness; accepting a callback does not promise it will be dispatched.

#include <il2cpp/hook.h>

using ScreenManagerUpdateCallback = void (*)();

bool                      RegisterFrameCallback(ScreenManagerUpdateCallback callback, const char* name) noexcept;
il2cpp::HookInstallResult EnsureFrameTick();
il2cpp::HookInstallResult FrameTickInstallation();
