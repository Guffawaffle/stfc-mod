#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

enum class SyncMessageKind {
  UserProfiles,
  AllianceProfiles,
  Json,
  Officers,
  PlayerInventories,
  Jobs,
  ActiveMissions,
  CompletedMissions,
  ResearchTreesState,
  GlobalActiveBuffs,
  AllianceGetBankResources,
  EntitySlots,
  ActiveOfficerTraits,
  EntitySlotsData,
  ForbiddenTechs,
  AllianceGetGameProperties,
  BattleResultHeaders,
  BattleReport,
  Resources,
  ResourcesDelta,
  ResourcesDeltaAlliance,
  StarbaseModules,
};

[[nodiscard]] std::optional<SyncMessageKind> adapt_entity_group_type(int32_t entity_group_type);

enum class SlotUpdateSource {
  Snapshot,
  Realtime,
};

class SlotStateDeduplicator
{
public:
  [[nodiscard]] bool   observe(int64_t slot_id, int64_t state, SlotUpdateSource source);
  [[nodiscard]] size_t size() const;

private:
  mutable std::mutex                   mutex_;
  std::unordered_map<int64_t, int64_t> states_;
};
