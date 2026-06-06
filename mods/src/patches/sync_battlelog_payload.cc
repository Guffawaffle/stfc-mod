/**
 * @file sync_battlelog_payload.cc
 * @brief Pure payload builders for public battle-log sync contracts.
 */
#include "patches/sync_battlelog_payload.h"

namespace sync_battlelog_payload
{
namespace
{
  constexpr char kLegacyBattlelogType[] = "battlelog";
}

nlohmann::json BuildLegacyBattlelogSyncPayload(const nlohmann::json& names, const nlohmann::json& journal)
{
  auto payload = nlohmann::json::array();
  payload.push_back({{"type", kLegacyBattlelogType}, {"names", names}, {"journal", journal}});
  return payload;
}
} // namespace sync_battlelog_payload
