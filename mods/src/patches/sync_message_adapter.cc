#include "patches/sync_message_adapter.h"

std::optional<SyncMessageKind> adapt_entity_group_type(const int32_t entity_group_type)
{
  // Stable EntityGroup.Type protobuf values. This adapter intentionally has no
  // IL2CPP dependency so client-message routing can be covered by pure tests.
  switch (entity_group_type) {
    case 0:
      return SyncMessageKind::UserProfiles;
    case 42:
      return SyncMessageKind::Json;
    case 43:
      return SyncMessageKind::Officers;
    case 46:
      return SyncMessageKind::PlayerInventories;
    case 56:
      return SyncMessageKind::Jobs;
    case 61:
      return SyncMessageKind::ActiveMissions;
    case 64:
      return SyncMessageKind::CompletedMissions;
    case 67:
      return SyncMessageKind::ResearchTreesState;
    case 69:
      return SyncMessageKind::GlobalActiveBuffs;
    case 71:
      return SyncMessageKind::AllianceProfiles;
    case 102:
      return SyncMessageKind::AllianceGetBankResources;
    case 117:
      return SyncMessageKind::EntitySlots;
    case 120:
      return SyncMessageKind::ActiveOfficerTraits;
    case 121:
      return SyncMessageKind::EntitySlotsData;
    case 140:
      return SyncMessageKind::ForbiddenTechs;
    case 160:
      return SyncMessageKind::AllianceGetGameProperties;
    case 219:
      return SyncMessageKind::BattleResultHeaders;
    case 221:
      return SyncMessageKind::BattleReport;
    case 234:
      return SyncMessageKind::Resources;
    case 235:
      return SyncMessageKind::ResourcesDelta;
    case 236:
      return SyncMessageKind::ResourcesDeltaAlliance;
    case 239:
      return SyncMessageKind::StarbaseModules;
    default:
      return std::nullopt;
  }
}

bool SlotStateDeduplicator::observe(const int64_t slot_id, const int64_t state, const SlotUpdateSource source)
{
  static_cast<void>(source);
  std::lock_guard lock(mutex_);
  const auto [it, inserted] = states_.try_emplace(slot_id, state);
  if (inserted) {
    return true;
  }

  if (it->second == state) {
    return false;
  }

  it->second = state;
  return true;
}

size_t SlotStateDeduplicator::size() const
{
  std::lock_guard lock(mutex_);
  return states_.size();
}
