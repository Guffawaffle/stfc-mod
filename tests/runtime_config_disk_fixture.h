#pragma once
// Only native frame/quit delivery and the separate state owner are substituted.
// The setting model, runtime adapter, background writer, TOML editor and checked
// filesystem replacement all execute their production implementations.
#include "runtime_config_writer.h"
#include "runtime_reload.h"
#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <string_view>
#if _WIN32
#include <Windows.h>
#endif
namespace spdlog
{
template <class... Args> void warn(const char*, Args&&...) {}
} // namespace spdlog
namespace mod_state
{
inline bool draining = false;
inline void BeginQuitDrain()
{ draining = true; }
inline bool QuitDrained()
{ return true; }
inline void ResumeAfterQuitVeto()
{ draining = false; }
} // namespace mod_state
inline bool install_screen_manager_update_hook()
{ return true; }
inline bool register_screen_manager_update_callback(void (*)())
{ return true; }
