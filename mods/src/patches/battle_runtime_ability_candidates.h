/**
 * @file battle_runtime_ability_candidates.h
 * @brief Experimental pure helpers for discovering battle-log ability/effect row candidates.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace battle_runtime_ability_candidates
{
struct EntityHints {
  std::function<bool(int64_t)> is_ship_id;
  std::function<bool(int64_t)> is_hull_id;
  std::function<bool(int64_t)> is_component_id;
  std::function<bool(int64_t)> is_officer_id;
};

[[nodiscard]] nlohmann::json BuildPreAttackCandidates(const nlohmann::json& record, size_t payload_start,
                                                      const EntityHints& entity_hints);

[[nodiscard]] nlohmann::json BuildTriggeredEffectCandidates(const nlohmann::json& record, size_t search_start,
                                                            const EntityHints& entity_hints);

[[nodiscard]] nlohmann::json BuildMitigationOrScalarCandidates(const nlohmann::json& record, size_t payload_start,
                                                               const EntityHints& entity_hints);

void AddCombatContext(nlohmann::json& candidates, size_t segment_index, size_t record_index, size_t round,
                      size_t sub_round);

[[nodiscard]] nlohmann::json CollectFromAttackRows(const nlohmann::json& attack_rows, const std::string& battle_id);

[[nodiscard]] nlohmann::json BuildCandidateSummary(const nlohmann::json& candidates);
} // namespace battle_runtime_ability_candidates
