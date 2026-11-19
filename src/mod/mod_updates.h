#pragma once

// Game-thread interface for the downstream release check. Network and cadence state live on the worker.
#include "settings/setting_metadata.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace mod_updates
{
void                     Prepare();
bool                     Available();
bool                     Checking();
void                     CheckNow();
std::string_view         InstalledVersion();
std::string              Status();
bool                     HasRelease();
void                     ViewRelease();
bool                     Ignored();
bool                     IgnoreAvailable();
uint64_t                 ReleaseGeneration();
bool                     SetIgnored(bool value, uint64_t generation);
mod_settings::SaveStatus IgnoreSaveStatus();
} // namespace mod_updates
