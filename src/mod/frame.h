#pragma once

// The one ScreenManager.Update owner. Call only after runtime initialization, from the game/bootstrap thread.
// Registration is independent of anchor readiness; accepting a callback does not promise it will be dispatched.

#include <il2cpp/hook.h>

using ScreenManagerUpdateCallback = void (*)();

bool                      RegisterFrameCallback(ScreenManagerUpdateCallback callback, const char* name) noexcept;
il2cpp::HookInstallResult EnsureFrameTick();
il2cpp::HookInstallResult FrameTickInstallation();
// Physical anchor installation may precede completion of host initialization. No host callbacks, reload or
// shortcuts run until both are ready. Querying this does not trigger installation.
bool FrameDeliveryAvailable() noexcept;
