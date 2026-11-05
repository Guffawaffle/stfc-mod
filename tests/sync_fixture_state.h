#pragma once

// Included by one fixture TU per executable. Production upload/state/publisher
// code links these owned, synthetic stand-ins instead of game hooks or files.
#include "sync/sync.h"

#include <functional>
#include <stdexcept>

namespace sync_fixture
{
inline data_sync::SharedState              state;
inline std::shared_ptr<const Config::Sync> settings{std::make_shared<const Config::Sync>()};
// Scoped to an explicitly owned fixture thread; never a production callback.
inline thread_local std::function<void()> session_observer, settings_observer;

inline void Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

inline void Fresh()
{
  std::scoped_lock lock(state.mutex);
  Check(state.targets.empty(), "fixture must release prior destinations before reset");
  Check(state.diagnostic_budget.Count() == 0 && state.diagnostic_budget.Bytes() == 0, "dispatch charge leaked");
  state.records.Reset();
  state.generation            = state.records.Generation();
  state.session.generation    = state.records.Generation();
  state.diagnostic_drops      = 0;
  state.diagnostics_ready     = true;
  auto config                 = std::make_shared<Config::Sync>();
  config->options.diagnostics = true;
  config->options.resources   = true;
  std::atomic_store_explicit(&settings, std::shared_ptr<const Config::Sync>{std::move(config)},
                             std::memory_order_release);
}
} // namespace sync_fixture

std::shared_ptr<const Config::Sync> Config::Sync::Get()
{
  auto snapshot = std::atomic_load_explicit(&sync_fixture::settings, std::memory_order_acquire);
  if (sync_fixture::settings_observer)
    sync_fixture::settings_observer();
  return snapshot;
}

namespace data_sync
{
SharedState& State()
{ return sync_fixture::state; }

bool OrdinarySessionOpen(uint64_t generation) noexcept
{
  const bool open = generation == State().generation.load() && State().health.Allows(generation);
  if (sync_fixture::session_observer)
    sync_fixture::session_observer();
  return open;
}

void SealOrdinarySession(uint64_t generation) noexcept
{
  State().health.Seal(generation);
  State().ready.notify_all();
}

#ifndef STFC_SYNC_FIXTURE_BATTLELOGS
std::string NewUuid()
{ return "00000000-0000-4000-8000-000000000001"; }
#endif
} // namespace data_sync
