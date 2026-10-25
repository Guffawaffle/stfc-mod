#pragma once

// Included by one fixture TU per executable. Production upload/state/publisher
// code links these owned, synthetic stand-ins instead of game hooks or files.
#include "sync/sync.h"

#include <stdexcept>

namespace sync_fixture
{
inline data_sync::SharedState                           state;
inline std::atomic<std::shared_ptr<const Config::Sync>> settings{std::make_shared<const Config::Sync>()};

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
  settings.store(std::move(config));
}
} // namespace sync_fixture

std::shared_ptr<const Config::Sync> Config::Sync::Get()
{ return sync_fixture::settings.load(); }

namespace data_sync
{
SharedState& State()
{ return sync_fixture::state; }

bool OrdinarySessionOpen(uint64_t generation) noexcept
{ return generation == State().generation.load() && State().health.Allows(generation); }

void SealOrdinarySession(uint64_t generation) noexcept
{
  State().health.Seal(generation);
  State().ready.notify_all();
}

std::string NewUuid()
{ return "00000000-0000-4000-8000-000000000001"; }
} // namespace data_sync
