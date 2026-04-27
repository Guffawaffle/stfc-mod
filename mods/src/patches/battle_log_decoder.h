/**
 * @file battle_log_decoder.h
 * @brief Pure decoder utilities for Scopely battle journal battle_log arrays.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace battle_log_decoder
{
struct DecodeOptions {
  bool                 include_segments = false;
  size_t               segment_limit = 0;
  size_t               edge_token_count = 12;
  size_t               top_segment_length_limit = 6;
  std::vector<int64_t> battle_type_filter;
};

[[nodiscard]] bool journal_matches_options(const nlohmann::json& probe_or_journal, const DecodeOptions& options);

[[nodiscard]] nlohmann::json decode_probe_entry(const nlohmann::json& probe_entry,
                                                const DecodeOptions& options = {});

[[nodiscard]] nlohmann::json decode_journal(const nlohmann::json& journal,
                                            const nlohmann::json& names = nlohmann::json::object(),
                                            const DecodeOptions& options = {},
                                            uint64_t journal_id_override = 0);

[[nodiscard]] nlohmann::json build_sidecar_battle_report_event(const nlohmann::json& journal,
                                                               const nlohmann::json& decoded,
                                                               uint64_t journal_id_override = 0,
                                                               int64_t captured_at_unix_ms = 0);

[[nodiscard]] nlohmann::json build_sidecar_battle_capture_event(const nlohmann::json& journal,
                                                                const nlohmann::json& names = nlohmann::json::object(),
                                                                uint64_t journal_id_override = 0,
                                                                int64_t captured_at_unix_ms = 0);

[[nodiscard]] nlohmann::json build_sidecar_battle_analytics_event(const nlohmann::json& journal,
                                                                  const nlohmann::json& decoded,
                                                                  uint64_t journal_id_override = 0,
                                                                  int64_t captured_at_unix_ms = 0);

/**
 * @brief Resolver hooks for catalog name lookups.
 *
 * The mod-side catalog emitter populates this struct with IL2CPP-backed
 * lookups (e.g. SpecService::GetHull -> HullSpec::Name). Tests and
 * dataless callers leave the callbacks empty; observed IDs are still
 * recorded with `unresolved: true`.
 */
struct CatalogResolver {
  std::function<std::string(int64_t)> hull_name;
  std::function<std::string(int64_t)> hull_type;
  std::function<std::string(int64_t)> ship_name;
  std::function<std::string(int64_t)> component_name;
  std::function<std::string(int64_t)> resource_name;
  std::function<std::string(int64_t)> system_name;
  std::function<std::string(int64_t)> officer_name;
  std::function<std::string(int64_t)> ability_name;
  std::function<std::string(int64_t)> forbidden_tech_name;
  std::function<std::string(int64_t)> buff_name;
  std::function<std::string(int64_t)> debuff_name;

  std::function<nlohmann::json(int64_t)> hull_metadata;
  std::function<nlohmann::json(int64_t)> ship_metadata;
  std::function<nlohmann::json(int64_t)> component_metadata;
  std::function<nlohmann::json(int64_t)> resource_metadata;
  std::function<nlohmann::json(int64_t)> system_metadata;
  std::function<nlohmann::json(int64_t)> officer_metadata;
  std::function<nlohmann::json(int64_t)> ability_metadata;
  std::function<nlohmann::json(int64_t)> forbidden_tech_metadata;
  std::function<nlohmann::json(int64_t)> buff_metadata;
  std::function<nlohmann::json(int64_t)> debuff_metadata;
};

[[nodiscard]] nlohmann::json build_sidecar_catalog_snapshot_event(const nlohmann::json& journal,
                                                                  const nlohmann::json& names,
                                                                  const nlohmann::json& decoded,
                                                                  const CatalogResolver& resolver = {},
                                                                  uint64_t journal_id_override = 0,
                                                                  int64_t captured_at_unix_ms = 0);

[[nodiscard]] nlohmann::json compare_probe_entries(const nlohmann::json& left,
                                                   const nlohmann::json& right,
                                                   const DecodeOptions& options = {});
} // namespace battle_log_decoder