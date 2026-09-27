#pragma once

#include <cstdint>
#include <optional>

struct FleetPlayerData;

namespace ship_identity
{
bool                   Available();
std::optional<int64_t> InstanceId(FleetPlayerData* fleet);
} // namespace ship_identity
