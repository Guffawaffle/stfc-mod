#include <doctest/doctest.h>

#include "patches/sync_message_adapter.h"

#include <limits>

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

  TEST_CASE("resource deltas accumulate into an initialized absolute balance")
  {
    AbsoluteResourceState resources;

    CHECK(resources.observe_snapshot(7, 100));
    CHECK_FALSE(resources.observe_snapshot(7, 100));

    const auto first = resources.apply_delta(7, 5);
    CHECK(first.status == ResourceDeltaStatus::Applied);
    CHECK(first.amount == 105);

    const auto repeated = resources.apply_delta(7, 5);
    CHECK(repeated.status == ResourceDeltaStatus::Applied);
    CHECK(repeated.amount == 110);

    const auto debit = resources.apply_delta(7, -20);
    CHECK(debit.status == ResourceDeltaStatus::Applied);
    CHECK(debit.amount == 90);
    CHECK(resources.amount(7) == 90);
  }

  TEST_CASE("resource deltas fail closed without a snapshot or on overflow")
  {
    AbsoluteResourceState resources;

    CHECK(resources.apply_delta(9, 5).status == ResourceDeltaStatus::NoSnapshot);
    CHECK_FALSE(resources.amount(9).has_value());

    REQUIRE(resources.observe_snapshot(9, std::numeric_limits<int64_t>::max()));
    const auto overflow = resources.apply_delta(9, 1);
    CHECK(overflow.status == ResourceDeltaStatus::Overflow);
    CHECK(overflow.amount == std::numeric_limits<int64_t>::max());
    CHECK(resources.amount(9) == std::numeric_limits<int64_t>::max());

    const auto unchanged = resources.apply_delta(9, 0);
    CHECK(unchanged.status == ResourceDeltaStatus::Unchanged);
    CHECK(unchanged.amount == std::numeric_limits<int64_t>::max());
  }
}
