#include <doctest/doctest.h>

#include "patches/sync_message_adapter.h"

TEST_SUITE("sync_message_adapter")
{
  TEST_CASE("all supported client 253 entity groups map to stable sync message kinds")
  {
    CHECK(adapt_entity_group_type(0) == SyncMessageKind::UserProfiles);
    CHECK(adapt_entity_group_type(71) == SyncMessageKind::AllianceProfiles);
    CHECK(adapt_entity_group_type(42) == SyncMessageKind::Json);
    CHECK(adapt_entity_group_type(43) == SyncMessageKind::Officers);
    CHECK(adapt_entity_group_type(46) == SyncMessageKind::PlayerInventories);
    CHECK(adapt_entity_group_type(56) == SyncMessageKind::Jobs);
    CHECK(adapt_entity_group_type(61) == SyncMessageKind::ActiveMissions);
    CHECK(adapt_entity_group_type(64) == SyncMessageKind::CompletedMissions);
    CHECK(adapt_entity_group_type(67) == SyncMessageKind::ResearchTreesState);
    CHECK(adapt_entity_group_type(69) == SyncMessageKind::GlobalActiveBuffs);
    CHECK(adapt_entity_group_type(102) == SyncMessageKind::AllianceGetBankResources);
    CHECK(adapt_entity_group_type(117) == SyncMessageKind::EntitySlots);
    CHECK(adapt_entity_group_type(120) == SyncMessageKind::ActiveOfficerTraits);
    CHECK(adapt_entity_group_type(121) == SyncMessageKind::EntitySlotsData);
    CHECK(adapt_entity_group_type(140) == SyncMessageKind::ForbiddenTechs);
    CHECK(adapt_entity_group_type(160) == SyncMessageKind::AllianceGetGameProperties);
    CHECK(adapt_entity_group_type(219) == SyncMessageKind::BattleResultHeaders);
    CHECK(adapt_entity_group_type(221) == SyncMessageKind::BattleReport);
    CHECK(adapt_entity_group_type(234) == SyncMessageKind::Resources);
    CHECK(adapt_entity_group_type(235) == SyncMessageKind::ResourcesDelta);
    CHECK(adapt_entity_group_type(236) == SyncMessageKind::ResourcesDeltaAlliance);
    CHECK(adapt_entity_group_type(239) == SyncMessageKind::StarbaseModules);
  }

  TEST_CASE("unknown entity groups fail closed")
  { CHECK_FALSE(adapt_entity_group_type(999'999).has_value()); }

  TEST_CASE("slot state is deduplicated across snapshot and realtime sources")
  {
    SlotStateDeduplicator slots;

    CHECK(slots.observe(7, 100, SlotUpdateSource::Snapshot));
    CHECK_FALSE(slots.observe(7, 100, SlotUpdateSource::Realtime));
    CHECK(slots.observe(7, 101, SlotUpdateSource::Realtime));
    CHECK_FALSE(slots.observe(7, 101, SlotUpdateSource::Snapshot));
    CHECK(slots.observe(8, 101, SlotUpdateSource::Snapshot));
    CHECK(slots.size() == 2);
  }
}
