#pragma once

// Local diagnostic marker. No claim or navigation action is performed.
void TraceClaimNavigation(const char* action) noexcept;

// Observe the cloud-load detour already owned by native Mod Settings. This
// optional science observer must never install a second hook on that method.
void TraceBulkSelectionCloudLoad(void* preferences, bool before) noexcept;
