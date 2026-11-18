#pragma once

// Game-thread interface for the downstream release check. Network and cadence state live on the worker.
#include <string>
#include <string_view>

namespace mod_updates
{
void             Prepare();
bool             Available();
bool             Checking();
void             CheckNow();
std::string_view InstalledVersion();
std::string      Status();
bool             HasRelease();
void             ViewRelease();
} // namespace mod_updates
