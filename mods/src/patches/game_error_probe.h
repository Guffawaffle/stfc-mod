#pragma once

struct MethodInfo;

// Claim diagnostics share the console probe's detour when it owns this exact handler.
bool GameErrorProbeOwnsHandler(const MethodInfo* method) noexcept;
