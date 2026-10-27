#pragma once

// Fixture-only Config boundary. It neither instantiates a worker nor touches any native/profile state.
#include "runtime_reload.h"

#include <cstddef>

namespace runtime_config_fixture
{
void        SetReloadSnapshot(runtime_config::ReloadSnapshot snapshot);
void        SetCaptureFailure(bool fail);
void        Reset();
std::size_t CaptureCount() noexcept;
std::size_t QueueResetCount() noexcept;
} // namespace runtime_config_fixture
