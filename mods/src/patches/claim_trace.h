#pragma once

// Local diagnostic marker, a no-op outside the Windows science probe.
void TraceClaimNavigation(const char* action) noexcept;

// Preserve the claim span around the original handler when the console probe owns the detour.
void TraceClaimGameError(void (*original)(void*, void*), void* handler, void* error);
