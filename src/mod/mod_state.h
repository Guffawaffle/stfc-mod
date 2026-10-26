#pragma once

#include <nlohmann/json.hpp>

#include "mod_state_writer.h"
#include <functional>
#include <optional>

namespace mod_state
{
// Reads the latest supported complete snapshot. Missing, malformed, non-object, and future-version files return no
// value.
std::optional<nlohmann::json> Read();

// Performs a serialized read-modify-write transaction, retrying briefly when another writer holds the state lock.
// Unsupported schema versions are left unchanged and return false.
bool Update(const std::function<void(nlohmann::json&)>& update);

// Performs the same transaction only when the state lock is immediately available.
bool TryUpdate(const std::function<void(nlohmann::json&)>& update);

// Call during feature startup. Resolves the scoped path before UI actions and
// retains one process-lifetime worker; never joins at DLL teardown.
bool PrepareAsync();
// Shared normal-quit owner invokes these; no second Unity quit detour and no main-thread file I/O/join.
void BeginQuitDrain();
bool QuitDrained();
void ResumeAfterQuitVeto();
// Admission is NOT durable success. Receipts name the latest desired value for
// one fixed group; callers must check Status before claiming it was saved.
uint64_t   QueueOfficerOrder(const std::vector<int64_t>& order);
uint64_t   QueueShipPins(const std::vector<int64_t>& ids, const std::vector<std::string>& legacy_names);
uint64_t   QueueConsoleOpacity(int percent);
SaveStatus Status(Group group);
} // namespace mod_state
