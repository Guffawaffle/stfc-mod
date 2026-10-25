#pragma once
#include "runtime_reload.h"
#include "settings/setting_metadata.h"
#include "toml_editor.h"
#include <chrono>
#include <toml++/toml.h>

namespace runtime_config
{
// Startup only: retain the semantic disk value as the optimistic comparison base.
void Configure(const toml::table& loaded);
// Capture before opening the file. Only unacknowledged/session-only owner-thread
// intents are protected; the returned values survive completion during the read.
ReloadSnapshot CaptureReload();
// Game/bootstrap thread after runtime initialization. Required frame delivery
// and quit hooks are distinct from the worker's lazy thread lifetime.
bool Install();
// Register one process-lifetime observer during patch installation. Called on
// the game thread when per-key status changes, never by the worker.
bool SetSaveStatusObserver(void (*observer)());
bool HasSaveFailures() noexcept;
// Definitions register their key once. Repeated registrations preserve the
// original startup comparison value and do not read or write the disk.
bool                     RegisterSetting(const char* section, const char* key) noexcept;
mod_settings::SaveStatus GetSaveStatus(const char* section, const char* key,
                                       const config_edit::Value& current) noexcept;
std::uint64_t            StatusSequence() noexcept;
void                     SaveSetting(const char* section, const char* key, config_edit::Value value,
                                     std::chrono::milliseconds delay = {}) noexcept;
#if _WIN32
void ForceClose() noexcept;
#endif
} // namespace runtime_config
