/**
 * @file sync_battlelog_payload.h
 * @brief Pure payload builders for public battle-log sync contracts.
 */
#pragma once

#include <nlohmann/json.hpp>

namespace sync_battlelog_payload
{
[[nodiscard]] nlohmann::json BuildLegacyBattlelogSyncPayload(const nlohmann::json& names,
                                                             const nlohmann::json& journal);
} // namespace sync_battlelog_payload
