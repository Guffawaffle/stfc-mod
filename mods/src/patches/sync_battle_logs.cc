/**
 * @file sync_battle_logs.cc
 * @brief Battle-log queueing, enrichment, resolver cache, and sent-ID persistence.
 */
#include "patches/sync_battle_logs.h"

#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "patches/sync_transport.h"

#include <Digit.PrimeServer.Models.pb.h>
#include <EASTL/algorithm.h>
#include <EASTL/bonus/ring_buffer.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#if _WIN32
#include <winrt/Windows.Foundation.h>
#endif

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
