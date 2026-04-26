/**
 * @file sync.cc
 * @brief Data synchronization engine — external HTTP sync of game state.
 *
 * Intercepts the game's protobuf and JSON entity-group processing to extract
 * player data (inventories, officers, research, ships, battle logs, etc.) and
 * forward it to user-configured external sync targets over HTTP.
 *
 * Architecture:
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │ Game hooks (DataContainer::ParseBinaryObject, etc.)         │
 *  │   → HandleEntityGroup() dispatches by EntityGroup::Type     │
 *  │       → process_* functions parse protobuf/JSON             │
 *  │           → queue_data() enqueues to sync_data_queue        │
 *  │               → ship_sync_data() thread dequeues & sends    │
 *  │                   → http::send_data() → per-target workers  │
 *  └─────────────────────────────────────────────────────────────┘
 *
 * Battle logs follow a separate pipeline:
 *  process_battle_headers() → combat_log_data_queue
 *    → ship_combat_log_data() thread fetches full journal from Scopely
 *    → resolves player/alliance names via cache or API
 *    → sends enriched battle data via http::send_data()
 *
 * Threading model:
 *  - ship_sync_data:       long-lived consumer for the main sync queue
 *  - ship_combat_log_data: long-lived consumer for combat log enrichment
 *  - target_worker_thread: one per sync target (created lazily), owns its
 *                          own cpr::Session and request queue
 *  - process_* functions:  each invoked on a detached std::thread from
 *                          HandleEntityGroup's submit_async lambda
 *
 * Each process_* function maintains its own static state map (with mutex)
 * and only emits data when values actually change (delta-based sync).
 *
 * Config keys (sync_targets[name]):
 *  - url, token, proxy, verify_ssl: per-target connection settings
 *  - enabled types: battlelogs, resources, ships, buildings, inventory, etc.
 * Config keys (sync_options):
 *  - proxy, verify_ssl: Scopely API proxy for combat log enrichment
 *  - sync_resolver_cache_ttl: TTL for player/alliance name caches
 *  - sync_logging, sync_debug: log verbosity toggles
 */

#include "config.h"
#include "errormsg.h"
#include "str_utils.h"
#include "patches/sync_battle_logs.h"
#include "patches/sync_scheduler.h"
#include "patches/sync_transport.h"

#include <il2cpp-api-types.h>
#include <Digit.PrimeServer.Models.pb.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/ServiceResponse.h>
#include <prime/RealtimeDataPayload.h>
#include <spud/detour.h>

#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef STR_FORMAT
#if __cpp_lib_format
#define STR_FORMAT std::format
#else
#define STR_FORMAT fmt::format
#endif
#endif

#ifndef _WIN32
#include <time.h>
#endif

// ─── JSON Serialization for Protobuf Types ─────────────────────────────────

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T> struct adl_serializer<google::protobuf::RepeatedField<T>> {
  static void to_json(json& j, const google::protobuf::RepeatedField<T>& proto)
  {
    j = json::array();

    for (const auto& v : proto) {
      j.push_back(v);
    }
  }

  static void from_json(const json& j, google::protobuf::RepeatedField<T>& proto)
  {
    if (j.is_array()) {
      for (const auto& v : j) {
        proto.Add(v.get<T>());
      }
    }
  }
};

template <> struct adl_serializer<SyncConfig::Type> {
  static void to_json(json& j, const SyncConfig::Type t)
  {
    for (const auto& opt : SyncOptions) {
      if (opt.type == t) {
        j = opt.type_str;
        return;
      }
    }

    j = nullptr;
  }

  static void from_json(const json& j, SyncConfig::Type& t)
  {
    if (j.is_string()) {
      const auto& s = j.get_ref<const std::string&>();

      for (const auto& opt : SyncOptions) {
        if (opt.type_str == s) {
          t = opt.type;
          return;
        }
      }
    }
  }
};
NLOHMANN_JSON_NAMESPACE_END

// ─── Delta-Tracking State Types ─────────────────────────────────────────────

/// Tracks officer/tech rank+level for delta detection.
struct RankLevelState {
  explicit RankLevelState(const int32_t r = -1, const int32_t l = -1)
      : rank(r)
      , level(l)
  {
  }

  bool operator==(const RankLevelState& other) const
  {
    return this->rank == other.rank && this->level == other.level;
  }

private:
  int64_t rank  = -1;
  int64_t level = -1;
};

/// Tracks officer/tech rank+level+shards for delta detection.
struct RankLevelShardsState {
  explicit RankLevelShardsState(const int32_t r = -1, const int32_t l = -1, const int32_t s = -1)
      : rank(r)
      , level(l)
      , shards(s)
  {
  }

  bool operator==(const RankLevelShardsState& other) const
  {
    return this->rank == other.rank && this->level == other.level && this->shards == other.shards;
  }

private:
  int32_t rank   = -1;
  int32_t level  = -1;
  int32_t shards = -1;
};

/// Tracks ship tier/level/components for delta detection.
struct ShipState {
  explicit ShipState(const int32_t t = -1, const int32_t l = -1, const double_t lp = -1.0,
                     const std::vector<int64_t>& c = {})
      : tier(t)
      , level(l)
      , level_percentage(lp)
      , components(c)
  {
  }

  bool operator==(const ShipState& other) const
  {
    return this->tier == other.tier && this->level == other.level
           && std::fabs(this->level_percentage - other.level_percentage) < 0.01 && this->components == other.components;
  }

private:
  int32_t              tier             = -1;
  int32_t              level            = -1;
  double_t             level_percentage = -1.0;
  std::vector<int64_t> components       = {};
};

struct pairhash {
  template <typename T, typename U> std::size_t operator()(const std::pair<T, U>& x) const
  {
    return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
  }
};

// ─── Protobuf Entity Processors ─────────────────────────────────────────────
//
// Each process_* function below:
//  1. Parses a protobuf or JSON payload from a unique_ptr<string>
//  2. Compares against a static local state map (with its own mutex)
//  3. Emits only changed entries via queue_data()
//
// This delta-based approach avoids flooding sync targets with redundant data.
// ─────────────────────────────────────────────────────────────────────────

/** @brief Processes ActiveMissionsResponse — emits current set of active mission IDs on change. */
void process_active_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<int64_t> active_mission_states;
  static std::mutex                  active_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ActiveMissionsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "active missions",
                         STR_FORMAT("Processing {} active missions", response.activemissions_size()));

    std::unordered_set<int64_t> active_missions;
    for (const auto& mission : response.activemissions()) {
      active_missions.insert(mission.id());
    }

    bool changed = false;
    {
      std::lock_guard lk(active_mission_states_mtx);
      if (active_mission_states != active_missions) {
        changed               = true;
        active_mission_states = std::move(active_missions);
      }
    }

    if (changed && !active_mission_states.empty()) {
      auto mission_array = json::array();

      for (const auto mission : active_mission_states) {
        mission_array.push_back({{"type", "active_" + SyncConfig::Type::Missions}, {"mid", mission}});
      }

      queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse active missions");
  }
}

/** @brief Processes CompletedMissionsResponse — emits newly completed mission IDs (append-only diff). */
void process_completed_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::vector<int64_t> completed_mission_states;
  static std::mutex           completed_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::CompletedMissionsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "completed missions",
                         STR_FORMAT("Processing {} completed missions", response.completedmissions_size()));

    const auto&          missions = response.completedmissions();
    std::vector<int64_t> completed_missions{missions.begin(), missions.end()};
    std::vector<int64_t> diff;

    // Assume the completed missions list is append-only: new entries may be added, but existing ones are never removed.
    {
      std::lock_guard lk(completed_mission_states_mtx);
      std::ranges::set_difference(completed_missions, completed_mission_states, std::back_inserter(diff));

      if (!diff.empty()) {
        completed_mission_states = std::move(completed_missions);
      }
    }

    if (!diff.empty()) {
      auto mission_array = json::array();

      for (const auto mission : diff) {
        mission_array.push_back({{"type", SyncConfig::Type::Missions}, {"mid", mission}});
      }

      queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse completed missions");
  }
}

/** @brief Processes InventoryResponse — emits items whose count changed. Marks first sync. */
void process_player_inventories(std::unique_ptr<std::string>&& bytes)
{
  using json   = nlohmann::json;
  using item_t = std::underlying_type_t<Digit::PrimeServer::Models::InventoryItemType>;
  static std::unordered_map<std::pair<item_t, int64_t>, int64_t, pairhash> inventory_states;
  static std::mutex                                                        inventory_states_mtx;
  static std::atomic_bool                                                  is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::InventoryResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "player inventories",
                         STR_FORMAT("Processing {} inventories", response.inventories_size()));

    auto inventory_items = json::array();
    {
      std::lock_guard lk(inventory_states_mtx);

      for (const auto& inventory : response.inventories() | std::views::values) {
        for (const auto& item : inventory.items()) {
          if (item.has_commonparams()) {
            const auto item_id = item.commonparams().refid();
            const auto count   = item.count();
            const auto key     = std::make_pair(item.type(), item_id);

            if (const auto& it = inventory_states.find(key); it == inventory_states.end() || it->second != count) {
              inventory_states[key] = count;
              inventory_items.push_back({{"type", SyncConfig::Type::Inventory},
                                         {"item_type", item.type()},
                                         {"refid", item_id},
                                         {"count", count}});
            }
          }
        }
      }
    }

    if (!inventory_items.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Inventory, inventory_items, first_sync);
    }
  } else {
    spdlog::error("Failed to parse player inventories");
  }
}

/** @brief Processes ResearchTreesState — emits research projects whose level changed. */
void process_research_trees_state(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> research_states;
  static std::mutex                           research_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ResearchTreesState(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "research trees state",
                         STR_FORMAT("Processing {} research projects", response.researchprojectlevels_size()));

    auto research_array = json::array();
    {
      std::lock_guard lk(research_states_mtx);

      for (const auto& [id, level] : response.researchprojectlevels()) {
        if (const auto& it = research_states.find(id); it == research_states.end() || it->second != level) {
          research_states[id] = level;
          research_array.push_back({{"type", SyncConfig::Type::Research}, {"rid", id}, {"level", level}});
        }
      }
    }

    if (!research_array.empty()) {
      queue_data(SyncConfig::Type::Research, research_array);
    }
  } else {
    spdlog::error("Failed to parse research trees state");
  }
}

/** @brief Processes OfficersResponse — emits officers whose rank/level/shards changed. */
void process_officers(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> officer_states;
  static std::mutex                                         officer_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficersResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "officers", STR_FORMAT("Processing {} officers", response.officers_size()));

    auto officers_array = json::array();
    {
      std::lock_guard lk(officer_states_mtx);

      for (const auto& officer : response.officers()) {
        const RankLevelShardsState officer_state{officer.rankindex(), officer.level(), officer.shardcount()};

        if (const auto& it = officer_states.find(officer.id());
            it == officer_states.end() || it->second != officer_state) {
          officer_states[officer.id()] = officer_state;
          officers_array.push_back({{"type", SyncConfig::Type::Officer},
                                    {"oid", officer.id()},
                                    {"rank", officer.rankindex()},
                                    {"level", officer.level()},
                                    {"shard_count", officer.shardcount()}});
        }
      }
    }

    if (!officers_array.empty()) {
      queue_data(SyncConfig::Type::Officer, officers_array);
    }
  } else {
    spdlog::error("Failed to parse officers");
  }
}

/** @brief Processes ForbiddenTechsResponse — emits forbidden/chaos techs whose tier/level/shards changed. */
void process_forbidden_techs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> tech_states;
  static std::mutex                                         tech_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ForbiddenTechsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "techs",
                         STR_FORMAT("Processing {} forbidden/chaos techs", response.forbiddentechs_size()));

    auto tech_array = json::array();
    {
      std::lock_guard lk(tech_states_mtx);

      for (const auto& tech : response.forbiddentechs()) {
        const RankLevelShardsState tech_state{tech.tier(), tech.level(), tech.shardcount()};

        if (const auto& it = tech_states.find(tech.id()); it == tech_states.end() || it->second != tech_state) {
          tech_states[tech.id()] = tech_state;
          tech_array.push_back({{"type", SyncConfig::Type::Tech},
                                {"fid", tech.id()},
                                {"tier", tech.tier()},
                                {"level", tech.level()},
                                {"shard_count", tech.shardcount()}});
        }
      }
    }

    if (!tech_array.empty()) {
      queue_data(SyncConfig::Type::Tech, tech_array);
    }
  } else {
    spdlog::error("Failed to parse forbidden techs");
  }
}

/** @brief Processes OfficerTraitsResponse — emits officer trait level changes. */
void process_active_officer_traits(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<std::pair<int64_t, int64_t>, int32_t, pairhash> trait_states;
  static std::mutex                                                         trait_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficerTraitsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "active officer traits",
                         STR_FORMAT("Processing {} active officer traits", response.activeofficertraits_size()));

    auto trait_array = json::array();
    {
      std::lock_guard lk(trait_states_mtx);

      for (const auto& [officer_id, officer_traits] : response.activeofficertraits()) {
        for (const auto& trait : officer_traits.activetraits() | std::views::values) {
          const auto& key = std::make_pair(officer_id, trait.traitid());

          if (const auto& it = trait_states.find(key); it == trait_states.end() || it->second != trait.level()) {
            trait_states[key] = trait.level();
            trait_array.push_back({{"type", SyncConfig::Type::Traits},
                                   {"oid", officer_id},
                                   {"tid", trait.traitid()},
                                   {"level", trait.level()}});
          }
        }
      }
    }

    if (!trait_array.empty()) {
      queue_data(SyncConfig::Type::Traits, trait_array);
    }
  } else {
    spdlog::error("Failed to parse active officer traits");
  }
}

/** @brief Processes GlobalActiveBuffsResponse — emits buff add/update/expire events. */
void process_global_active_buffs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, std::pair<int32_t, int64_t>> buff_states;
  static std::mutex                                               buff_states_mtx;
  static std::atomic_bool                                         is_first_sync{true};


  if (auto response = Digit::PrimeServer::Models::GlobalActiveBuffsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "global buffs",
                         STR_FORMAT("Processing {} active buffs", response.globalactivebuffs_size()));

    auto buff_array = json::array();
    {
      std::lock_guard lk(buff_states_mtx);

      // Track all buff ids present in the response to detect removals.
      std::unordered_set<int64_t> present_ids;
      present_ids.reserve(static_cast<size_t>(response.globalactivebuffs_size()));

      for (const auto& buff : response.globalactivebuffs()) {
        present_ids.insert(buff.buffid());
        const bool expires = buff.has_activebuff() && buff.activebuff().has_expirytime();
        const auto state   = std::make_pair(buff.level(), expires ? buff.activebuff().expirytime().seconds() : -1);

        if (const auto& it = buff_states.find(buff.buffid()); it == buff_states.end() || it->second != state) {
          buff_states[buff.buffid()] = state;
          buff_array.push_back({{"type", SyncConfig::Type::Buffs},
                                {"bid", buff.buffid()},
                                {"level", state.first},
                                {"expiry_time", expires ? json(state.second) : json(nullptr)}});
        }
      }

      // Remove buffs that are no longer present and record each removal.
      for (auto it = buff_states.begin(); it != buff_states.end(); ) {
        if (!present_ids.contains(it->first)) {
          buff_array.push_back({
            {"type", "expired_" + SyncConfig::Type::Buffs},
            {"bid", it->first},
          });
          it = buff_states.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!buff_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Buffs, buff_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse global active buffs");
  }
}

static std::unordered_map<int64_t, int64_t> slot_states;
static std::mutex                           slot_states_mtx;

inline std::optional<std::chrono::time_point<std::chrono::system_clock>> parse_timestamp(const std::string& timestamp)
{
#ifdef _WIN32
  std::istringstream ss(timestamp);
  std::chrono::system_clock::time_point time_point;

  if (!std::chrono::from_stream(ss, "%Y-%m-%dT%H:%M:%S", time_point)) {
    spdlog::error("Failed to parse timestamp: {}", timestamp);
    return std::nullopt;
  }

  return time_point;
#else
  std::tm tm = {};
  if (strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) == nullptr) {
    spdlog::error("Failed to parse timestamp: {}", timestamp);
    return std::nullopt;
  }

  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
#endif
}

/** @brief Processes EntitySlots (protobuf) - emits slot changes for consumables, presets, skills, etc. */
void process_entity_slots(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlots(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "entity slots", STR_FORMAT("Processing {} slots", response.entityslots__size()));

    auto slot_array = json::array();
    {
      std::lock_guard lk(slot_states_mtx);

      for (const auto& slot : response.entityslots_()) {
        json    slot_params;
        int64_t state_value = slot.has_slotitemid() ? slot.slotitemid().value() : -1;

        switch (slot.slottype()) {
          case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
            if (slot.has_consumableslotparams()) {
              const auto& consumable = slot.consumableslotparams();
              slot_params["expiry_time"] =
                  consumable.has_expirytime() ? json(consumable.expirytime().seconds()) : json(nullptr);
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
            if (slot.has_officerpresetslotparams()) {
              const auto& preset = slot.officerpresetslotparams();
              slot_params = {{"name", preset.name()}, {"order", preset.order()}, {"officer_ids", preset.officerids()}};
              state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
            if (slot.has_fleetcommanderslotparams()) {
              slot_params["order"] = slot.fleetcommanderslotparams().order();
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
            if (slot.has_selectableskillslotparams()) {
              const auto& skill = slot.selectableskillslotparams();
              slot_params["cooldown_expiration"] =
                  skill.has_cooldownexpiration() ? json(skill.cooldownexpiration().seconds()) : json(nullptr);
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
            if (slot.has_fleetpresetslotparams()) {
              const auto& preset     = slot.fleetpresetslotparams();
              auto        setup_json = json::array();

              for (const auto& setup : preset.setups()) {
                setup_json.push_back({{"drydock_id", setup.drydockid()},
                                      {"ship_id", setup.shipids()[0]},
                                      {"officer_ids", setup.officerids()}});
              }

              slot_params = {{"name", preset.name()}, {"order", preset.order()}, {"setup", setup_json}};
              state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
            }
          default:
            continue;
        }

        if (const auto& it = slot_states.find(slot.id()); it == slot_states.end() || it->second != state_value) {
          slot_states[slot.id()] = state_value;
          slot_array.push_back({{"type", SyncConfig::Type::Slots},
                                {"sid", slot.id()},
                                {"slot_type", slot.slottype()},
                                {"spec_id", slot.slotspecid()},
                                {"item_id", slot.has_slotitemid() ? json(slot.slotitemid().value()) : json(nullptr)},
                                {"params", slot_params}});
        }
      }
    }

    if (!slot_array.empty()) {
      queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots");
  }
}

/** @brief Processes entity slot updates from real-time (RTC) JSON payloads. */
void process_entity_slots_rtc(std::unique_ptr<std::string>&& json_payload)
{
  using json = nlohmann::json;

  try {
    auto data = json::parse(*json_payload);
    http::sync_log_trace("PROCESS", "entity slots (RTC)", "Processing entity slot update");

    const auto item = data["item_id"];
    const auto item_id = item.is_null() ? -1 : item.get<int64_t>();

    json    slot_params;
    int64_t state_value = item_id;

    const auto type = data["slot_type"].get<int32_t>();
    switch (type) {
      case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
        if (const auto& expiry_time = data["consumable_slot_params"]["expiry_time"]; expiry_time.is_null()) {
          slot_params["expiry_time"] = nullptr;
        } else {
          const auto timestamp = parse_timestamp(data["consumable_slot_params"]["expiry_time"].get_ref<const std::string&>());
          if (timestamp.has_value()) {
            slot_params["expiry_time"] = timestamp.value().time_since_epoch().count();
          } else {
            slot_params["expiry_time"] = nullptr;
          }
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
        if (const auto& preset = data["officer_preset_slot_params"]; !preset.is_null()) {
          slot_params = preset;
          state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
        if (const auto& params = data["fleet_commander_slot_params"]; !params.is_null()) {
          slot_params["order"] = params["order"];
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
        if (const auto& params = data["selectable_skill_slot_params"]; !params.is_null()) {
          if (const auto& expiry_time = params["cooldown_expiration"]; expiry_time.is_null()) {
            slot_params["cooldown_expiration"] = nullptr;
          } else {
            const auto timestamp = parse_timestamp(params["cooldown_expiration"].get_ref<const std::string&>());
            if (timestamp.has_value()) {
              slot_params["cooldown_expiration"] = timestamp.value().time_since_epoch().count();
            } else {
              slot_params["cooldown_expiration"] = nullptr;
            }
          }
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
        if (const auto& params = data["fleet_preset_slot_params"]; !params.is_null()) {
          auto setup_json = json::array();
          for (const auto& setup : params["setups"]) {
            setup_json.push_back({{"drydock_id", setup["d"]}, {"ship_id", setup["s"][0]}, {"officer_ids", setup["o"]}});
          }

          slot_params = {{"name", params["name"]}, {"order", params["order"]}, {"setup", setup_json}};
          state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
        }
        break;
      default:
        return;
    }

    const auto id = data["slot_id"].get<int64_t>();
    {
      std::lock_guard lk(slot_states_mtx);

      if (const auto& it = slot_states.find(id); it == slot_states.end() || it->second != state_value) {
        slot_states[id] = state_value;

        auto slot_array = json::array();
        slot_array.push_back({{"type", SyncConfig::Type::Slots},
                              {"sid", id},
                              {"slot_type", type},
                              {"spec_id", data["slot_spec_id"]},
                              {"item_id", item},
                              {"params", slot_params}});

        queue_data(SyncConfig::Type::Slots, slot_array);
      }
    }
  } catch (json::exception& e) {
    spdlog::error("Failed to parse slots JSON: {}", e.what());
  }
}

/** @brief Processes JobResponse — emits new/completed jobs (research, construction, tier-up, scrap). */
void process_jobs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<std::string> jobs_active;
  static std::mutex                      jobs_active_mtx;
  static std::atomic_bool                is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::JobResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "jobs", STR_FORMAT("Processing {} jobs", response.jobs_size()));

    std::unordered_set<std::string> uuids_in_response;
    uuids_in_response.reserve(response.jobs_size());
    auto job_array = json::array();

    for (const auto& job : response.jobs()) {
      const std::string& uuid = job.uuid();
      uuids_in_response.insert(uuid);

      bool emit = false;
      {
        std::lock_guard lk(jobs_active_mtx);
        emit = jobs_active.insert(uuid).second;
      }

      if (!emit) {
        continue;
      }

      json job_params;

      switch (job.type()) {
        case Digit::PrimeServer::Models::JOBTYPE_RESEARCH: {
          const auto& research = job.researchparams();
          job_params           = {{"rid", research.projectid()}, {"level", research.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_STARBASECONSTRUCTION: {
          const auto& construction = job.starbaseconstructionparams();
          job_params               = {{"bid", construction.moduleid()}, {"level", construction.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPTIERUP: {
          const auto& upgrade = job.tierupshipparams();
          job_params          = {{"psid", upgrade.shipid()}, {"tier", upgrade.newtier()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPSCRAP: {
          const auto& scrap = job.scrapyardparams();
          job_params        = {{"psid", scrap.shipid()}, {"hull_id", scrap.hullid()}, {"level", scrap.level()}};
        } break;
        default:
          continue;
      }

      json job_data = json::object({{"type", SyncConfig::Type::Jobs},
                                    {"job_type", job.type()},
                                    {"uuid", job.uuid()},
                                    {"start_time", job.starttime().seconds()},
                                    {"duration", job.duration()},
                                    {"reduction", job.reductioninseconds()}});

      job_data.update(job_params);
      job_array.push_back(std::move(job_data));
    }

    // Prune entries that are no longer present to prevent unbounded growth
    {
      std::lock_guard lk(jobs_active_mtx);
      for (auto it = jobs_active.begin(); it != jobs_active.end();) {
        if (!uuids_in_response.contains(*it)) {
          job_array.push_back({
            {"type", "completed_" + SyncConfig::Type::Jobs},
            {"uuid", *it}
          });
          it = jobs_active.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!job_array.empty()) {
      bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Jobs, job_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse jobs");
  }
}

/** @brief Processes AllianceGamePropertiesResponse — emits Emerald Chain loyalty level changes. */
void process_alliance_games_props(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::atomic_int32_t emerald_chain_level{-1};

  if (auto response = Digit::PrimeServer::Models::AllianceGamePropertiesResponse(); response.ParseFromString(*bytes)) {

    for (const auto& prop : response.properties()) {
      if (prop.propertyname() == "claimed_loyalty_tiers") {
        const auto& claimed_ec_levels = prop.valuelist();
        const auto& max_element =
            std::ranges::max_element(claimed_ec_levels, {}, [](const auto& level) { return std::stoi(level); });
        const auto ec_level = max_element != claimed_ec_levels.end() ? std::stoi(*max_element) : -1;

        int32_t current_level = emerald_chain_level.load();
        if (ec_level != current_level && emerald_chain_level.compare_exchange_strong(current_level, ec_level)) {
          auto ag_array = json::array();
          ag_array.push_back({{"type", SyncConfig::Type::EmeraldChain}, {"level", ec_level}});
          queue_data(SyncConfig::Type::EmeraldChain, ag_array);
        }

        break;
      }
    }
  } else {
    spdlog::error("Failed to parse alliance games properties");
  }
}

// ─── JSON Entity Processors ──────────────────────────────────────────────────

/** @brief Processes the resources JSON section — emits resource amount changes. */
void process_resources(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int64_t> resource_states;
  static std::mutex                           resource_states_mtx;
  static std::atomic_bool                     is_first_sync{true};

  http::sync_log_trace("PROCESS", "resources", STR_FORMAT("Processing {} resources", section.size()));

  auto resource_array = json::array();
  {
    std::lock_guard lk(resource_states_mtx);

    for (const auto& [str_id, resource] : section.get<json::object_t>()) {
      auto id     = std::stoll(str_id);
      auto amount = resource["current_amount"].get<int64_t>();

      if (const auto& it = resource_states.find(id); it == resource_states.end() || it->second != amount) {
        resource_states[id] = amount;
        resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", id}, {"amount", amount}});
      }
    }
  }

  if (!resource_array.empty()) {
    bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
    queue_data(SyncConfig::Type::Resources, resource_array, first_sync);
  }
}

/** @brief Processes the starbase_modules JSON section — emits building level changes. */
void process_starbase_modules(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> module_states;
  static std::mutex                           module_states_mtx;

  http::sync_log_trace("PROCESS", "starbase modules", STR_FORMAT("Processing {} buildings", section.size()));

  auto starbase_array = json::array();
  {
    std::lock_guard lk(module_states_mtx);

    for (const auto& module : section.get<json::object_t>() | std::views::values) {
      const auto id    = module["id"].get<int64_t>();
      const auto level = module["level"].get<int32_t>();

      if (const auto& it = module_states.find(id); it == module_states.end() || it->second != level) {
        module_states[id] = level;
        starbase_array.push_back({{"type", SyncConfig::Type::Buildings}, {"bid", id}, {"level", level}});
      }
    }
  }

  if (!starbase_array.empty()) {
    queue_data(SyncConfig::Type::Buildings, starbase_array);
  }
}

/** @brief Processes the ships JSON section — emits ship tier/level/component changes. */
void process_ships(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, ShipState> ship_states;
  static std::mutex                             ship_states_mtx;
  static std::atomic_bool                       is_first_sync{true};

  http::sync_log_trace("PROCESS", "ships", STR_FORMAT("Processing {} ships", section.size()));

  auto ship_array = json::array();
  {
    std::lock_guard lk(ship_states_mtx);

    for (const auto& ship : section.get<json::object_t>() | std::views::values) {
      const auto      id               = ship["id"].get<int64_t>();
      const auto      tier             = ship["tier"].get<int32_t>();
      const auto      level            = ship["level"].get<int32_t>();
      const auto      level_percentage = ship["level_percentage"].get<double_t>();
      const auto      components       = ship["components"].get<std::vector<int64_t>>();
      const ShipState state{tier, level, level_percentage, components};

      if (const auto& it = ship_states.find(id); it == ship_states.end() || it->second != state) {
        ship_states[id] = state;
        ship_array.push_back({{"type", SyncConfig::Type::Ships},
                              {"psid", id},
                              {"level", level},
                              {"level_percentage", level_percentage},
                              {"tier", tier},
                              {"hull_id", ship["hull_id"].get<int64_t>()},
                              {"components", components}});
      }
    }
  }

  if (!ship_array.empty()) {
    bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
    queue_data(SyncConfig::Type::Ships, ship_array, first_sync);
  }
}

/**
 * @brief Top-level JSON entity dispatcher.
 *
 * Parses the JSON blob and routes each key (battle_result_headers, resources,
 * starbase_modules, ships) to its respective processor, respecting sync_options.
 */
void process_json(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  try {
    const auto result = json::parse(bytes->begin(), bytes->end());

    for (const auto& [key, section] : result.items()) {
      if (key == "battle_result_headers") {
        if (!Config::Get().sync_options.battlelogs) {
          continue;
        }

        process_battle_headers(section);

      } else if (key == "resources") {
        if (!Config::Get().sync_options.resources) {
          continue;
        }

        process_resources(section);

      } else if (key == "starbase_modules") {
        if (!Config::Get().sync_options.buildings) {
          continue;
        }

        process_starbase_modules(section);

      } else if (key == "ships") {
        if (!Config::Get().sync_options.ships) {
          continue;
        }

        process_ships(section);
      }
    }
  } catch (const json::exception& e) {
    spdlog::error("Error parsing json: {}", e.what());
  }
}

// ─── Entity Group Dispatch ───────────────────────────────────────────────────

/**
 * @brief Central dispatcher for entity group data.
 *
 * Routes each EntityGroup by type to the appropriate process_* function,
 * spawning each on a detached thread via submit_async. Checks the
 * corresponding sync_options flag before dispatching.
 */
void HandleEntityGroup(EntityGroup* entity_group)
{
  if (entity_group == nullptr || entity_group->Group == nullptr || entity_group->Group->bytes == nullptr
      || entity_group->Group->Length <= 0) {
    return;
  }

  const auto byteCount = static_cast<size_t>(entity_group->Group->Length);
  auto       bytesPtr  = reinterpret_cast<const char*>(entity_group->Group->bytes->m_Items);

  // Helper to run processing asynchronously with exception handling
  auto submit_async = [bytesPtr, byteCount]<typename T>(T&& func) {
    auto payload = std::make_unique<std::string>(bytesPtr, byteCount);

    try {
      std::thread([f = std::forward<T>(func), p = std::move(payload)]() mutable {
        try {
          f(std::move(p));
        } catch (const std::exception& e) {
          spdlog::error("Exception in HandleEntityGroup: {}", e.what());
        } catch (...) {
          spdlog::error("Unknown exception in HandleEntityGroup");
        }
      }).detach();
    } catch (const std::exception& e) {
      spdlog::error("Failed to spawn async task: {}", e.what());
    } catch (...) {
      spdlog::error("Failed to spawn async task: unknown exception");
    }
  };

  switch (entity_group->Type_) {
    case EntityGroup::Type::ActiveMissions:
      if (Config::Get().sync_options.missions) {
        submit_async(process_active_missions);
      }
      break;
    case EntityGroup::Type::CompletedMissions:
      if (Config::Get().sync_options.missions) {
        submit_async(process_completed_missions);
      }
      break;
    case EntityGroup::Type::PlayerInventories:
      if (Config::Get().sync_options.inventory) {
        submit_async(process_player_inventories);
      }
      break;
    case EntityGroup::Type::ResearchTreesState:
      if (Config::Get().sync_options.research) {
        submit_async(process_research_trees_state);
      }
      break;
    case EntityGroup::Type::Officers:
      if (Config::Get().sync_options.officer) {
        submit_async(process_officers);
      }
      break;
    case EntityGroup::Type::ForbiddenTechs:
      if (Config::Get().sync_options.tech) {
        submit_async(process_forbidden_techs);
      }
      break;
    case EntityGroup::Type::ActiveOfficerTraits:
      if (Config::Get().sync_options.traits) {
        submit_async(process_active_officer_traits);
      }
      break;
    case EntityGroup::Type::Json:
      if (const auto& o = Config::Get().sync_options; o.battlelogs || o.resources || o.ships || o.buildings) {
        submit_async(process_json);
      }
      break;
    case EntityGroup::Type::Jobs:
      if (Config::Get().sync_options.jobs) {
        submit_async(process_jobs);
      }
      break;
    case EntityGroup::Type::GlobalActiveBuffs:
      if (Config::Get().sync_options.buffs) {
        submit_async(process_global_active_buffs);
      }
      break;
    case EntityGroup::Type::EntitySlots:
      if (Config::Get().sync_options.slots) {
        submit_async(process_entity_slots);
      }
      break;
    case EntityGroup::Type::AllianceGetGameProperties:
      if (Config::Get().sync_options.buffs) {
        submit_async(process_alliance_games_props);
      }
      break;
    case EntityGroup::Type::UserProfiles:
      if (Config::Get().sync_options.battlelogs) {
        submit_async(cache_player_names);
      }
      break;
    case EntityGroup::Type::AllianceProfiles:
      if (Config::Get().sync_options.battlelogs) {
        submit_async(cache_alliance_names);
      }
      break;
    default:
      break;
  }
}

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

/**
 * @brief Hook: DataContainer::ParseBinaryObject
 *
 * Intercepts binary entity group parsing to extract data before the game processes it.
 * Original method: deserializes a protobuf entity group into the data container.
 * Our modification: calls HandleEntityGroup() first, then the original.
 */
void DataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
}

/**
 * @brief Hook: SlotDataContainer::ParseSlotUpdatedJson / ParseSlotRemovedJson
 *
 * Intercepts real-time slot update/removal notifications (RTC channel).
 * Original method: parses a JSON payload for slot changes.
 * Our modification: spawns process_entity_slots_rtc() on a detached thread.
 */
void DataContainer_ParseRtcPayload(auto original, void* _this, bool incrementalJsonParsing, RealtimeDataPayload* data)
{
  original(_this, incrementalJsonParsing, data);

  if (data == nullptr || data->Target == nullptr || data->DataType == nullptr || data->Data == nullptr) {
    return;
  }

  const auto target = to_string(data->Target);
  if (target != "slot:assign" && target != "slot:clear") {
    return;
  }

  const auto type_string = to_string(data->DataType);
  if (std::stoi(type_string) != DataType::JSON) {
    return;
  }

  const auto rtcData = to_string(data->Data);
  auto payload = std::make_unique<std::string>(rtcData);

  std::thread([p = std::move(payload)]() mutable {
    try {
      process_entity_slots_rtc(std::move(p));
    } catch (const std::exception& e) {
      spdlog::error("Exception in ParseRtcPayload: {}", e.what());
    } catch (...) {
      spdlog::error("Unknown exception in ParseRtcPayload");
    }
  }).detach();
}

/**
 * @brief Hook: GameServerModelRegistry::ProcessResultInternal
 *
 * Intercepts HTTP response processing to extract entity groups from the response.
 * Original method: processes the service response and invokes callbacks.
 * Our modification: iterates entity groups and calls HandleEntityGroup() before original.
 */
void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, HttpResponse* http_response,
                                                   ServiceResponse* service_response, void* callback,
                                                   void* callback_error)
{
  const auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    const auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, http_response, service_response, callback, callback_error);
}

/**
 * @brief Hook: GameServerModelRegistry::HandleBinaryObjects
 *
 * Intercepts bulk binary object handling to extract entity groups.
 * Same pattern as ProcessResultInternal but for binary-only responses.
 */
void GameServerModelRegistry_HandleBinaryObjects(auto original, void* _this, ServiceResponse* service_response)
{
  const auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    const auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, service_response);
}

/**
 * @brief Hook: PrimeApp::InitPrimeServer
 *
 * Captures the game server URL and session ID so we can make authenticated
 * requests to the Scopely API for combat log enrichment.
 */
void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl, Il2CppString* gatewayServerUrl,
                              Il2CppString* sessionId, Il2CppString* serverRegion)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId, serverRegion);
  http::headers::instanceSessionId = to_string(to_wstring(sessionId));
  http::headers::gameServerUrl     = to_string(to_wstring(gameServerUrl));
}

/** @brief Hook: GameServer::Initialise — captures the game version string for HTTP headers. */
void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                           bool encryptRequests, Il2CppString* serverRegion)
{
  original(_this, sessionId, gameVersion, encryptRequests, serverRegion);
  http::headers::primeVersion = to_string(to_wstring(gameVersion));
}

/** @brief Hook: GameServer::SetInstanceIdHeader — captures the instance ID for HTTP headers. */
void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::instanceId = instanceId;
}

// ─── Hook Installation ──────────────────────────────────────────────────────

/**
 * @brief Installs all sync-related hooks and starts background threads.
 *
 * Hooks multiple DataContainer::ParseBinaryObject variants to intercept
 * entity group data, hooks PrimeApp/GameServer for session credentials,
 * and spawns the two long-lived consumer threads:
 *  - ship_sync_data (main sync queue consumer)
 *  - ship_combat_log_data (combat log enrichment consumer)
 */
void InstallSyncPatches()
{
  load_previously_sent_logs();

  void* process_result_internal_target = nullptr;

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
  } else {
    auto ptr = game_server_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
      process_result_internal_target = ptr;
    }

    ptr = game_server_model_registry.GetMethod("HandleBinaryObjects");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegsitry", "HandleBinaryObjects");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_HandleBinaryObjects);
    }
  }

  if (auto platform_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
      !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (const auto ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInterval");
    } else if (ptr == process_result_internal_target) {
      spdlog::info("PlatformModelRegistry::ProcessResultInternal shares address with GameServerModelRegistry — already hooked");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }

  if (auto buff_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffDataContainer");
      !buff_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffDataContainer");
  } else {
    if (const auto ptr = buff_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto buff_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
      !buff_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    if (const auto ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto inventory_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                              "Digit.PrimeServer.Services", "InventoryDataContainer");
      !inventory_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
  } else {
    if (const auto ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
      !job_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobService");
  } else {
    if (const auto ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service_data_container = il2cpp_get_class_helper(
          "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobServiceDataContainer");
      !job_service_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobServiceDataContainer");
  } else {
    if (const auto ptr = job_service_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobServiceDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto missions_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
      !missions_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
  } else {
    if (const auto ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                             "Digit.PrimeServer.Services", "ResearchDataContainer");
      !research_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
  } else {
    if (const auto ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
      !research_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
  } else {
    if (const auto ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto slot_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SlotDataContainer");
      !slot_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "SlotDataContainer");
  } else {
    if (const auto ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotUpdatedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotUpdatedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotRemovedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotRemovedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    if (const auto ptr = prime_app.GetMethod("InitPrimeServer"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
    } else {
      SPUD_STATIC_DETOUR(ptr, PrimeApp_InitPrimeServer);
    }
  }

  if (auto game_server =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
      !game_server.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
  } else {
    if (const auto ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_Initialise);
    }

    if (const auto ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_SetInstanceIdHeader);
    }
  }

  std::thread(ship_sync_data).detach();
  std::thread(ship_combat_log_data).detach();
}
