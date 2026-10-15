// Canonical record state and per-target acknowledgement, without threads or HTTP.
#include "patches/parts/sync/sync.h"
#include <cmath>

namespace data_sync
{
namespace
{
  using json = nlohmann::json;
  struct Spec {
    SyncConfig::Type category;
    const char*      type;
    const char*      removed      = nullptr;
    const char*      id           = nullptr;
    bool             initial_full = false, always_full = false;
  };
  constexpr std::array kStreams{
      Spec{SyncConfig::Type::kAwayAssignments, "away_assignment", "collected_away_assignment", "aid", true},
      Spec{SyncConfig::Type::kBattles, "battlelog"},
      Spec{SyncConfig::Type::kBuffs, "buff", "expired_buff", "bid", true},
      Spec{SyncConfig::Type::kBuildings, "module"},
      Spec{SyncConfig::Type::kEmeraldChain, "emerald_chain"},
      Spec{SyncConfig::Type::kHaven, "haven"},
      Spec{SyncConfig::Type::kHaven, "haven_pieces"},
      Spec{SyncConfig::Type::kHaven, "haven_prosperity", nullptr, nullptr, true, true},
      Spec{SyncConfig::Type::kInventory, "inventory", nullptr, nullptr, true},
      Spec{SyncConfig::Type::kJobs, "job", "completed_job", "uuid", true},
      Spec{SyncConfig::Type::kMissions, "active_mission"},
      Spec{SyncConfig::Type::kMissions, "mission"},
      Spec{SyncConfig::Type::kOfficer, "officer"},
      Spec{SyncConfig::Type::kProfile, "profile", nullptr, nullptr, true},
      Spec{SyncConfig::Type::kResearch, "research"},
      Spec{SyncConfig::Type::kResources, "resource", nullptr, nullptr, true},
      Spec{SyncConfig::Type::kResources, "alliance_resource", nullptr, nullptr, true},
      Spec{SyncConfig::Type::kShips, "ship", nullptr, nullptr, true},
      Spec{SyncConfig::Type::kSlots, "slot"},
      Spec{SyncConfig::Type::kStoreOffers, "store_offer"},
      Spec{SyncConfig::Type::kTech, "ft"},
      Spec{SyncConfig::Type::kTraits, "trait"},
  };
  static_assert(kStreams.size() == static_cast<size_t>(Stream::kCount));
} // namespace
nlohmann::json Upload::Records() const
{
  auto result = nlohmann::json::array();
  for (const auto& record : records) {
    if (record->is_array())
      for (const auto& item : *record)
        result.push_back(item);
    else
      result.push_back(*record);
  }
  return result;
}
const char* Upload::Type() const
{ return kStreams[static_cast<size_t>(stream)].type; }
void RecordStore::Reset()
{
  ++generation;
  streams_ = {};
  battle_order_.clear();
}
bool RecordStore::Contains(Stream stream, const json& key) const
{
  const auto& records = streams_[static_cast<size_t>(stream)];
  return records && records->records.contains(key);
}
bool RecordStore::Apply(Stream stream, RecordSet records, Update mode)
{
  auto& slot    = streams_[static_cast<size_t>(stream)];
  bool  changed = !slot;
  if (!slot)
    slot.emplace();
  if (mode != Update::kMerge && !slot->full) {
    slot->full = true;
    changed    = true;
  }
  auto& current = slot->records;
  for (auto& [key, record] : records) {
    const auto type = kStreams[static_cast<size_t>(stream)].type;
    if (record.is_array())
      for (auto& item : record)
        item["type"] = type;
    else
      record["type"] = type;
    const auto old = current.find(key);
    if (old != current.end()) {
      if (record == *old->second)
        continue;
      if (stream == Stream::kShips
          && std::fabs(record.at("level_percentage").get<double>() - old->second->at("level_percentage").get<double>())
                 < 0.01) {
        auto comparable                = record;
        comparable["level_percentage"] = old->second->at("level_percentage");
        if (comparable == *old->second)
          continue;
      }
      // XP rides along but is not part of the key: an XP-only change waits for the next level, Power or Haven change.
      if (stream == Stream::kProfile) {
        auto comparable  = record;
        comparable["xp"] = old->second->at("xp");
        if (comparable == *old->second)
          continue;
      }
    }
    changed = true;
    if (stream == Stream::kBattles && old == current.end())
      battle_order_.push_back(key);
    current.insert_or_assign(key, std::make_shared<const json>(std::move(record)));
  }
  if (mode == Update::kReplace)
    changed |= std::erase_if(current, [&](const auto& entry) { return !records.contains(entry.first); }) != 0;
  // Journals are bounded; other streams contain the current account's entities.
  while (battle_order_.size() > kRecentBattles) {
    changed |= current.erase(battle_order_.front()) != 0;
    battle_order_.pop_front();
  }
  if (changed)
    ++slot->version;
  return changed;
}
void Delivery::Configure(const SyncTargetConfig& config)
{
  if (config == config_)
    return;
  config_ = config;
  ++revision_;
  cursors_ = {};
}
std::optional<Upload> Delivery::Next(const RecordStore& store, Clock::time_point now)
{
  if (generation_ != store.generation) {
    generation_ = store.generation;
    cursors_    = {};
  }
  for (size_t count = 0; count < kStreams.size(); ++count) {
    const size_t i          = (next_ + count) % kStreams.size();
    const auto&  collection = store.streams_[i];
    auto&        cursor     = cursors_[i];
    const auto&  spec       = kStreams[i];
    if (!collection || !config_.enabled(spec.category) || now < cursor.retry_at)
      continue;
    const auto& current   = collection->records;
    const bool  battle    = static_cast<Stream>(i) == Stream::kBattles;
    const bool  uncertain = cursor.retry_at != Clock::time_point{};
    if (battle)
      std::erase_if(cursor.accepted, [&](const auto& entry) { return !current.contains(entry.first); });
    if (!uncertain && (battle ? current == cursor.accepted : collection->version == cursor.version))
      continue;
    Upload upload{static_cast<Stream>(i), store.generation, revision_, collection->version, {}};
    upload.first_sync = spec.always_full || (spec.initial_full && collection->full && !cursor.full);
    bool blocked      = false;
    for (const auto& [key, record] : current) {
      const bool accepted = cursor.accepted.contains(key);
      if (spec.removed && !accepted && !cursor.attempted.contains(key)) {
        // Bound uncertain writes by waiting for acknowledgement before introducing more entities.
        if (cursor.attempted.size() >= 1024) {
          blocked = true;
          continue;
        }
        cursor.attempted.emplace(key, record);
      }
      if (!battle)
        upload.snapshot.emplace(key, record);
      if (upload.first_sync || (uncertain && !battle) || !accepted || cursor.accepted.at(key) != record) {
        upload.records.push_back(record);
        if (upload.stream == Stream::kBattles) {
          upload.snapshot.emplace(key, record);
          break;
        }
      }
    }
    // A timeout may hide a successful server write. Remove records from attempted uploads too.
    if (spec.removed) {
      // Attempted contains only new, unacknowledged keys, so these maps are disjoint.
      for (const auto* previous : {&cursor.accepted, &cursor.attempted})
        for (const auto& [key, record] : *previous)
          if (!current.contains(key))
            upload.records.push_back(
                std::make_shared<const json>(json{{"type", spec.removed}, {spec.id, record->at(spec.id)}}));
    }
    if (blocked) {
      // A partial list cannot claim to be a full baseline or acknowledge the current stream version.
      upload.first_sync = false;
      upload.version    = 0;
    }
    if (upload.records.empty() && !upload.first_sync) {
      cursor.accepted = current;
      cursor.version  = collection->version;
      continue;
    }
    next_ = (i + 1) % kStreams.size();
    return upload;
  }
  return std::nullopt;
}
void Delivery::Complete(Upload upload, bool success, uint64_t generation, Clock::time_point now)
{
  if (upload.generation != generation || upload.revision != revision_)
    return;
  auto& cursor = cursors_[static_cast<size_t>(upload.stream)];
  if (!success) {
    cursor.retry_at = now + cursor.backoff;
    cursor.backoff  = std::min(cursor.backoff * 2, std::chrono::seconds(60));
    return;
  }
  if (upload.stream == Stream::kBattles)
    for (auto& [key, record] : upload.snapshot)
      cursor.accepted.insert_or_assign(key, std::move(record));
  else
    cursor.accepted = std::move(upload.snapshot);
  cursor.full |= upload.first_sync;
  cursor.version = upload.version;
  cursor.attempted.clear();
  cursor.retry_at = {};
  cursor.backoff  = std::chrono::seconds(1);
}
} // namespace data_sync
