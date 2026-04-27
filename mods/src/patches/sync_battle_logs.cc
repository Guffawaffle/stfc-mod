/**
 * @file sync_battle_logs.cc
 * @brief Battle-log queueing, enrichment, resolver cache, and sent-ID persistence.
 */
#include "patches/sync_battle_logs.h"

#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "patches/battle_log_decoder.h"
#include "patches/sync_transport.h"
#include "str_utils.h"
#include "testable_functions.h"

#include <Digit.PrimeServer.Models.pb.h>
#include <EASTL/algorithm.h>
#include <EASTL/bonus/ring_buffer.h>
#include <nlohmann/json.hpp>
#include <prime/ActivatedAbilityManager.h>
#include <prime/GameWorldManager.h>
#include <prime/SpecManager.h>
#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#if _WIN32
#include <winrt/Windows.Foundation.h>
#endif

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <string>
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

#if _WIN32
struct WinRtApartmentGuard {
  WinRtApartmentGuard() { winrt::init_apartment(); }
  ~WinRtApartmentGuard() { winrt::uninit_apartment(); }
};
#endif

std::mutex              combat_log_data_mtx;
std::condition_variable combat_log_data_cv;
std::queue<uint64_t>    combat_log_data_queue;

struct CachedPlayerData {
  std::string                           name;
  int64_t                               alliance{-1};
  std::chrono::steady_clock::time_point expires_at;
};
std::unordered_map<std::string, CachedPlayerData> player_data_cache;
std::mutex                                        player_data_cache_mtx;

struct CachedAllianceData {
  std::string                           name;
  std::string                           tag;
  std::chrono::steady_clock::time_point expires_at;
};
std::unordered_map<int64_t, CachedAllianceData> alliance_data_cache;
std::mutex                                      alliance_data_cache_mtx;

static eastl::ring_buffer<uint64_t> previously_sent_battlelogs;
static std::mutex                   previously_sent_battlelogs_mtx;
static std::mutex                   battle_probe_file_mtx;

static constexpr char kBattleProbeFile[] = "community_patch_battle_probe.jsonl";
static constexpr char kBattleProbeSummaryFile[] = "community_patch_battle_probe_summary.jsonl";
static constexpr char kBattleProbeSegmentsFile[] = "community_patch_battle_probe_segments.jsonl";
static constexpr char kBattleFeedFile[] = "community_patch_battle_feed.jsonl";

static bool battle_probe_enabled()
{
  const auto& config = Config::Get();
  return config.sync_options.battlelogs && config.sync_debug;
}

static int64_t current_probe_unix_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static size_t json_entry_count(const nlohmann::json& value)
{
  if (value.is_array() || value.is_object()) {
    return value.size();
  }

  return 0;
}

static const char* hull_type_label(HullType type)
{
  switch (type) {
  case HullType::Destroyer: return "Destroyer";
  case HullType::Survey: return "Survey";
  case HullType::Explorer: return "Explorer";
  case HullType::Battleship: return "Battleship";
  case HullType::Defense: return "Defense";
  case HullType::ArmadaTarget: return "ArmadaTarget";
  case HullType::Any: return "Any";
  }

  return "Unknown";
}

static std::string normalize_catalog_resource_name(std::string text)
{
  if (text.size() > 5 && text.ends_with("_LIVE")) {
    text.resize(text.size() - 5);
  }

  constexpr std::string_view resource_key_prefix = "Resource_";
  if (text.size() >= resource_key_prefix.size()
      && text.compare(0, resource_key_prefix.size(), resource_key_prefix) == 0) {
    text.erase(0, resource_key_prefix.size());
  }

  for (auto& character : text) {
    if (character == '_') {
      character = ' ';
    }
  }

  constexpr std::string_view resource_display_prefix = "Resource ";
  if (text.size() >= resource_display_prefix.size()
      && text.compare(0, resource_display_prefix.size(), resource_display_prefix) == 0) {
    text.erase(0, resource_display_prefix.size());
  }

  return text;
}

static std::string normalize_catalog_key_name(std::string text)
{
  if (text.size() > 5 && text.ends_with("_LIVE")) {
    text.resize(text.size() - 5);
  }

  for (auto& character : text) {
    if (character == '_') {
      character = ' ';
    }
  }

  return std::string{StripAsciiWhitespace(text)};
}

static std::string il2cpp_string_or_empty(Il2CppString* value)
{
  return value ? to_string(value) : std::string{};
}

static void add_catalog_id(nlohmann::json& metadata, std::string_view key, int64_t value)
{
  if (value != 0) {
    metadata[std::string{key}] = std::to_string(value);
  }
}

static nlohmann::json catalog_loca_metadata(IdRefs* id_refs)
{
  auto metadata = nlohmann::json::object();
  if (!id_refs) {
    return metadata;
  }

  const auto loca_key = std::string{StripAsciiWhitespace(il2cpp_string_or_empty(id_refs->LocaStringId))};
  if (!loca_key.empty()) {
    metadata["locaKey"] = loca_key;
  }
  add_catalog_id(metadata, "locaId", id_refs->LocaId);

  return metadata;
}

static SpecManager* resolve_spec_manager()
{
  return SpecManager::Instance();
}

static HullSpec* resolve_hull_spec(int64_t hull_id)
{
  if (hull_id == 0) {
    return nullptr;
  }

  auto* spec_manager = SpecManager::Instance();
  if (!spec_manager) {
    return nullptr;
  }

  return spec_manager->GetHull(hull_id);
}

static ComponentSpec* resolve_component_spec(int64_t component_id)
{
  if (component_id == 0) {
    return nullptr;
  }

  auto* spec_manager = resolve_spec_manager();
  return spec_manager ? spec_manager->SearchForSpec(component_id) : nullptr;
}

static OfficerSpec* resolve_officer_spec(int64_t officer_id)
{
  if (officer_id == 0) {
    return nullptr;
  }

  auto* spec_manager = resolve_spec_manager();
  return spec_manager ? spec_manager->GetOfficerSpec(officer_id) : nullptr;
}

static BuffSpec* resolve_buff_spec(int64_t buff_id)
{
  if (buff_id == 0) {
    return nullptr;
  }

  auto* spec_manager = resolve_spec_manager();
  return spec_manager ? spec_manager->GetBuffSpec(buff_id, true) : nullptr;
}

static ForbiddenTechSpec* resolve_forbidden_tech_spec(int64_t forbidden_tech_id)
{
  if (forbidden_tech_id == 0) {
    return nullptr;
  }

  auto* spec_manager = resolve_spec_manager();
  return spec_manager ? spec_manager->GetForbiddenTechSpec(forbidden_tech_id) : nullptr;
}

static battle_log_decoder::CatalogResolver build_catalog_resolver()
{
  battle_log_decoder::CatalogResolver resolver{};

  resolver.hull_name = [](int64_t hull_id) {
    auto* hull = resolve_hull_spec(hull_id);
    auto  name = il2cpp_string_or_empty(hull ? hull->Name : nullptr);
    return name.empty() ? std::string{} : parse_hull_key(name);
  };

  resolver.hull_type = [](int64_t hull_id) {
    auto* hull = resolve_hull_spec(hull_id);
    return hull ? std::string{hull_type_label(hull->Type)} : std::string{};
  };

  resolver.resource_name = [](int64_t resource_id) {
    if (resource_id == 0) {
      return std::string{};
    }

    auto* spec_manager = SpecManager::Instance();
    if (!spec_manager) {
      return std::string{};
    }

    auto* spec = spec_manager->GetResourceSpec(resource_id);
    auto  name = il2cpp_string_or_empty(spec ? spec->Name : nullptr);
    return name.empty() ? std::string{} : normalize_catalog_resource_name(name);
  };

  resolver.component_name = [](int64_t component_id) {
    auto* spec = resolve_component_spec(component_id);
    auto  name = il2cpp_string_or_empty(spec ? spec->Name : nullptr);
    return name.empty() ? std::string{} : normalize_catalog_key_name(name);
  };

  resolver.component_metadata = [](int64_t component_id) {
    auto* spec = resolve_component_spec(component_id);
    return catalog_loca_metadata(spec ? spec->IdRefsValue : nullptr);
  };

  resolver.system_metadata = [](int64_t system_id) {
    auto metadata = nlohmann::json::object();
    auto* manager = GameWorldManager::Instance();
    int64_t loca_id = 0;
    if (manager && manager->TryGetGalaxyLocaId(system_id, &loca_id)) {
      add_catalog_id(metadata, "locaId", loca_id);
    }
    return metadata;
  };

  resolver.officer_metadata = [](int64_t officer_id) {
    auto* spec = resolve_officer_spec(officer_id);
    auto  metadata = catalog_loca_metadata(spec ? spec->IdRefsValue : nullptr);
    if (spec) {
      add_catalog_id(metadata, "captainManeuverId", spec->CaptainManeuverId);
      add_catalog_id(metadata, "officerAbilityId", spec->OfficerAbilityId);
      add_catalog_id(metadata, "belowDecksAbilityId", spec->BelowDecksAbilityId);
    }
    return metadata;
  };

  resolver.ability_metadata = [](int64_t ability_id) {
    auto metadata = nlohmann::json::object();
    auto* manager = ActivatedAbilityManager::Instance();
    if (manager) {
      add_catalog_id(metadata, "locaId", manager->GetActivatedAbilityLocaId(ability_id));
    }
    return metadata;
  };

  resolver.forbidden_tech_metadata = [](int64_t forbidden_tech_id) {
    auto* spec = resolve_forbidden_tech_spec(forbidden_tech_id);
    return catalog_loca_metadata(spec ? spec->IdRefsValue : nullptr);
  };

  resolver.buff_metadata = [](int64_t buff_id) {
    auto* spec = resolve_buff_spec(buff_id);
    return catalog_loca_metadata(spec ? spec->IdRefsValue : nullptr);
  };

  resolver.debuff_metadata = [](int64_t debuff_id) {
    auto* spec = resolve_buff_spec(debuff_id);
    return catalog_loca_metadata(spec ? spec->IdRefsValue : nullptr);
  };

  return resolver;
}

static void append_jsonl_probe_entry(const std::string& path, const nlohmann::json& probe_entry,
                                     std::string_view entry_kind)
{
  std::lock_guard lk(battle_probe_file_mtx);

  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::app);
  if (!file.is_open()) {
    spdlog::error("Failed to open {} file for append: {}", entry_kind, path);
    return;
  }

  file << probe_entry.dump() << '\n';
}

static void append_battle_probe_summary(uint64_t journal_id, const nlohmann::json& names, const nlohmann::json& journal,
                                        const nlohmann::json& decoded, int64_t captured_at_unix_ms)
{
  if (!battle_probe_enabled()) {
    return;
  }

  const auto summary_path = File::MakePathString(kBattleProbeSummaryFile, true);

  const auto& initiator_fleet_data = journal.contains("initiator_fleet_data") ? journal["initiator_fleet_data"]
                                                                                : nlohmann::json::object();
  const auto& target_fleet_data    = journal.contains("target_fleet_data") ? journal["target_fleet_data"]
                                                                             : nlohmann::json::object();
  const auto& chest_drop           = journal.contains("chest_drop") ? journal["chest_drop"] : nlohmann::json::object();
  const auto* battle_log           = journal.contains("battle_log") ? &journal["battle_log"] : nullptr;
  const auto signature = decoded.value("ok", false) && decoded.contains("signature") ? decoded["signature"]
                                                                                       : nlohmann::json::object();

  nlohmann::json summary_entry{{"type", "battle_probe_summary"},
                               {"journal_id", journal_id},
                               {"captured_at_unix_ms", captured_at_unix_ms},
                               {"battle_id", journal.value("id", journal_id)},
                               {"battle_time", journal.value("battle_time", std::string{})},
                               {"battle_type", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                               {"battle_duration", journal.value("battle_duration", 0)},
                               {"initiator_id", journal.value("initiator_id", std::string{})},
                               {"target_id", journal.value("target_id", std::string{})},
                               {"initiator_wins", journal.value("initiator_wins", false)},
                               {"system_id", journal.value("system_id", int64_t{0})},
                               {"coords", journal.contains("coords") ? journal["coords"] : nlohmann::json()},
                               {"names_count", json_entry_count(names)},
                               {"battle_log_count", signature.value("token_count", battle_log != nullptr ? json_entry_count(*battle_log) : size_t{0})},
                               {"battle_log_json_type", battle_log != nullptr ? battle_log->type_name() : "missing"},
                               {"battle_log_first_token", signature.contains("first_token") ? signature["first_token"] : nlohmann::json()},
                               {"battle_log_last_token", signature.contains("last_token") ? signature["last_token"] : nlohmann::json()},
                               {"battle_log_negative_tokens", signature.contains("negative_tokens") ? signature["negative_tokens"] : nlohmann::json::array()},
                               {"battle_log_segment_count", signature.value("segment_count", size_t{0})},
                               {"battle_log_top_segment_lengths", signature.contains("top_segment_lengths") ? signature["top_segment_lengths"] : nlohmann::json::array()},
                               {"battle_log_first_12", signature.contains("first_tokens") ? signature["first_tokens"] : nlohmann::json::array()},
                               {"battle_log_last_12", signature.contains("last_tokens") ? signature["last_tokens"] : nlohmann::json::array()},
                               {"battle_log_participant_count", decoded.value("participant_count", size_t{0})},
                               {"battle_log_ship_count", decoded.value("ship_count", size_t{0})},
                               {"battle_log_component_count", decoded.value("component_count", size_t{0})},
                               {"initiator_deployed_fleet_count",
                                initiator_fleet_data.contains("deployed_fleets")
                                    ? json_entry_count(initiator_fleet_data["deployed_fleets"])
                                    : size_t{0}},
                               {"target_deployed_fleet_count",
                                target_fleet_data.contains("deployed_fleets")
                                    ? json_entry_count(target_fleet_data["deployed_fleets"])
                                    : size_t{0}},
                               {"initiator_ship_count",
                                initiator_fleet_data.contains("ship_ids")
                                    ? json_entry_count(initiator_fleet_data["ship_ids"])
                                    : size_t{0}},
                               {"target_ship_count",
                                target_fleet_data.contains("ship_ids") ? json_entry_count(target_fleet_data["ship_ids"])
                                                                         : size_t{0}},
                               {"resources_dropped_count",
                                journal.contains("resources_dropped") ? json_entry_count(journal["resources_dropped"])
                                                                       : size_t{0}},
                               {"resources_transferred_count",
                                journal.contains("resources_transferred")
                                    ? json_entry_count(journal["resources_transferred"])
                                    : size_t{0}},
                               {"loot_roll_key",
                                chest_drop.is_object() ? chest_drop.value("loot_roll_key", std::string{}) : std::string{}},
                               {"chests_gained_count",
                                chest_drop.is_object() && chest_drop.contains("chests_gained")
                                    ? json_entry_count(chest_drop["chests_gained"])
                                    : size_t{0}}};

  append_jsonl_probe_entry(summary_path, summary_entry, "battle probe summary");
}

static void append_battle_probe_segments(uint64_t journal_id, const nlohmann::json& decoded, int64_t captured_at_unix_ms)
{
  if (!battle_probe_enabled() || !BattleLogDecoderEnabled() || !BattleLogDecoderEmitSegments()) {
    return;
  }

  if (!decoded.value("ok", false)) {
    spdlog::warn("Battle log decoder skipped journal {}: {}", journal_id, decoded.value("reason", std::string{"unknown"}));
    return;
  }

  auto segment_entry = decoded;
  segment_entry["type"] = "battle_probe_segments";
  segment_entry["captured_at_unix_ms"] = captured_at_unix_ms;
  append_jsonl_probe_entry(File::MakePathString(kBattleProbeSegmentsFile, true), segment_entry, "battle probe segments");
}

static void append_battle_feed(uint64_t journal_id, const nlohmann::json& names, const nlohmann::json& journal,
                               const nlohmann::json& decoded, int64_t captured_at_unix_ms)
{
  if (!battle_probe_enabled() || !BattleLogDecoderEmitFeed()) {
    return;
  }

  auto capture_event = battle_log_decoder::build_sidecar_battle_capture_event(journal, names, journal_id, captured_at_unix_ms);
  if (capture_event.value("ok", true) == false) {
    spdlog::warn("Battle capture feed skipped journal {}: {}", journal_id,
                 capture_event.value("reason", std::string{"unknown"}));
  } else {
    append_jsonl_probe_entry(File::MakePathString(kBattleFeedFile, true), capture_event, "battle capture feed");
  }

  if (!BattleLogDecoderEnabled()) {
    return;
  }

  auto event = battle_log_decoder::build_sidecar_battle_report_event(journal, decoded, journal_id, captured_at_unix_ms);
  if (event.value("ok", true) == false) {
    spdlog::warn("Battle feed skipped journal {}: {}", journal_id, event.value("reason", std::string{"unknown"}));
    return;
  }

  append_jsonl_probe_entry(File::MakePathString(kBattleFeedFile, true), event, "battle feed");

  auto catalog_resolver = build_catalog_resolver();
  auto catalog_event = battle_log_decoder::build_sidecar_catalog_snapshot_event(
      journal, names, decoded, catalog_resolver, journal_id, captured_at_unix_ms);
  if (catalog_event.value("ok", true) == false) {
    spdlog::warn("Catalog snapshot feed skipped journal {}: {}", journal_id,
                 catalog_event.value("reason", std::string{"unknown"}));
  } else {
    append_jsonl_probe_entry(File::MakePathString(kBattleFeedFile, true), catalog_event, "catalog snapshot feed");
  }

  auto analytics_event = battle_log_decoder::build_sidecar_battle_analytics_event(journal, decoded, journal_id, captured_at_unix_ms);
  if (analytics_event.value("ok", true) == false) {
    spdlog::warn("Battle analytics feed skipped journal {}: {}", journal_id,
                 analytics_event.value("reason", std::string{"unknown"}));
    return;
  }

  append_jsonl_probe_entry(File::MakePathString(kBattleFeedFile, true), analytics_event, "battle analytics feed");
}

static void append_battle_probe(uint64_t journal_id, const nlohmann::json& names, const nlohmann::json& journal)
{
  if (!battle_probe_enabled()) {
    return;
  }

  const auto probe_path = File::MakePathString(kBattleProbeFile, true);
  const auto captured_at_unix_ms = current_probe_unix_ms();

  nlohmann::json probe_entry{{"type", "battle_probe"},
                             {"journal_id", journal_id},
                             {"captured_at_unix_ms", captured_at_unix_ms},
                             {"names", names},
                             {"journal", journal}};

  battle_log_decoder::DecodeOptions decode_options;
  decode_options.include_segments = BattleLogDecoderEnabled()
                                    && (BattleLogDecoderEmitSegments() || BattleLogDecoderEmitFeed());
  auto decoded = battle_log_decoder::decode_journal(journal, names, decode_options, journal_id);

  append_jsonl_probe_entry(probe_path, probe_entry, "battle probe");
  append_battle_probe_summary(journal_id, names, journal, decoded, captured_at_unix_ms);
  append_battle_probe_segments(journal_id, decoded, captured_at_unix_ms);
  append_battle_feed(journal_id, names, journal, decoded, captured_at_unix_ms);
  spdlog::debug("Appended battle probe for journal {} to {}", journal_id, probe_path);
}

void load_previously_sent_logs()
{
  using json = nlohmann::json;
  std::lock_guard lk(previously_sent_battlelogs_mtx);

  previously_sent_battlelogs.set_capacity(300);

  try {
    std::ifstream file(File::Battles(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      spdlog::warn("Failed to open battles file (not found or not readable); starting with empty cache");
      return;
    }

    const auto battlelogs = json::parse(file);
    for (const auto& v : battlelogs) {
      previously_sent_battlelogs.push_back(v.get<uint64_t>());
    }

    spdlog::debug("Loaded {} previously sent battle logs", previously_sent_battlelogs.size());
  } catch (const std::exception& exception) {
    spdlog::error("Failed to parse battles file: {}", exception.what());
  } catch (...) {
    spdlog::error("Failed to parse battles file");
  }
}

static void save_previously_sent_logs()
{
  using json           = nlohmann::json;
  auto battlelog_array = json::array();

  {
    std::lock_guard lk(previously_sent_battlelogs_mtx);
    for (auto id : previously_sent_battlelogs) {
      battlelog_array.push_back(id);
    }
  }

  try {
    std::ofstream file(File::Battles(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      spdlog::error("Failed to open battles file for writing");
      return;
    }

    file << battlelog_array.dump();
    spdlog::trace("Saved {} previously sent battle logs", battlelog_array.size());

  } catch (const std::exception& exception) {
    spdlog::error("Failed to save battles JSON: {}", exception.what());
  } catch (...) {
    spdlog::error("Unknown error while saving battles JSON.");
  }
}

void process_battle_headers(const nlohmann::json& section)
{
  http::sync_log_trace("PROCESS", "battle headers", STR_FORMAT("Processing {} battle headers", section.size()));

  std::vector<uint64_t> battle_ids;
  battle_ids.reserve(section.size());

  for (const auto& battle : section) {
    const auto id = battle["id"].get<uint64_t>();
    battle_ids.push_back(id);
  }

  std::vector<uint64_t> to_enqueue;
  {
    std::lock_guard lk(previously_sent_battlelogs_mtx);

    for (const auto id : battle_ids | std::views::reverse) {
      if (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id)
          == previously_sent_battlelogs.end()) {
        previously_sent_battlelogs.push_back(id);
        to_enqueue.push_back(id);
      }
    }
  }

  if (!to_enqueue.empty()) {
    http::sync_log_trace("PROCESS", "battle headers",
                         STR_FORMAT("Queuing {} battles for background processing", to_enqueue.size()));

    {
      std::lock_guard lk(combat_log_data_mtx);
      for (const auto id : to_enqueue) {
        combat_log_data_queue.push(id);
      }
    }

    save_previously_sent_logs();
    combat_log_data_cv.notify_all();
  }
}

void cache_player_names(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::UserProfilesResponse(); response.ParseFromString(*bytes)) {

    std::unordered_map<std::string, CachedPlayerData> names;
    const auto                                        expires_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

    for (const auto& profile : response.userprofiles()) {
      names.insert_or_assign(profile.userid(), CachedPlayerData{profile.name(), profile.allianceid(), expires_at});
    }

    {
      std::lock_guard lk(player_data_cache_mtx);
      player_data_cache.insert(names.begin(), names.end());
    }
  } else {
    spdlog::error("Failed to parse user profile");
  }
}

void cache_alliance_names(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::GetAllianceProfilesResponse(); response.ParseFromString(*bytes)) {

    std::unordered_map<int64_t, CachedAllianceData> names;
    const auto                                      expires_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

    for (const auto& alliance : response.allianceprofiles()) {
      if (alliance.id() > 0) {
        names.insert_or_assign(alliance.id(), CachedAllianceData{alliance.name(), alliance.tag(), expires_at});
      }
    }

    {
      std::lock_guard lk(alliance_data_cache_mtx);
      alliance_data_cache.insert(names.begin(), names.end());
    }

  } else {
    spdlog::error("Failed to parse alliance profile");
  }
}

inline void collect_user_ids_from_fleet(const nlohmann::json& fleet_data, std::unordered_set<std::string>& user_ids)
{
  if (!fleet_data.contains("ref_ids") || fleet_data["ref_ids"].is_null()) {
    for (const auto& fleet : fleet_data["deployed_fleets"]) {
      const auto& player_id = fleet["uid"].get<std::string>();
      user_ids.insert(player_id);
    }
  }
}

inline void collect_alliance_ids(const nlohmann::json& names, std::unordered_set<int64_t>& alliance_ids)
{
  for (const auto& [player_id, entry] : names.items()) {
    try {
      const auto alliance_id = entry["alliance_id"].get<int64_t>();
      alliance_ids.insert(alliance_id);
    } catch (const nlohmann::json::exception&) {
    }
  }
}

void resolve_player_names(const std::unordered_set<std::string>& user_ids, nlohmann::json& out_names,
                          nlohmann::json& out_request_ids, const std::chrono::time_point<std::chrono::steady_clock> now)
{
  std::lock_guard lk(player_data_cache_mtx);

  for (const auto& user_id : user_ids) {
    const auto it = player_data_cache.find(user_id);
    if (it != player_data_cache.end()) {
      if (it->second.expires_at > now) {
        out_names[user_id] = {{"name", it->second.name},
                              {"alliance_id", it->second.alliance},
                              {"alliance_name", nullptr},
                              {"alliance_tag", nullptr}};
      } else {
        player_data_cache.erase(it);
        out_request_ids.push_back(user_id);
      }
    } else {
      out_request_ids.push_back(user_id);
    }
  }
}

void resolve_alliance_names(const std::unordered_set<int64_t>& alliance_ids, nlohmann::json& out_names,
                            nlohmann::json&                                          out_request_ids,
                            const std::chrono::time_point<std::chrono::steady_clock> now)
{
  std::lock_guard lk(alliance_data_cache_mtx);

  for (const auto& alliance_id : alliance_ids) {
    const auto it = alliance_data_cache.find(alliance_id);
    if (it != alliance_data_cache.end()) {
      if (it->second.expires_at > now) {
        for (auto& [player_id, entry] : out_names.items()) {
          try {
            if (entry["alliance_id"].get<int64_t>() == alliance_id) {
              entry["alliance_name"] = it->second.name;
              entry["alliance_tag"]  = it->second.tag;
              entry.erase("alliance_id");
            }
          } catch (const nlohmann::json::exception&) {
          }
        }
      } else {
        alliance_data_cache.erase(it);
        out_request_ids.push_back(alliance_id);
      }
    }
  }
}

void ship_combat_log_data()
{
  using json = nlohmann::json;

#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  for (;;) {
    uint64_t journal_id;
    {
      std::unique_lock lock(combat_log_data_mtx);
      combat_log_data_cv.wait(lock, [] { return !combat_log_data_queue.empty(); });
      journal_id = combat_log_data_queue.front();
      combat_log_data_queue.pop();
    }

    const bool capture_locally = battle_probe_enabled();
    const bool send_remotely   = !Config::Get().sync_targets.empty();

    if (!capture_locally && !send_remotely) {
      spdlog::debug("Skipping combat log fetch for battle {} because no sync targets are configured and sync.debug is disabled",
                    journal_id);
      continue;
    }

    try {
      http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching combat log for battle {}", journal_id));

      const json journals_body{{"journal_id", journal_id}};
      auto       battle_log = http::get_scopely_data("/journals/get", journals_body.dump());
      json       battle_json;

      if (battle_log.empty()) {
        continue;
      }

      try {
        battle_json = std::move(json::parse(battle_log));
      } catch (const json::exception& exception) {
        spdlog::error("Error parsing journal response from game server: {}", exception.what());
        continue;
      }

      const auto& journal              = battle_json["journal"];
      const auto& target_fleet_data    = journal["target_fleet_data"];
      const auto& initiator_fleet_data = journal["initiator_fleet_data"];

      auto       names      = json::object();
      const auto now        = std::chrono::steady_clock::now();
      const auto expires_at = now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

      {
        std::unordered_set<std::string> user_ids;
        collect_user_ids_from_fleet(target_fleet_data, user_ids);
        collect_user_ids_from_fleet(initiator_fleet_data, user_ids);

        json profiles_request{{"user_ids", json::array()}};
        resolve_player_names(user_ids, names, profiles_request["user_ids"], now);

        const auto fetch_count = profiles_request["user_ids"].size();
        if (fetch_count > 0) {
          http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching {} player profiles", fetch_count));

          auto profiles      = http::get_scopely_data("/user_profile/profiles", profiles_request.dump());
          auto profiles_json = json::parse(profiles);

          std::lock_guard lk(player_data_cache_mtx);

          try {
            for (const auto& [player_id, profile] : profiles_json["user_profiles"].get<json::object_t>()) {
              const auto& name        = profile["name"].get<std::string>();
              const auto& alliance_id = profile["alliance_id"].get<int64_t>();

              names[player_id] = {
                  {"name", name}, {"alliance_id", alliance_id}, {"alliance_name", nullptr}, {"alliance_tag", nullptr}};
              player_data_cache[player_id] = {name, alliance_id, expires_at};
            }
          } catch (const json::exception& exception) {
            spdlog::error("Failed to parse user profiles: {}", exception.what());
          }
        }
      }

      {
        std::unordered_set<int64_t> alliance_ids;
        json alliances_request{{"user_current_rank", 0}, {"alliance_id", 0}, {"alliance_ids", json::array()}};

        collect_alliance_ids(names, alliance_ids);
        resolve_alliance_names(alliance_ids, names, alliances_request["alliance_ids"], now);

        const auto fetch_count = alliances_request["alliance_ids"].size();
        if (fetch_count > 0) {
          http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching {} alliance profiles", fetch_count));

          auto profiles      = http::get_scopely_data("/alliance/get_alliances_public_info", alliances_request.dump());
          auto profiles_json = json::parse(profiles);

          std::lock_guard lk(alliance_data_cache_mtx);

          try {
            for (const auto& [alliance_id_str, profile] : profiles_json["alliances_info"].get<json::object_t>()) {
              const auto  id   = profile["id"].get<int64_t>();
              const auto& name = profile["name"].get<std::string>();
              const auto& tag  = profile["tag"].get<std::string>();

              alliance_data_cache[id] = {name, tag, expires_at};
            }
          } catch (json::exception& exception) {
            spdlog::error("Failed to parse alliance profiles: {}", exception.what());
          }

          for (auto& [player_id, entry] : names.items()) {
            try {
              if (entry.contains("alliance_id")) {
                const auto alliance_id = entry["alliance_id"].get<int64_t>();
                const auto it          = alliance_data_cache.find(alliance_id);
                if (it != alliance_data_cache.end()) {
                  entry["alliance_name"] = it->second.name;
                  entry["alliance_tag"]  = it->second.tag;
                  entry.erase("alliance_id");
                }
              }
            } catch (json::exception& exception) {
              spdlog::error("Failed to update cached player data: {}", exception.what());
            }
          }
        }
      }

      append_battle_probe(journal_id, names, journal);

      if (!send_remotely) {
        spdlog::debug("Captured battle journal {} locally; no sync targets configured", journal_id);
        continue;
      }

      auto battle_array = json::array();
      battle_array.push_back(
          {{"type", SyncConfig::Type::Battles}, {"names", names}, {"journal", battle_json["journal"]}});

      try {
        auto ship_data = battle_array.dump();
        http::send_data(SyncConfig::Type::Battles, ship_data, false);
      } catch (const std::runtime_error& exception) {
        ErrorMsg::SyncRuntime("combat", exception);
      } catch (const std::exception& exception) {
        ErrorMsg::SyncException("combat", exception);
      } catch (const std::wstring& message) {
        ErrorMsg::SyncMsg("combat", message);
#if _WIN32
      } catch (winrt::hresult_error const& exception) {
        ErrorMsg::SyncWinRT("combat", exception);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("combat", "Unknown error during sending of sync data");
      }

    } catch (json::exception& exception) {
      spdlog::error("Error parsing combat log or profiles: {}", exception.what());
    } catch (std::exception& exception) {
      spdlog::error("Error processing combat log: {}", exception.what());
    } catch (...) {
      spdlog::error("Unknown error during processing of combat log data");
    }
  }
}
