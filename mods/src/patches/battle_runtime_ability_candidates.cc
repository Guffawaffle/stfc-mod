/**
 * @file battle_runtime_ability_candidates.cc
 * @brief Experimental token-group discovery for runtime battle ability/effect rows.
 */
#include "patches/battle_runtime_ability_candidates.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace battle_runtime_ability_candidates
{
namespace
{
  constexpr int64_t kRoundStartMarker              = -96;
  constexpr int64_t kSourceValueMarker             = -86;
  constexpr int64_t kOwnerShipMarker               = -85;
  constexpr int64_t kSecondaryOwnerMarker          = -84;
  constexpr int64_t kSecondaryValueMarker          = -82;
  constexpr int64_t kSecondaryOwnerEndMarker       = -81;
  constexpr int64_t kCombatantRefMarker            = -88;
  constexpr int64_t kAttackPreludeTerminator       = -83;
  constexpr int64_t kTriggeredEffectStartMarker    = -93;
  constexpr int64_t kTriggeredEffectValueMarker    = -91;
  constexpr int64_t kTriggeredEffectValueEndMarker = -92;
  constexpr int64_t kTriggeredEffectTerminator     = -94;
  constexpr size_t  kMaxRawTokenSamples            = 64;
  constexpr size_t  kMaxMarkerFields               = 24;
  constexpr size_t  kMaxMarkerFieldValues          = 16;
  constexpr size_t  kMaxCandidateIntegers          = 16;

  [[nodiscard]] std::optional<int64_t> json_to_i64(const nlohmann::json& value)
  {
    try {
      if (value.is_number_integer()) {
        return value.get<int64_t>();
      }
      if (value.is_number_unsigned()) {
        const auto unsigned_value = value.get<uint64_t>();
        if (unsigned_value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
          return static_cast<int64_t>(unsigned_value);
        }
      }
      if (value.is_string()) {
        const auto text     = value.get<std::string>();
        size_t     consumed = 0;
        const auto parsed   = std::stoll(text, &consumed, 10);
        if (consumed == text.size()) {
          return parsed;
        }
      }
    } catch (...) {
    }
    return std::nullopt;
  }

  [[nodiscard]] bool json_matches_i64(const nlohmann::json& value, int64_t expected)
  {
    const auto parsed = json_to_i64(value);
    return parsed && *parsed == expected;
  }

  void append_unique(std::vector<int64_t>& values, int64_t value)
  {
    if (std::ranges::find(values, value) == values.end()) {
      values.push_back(value);
    }
  }

  [[nodiscard]] nlohmann::json json_i64_array(const std::vector<int64_t>& values)
  {
    auto result = nlohmann::json::array();
    for (const auto value : values) {
      result.push_back(value);
    }
    return result;
  }

  [[nodiscard]] nlohmann::json json_optional_i64_string(const std::optional<int64_t>& value)
  { return value ? nlohmann::json(std::to_string(*value)) : nlohmann::json(); }

  [[nodiscard]] nlohmann::json json_slice(const nlohmann::json& values, size_t start, size_t count)
  {
    auto result = nlohmann::json::array();
    if (!values.is_array() || start >= values.size() || count == 0) {
      return result;
    }
    const auto end = std::min(values.size(), start + count);
    for (size_t index = start; index < end; ++index) {
      result.push_back(values[index]);
    }
    return result;
  }

  [[nodiscard]] std::vector<int64_t> collect_negative_markers(const nlohmann::json& values)
  {
    std::vector<int64_t> markers;
    if (!values.is_array()) {
      return markers;
    }
    for (const auto& value : values) {
      const auto parsed = json_to_i64(value);
      if (parsed && *parsed < 0) {
        append_unique(markers, *parsed);
      }
    }
    return markers;
  }

  [[nodiscard]] bool marker_list_contains(const std::vector<int64_t>& markers, int64_t marker)
  { return std::ranges::find(markers, marker) != markers.end(); }

  [[nodiscard]] nlohmann::json token_sample_json(const nlohmann::json& record, size_t token_index)
  {
    auto sample = nlohmann::json{{"index", token_index}, {"value", record[token_index]}};
    if (const auto parsed = json_to_i64(record[token_index])) {
      sample["exact"] = std::to_string(*parsed);
    }
    return sample;
  }

  [[nodiscard]] nlohmann::json marker_field_samples_json(const nlohmann::json& record, size_t start, size_t end)
  {
    auto samples = nlohmann::json::array();
    if (!record.is_array() || start > end || end >= record.size()) {
      return samples;
    }

    nlohmann::json current;
    for (size_t index = start; index <= end; ++index) {
      const auto token_value = json_to_i64(record[index]);
      if (token_value && *token_value < 0) {
        if (current.is_object()) {
          samples.push_back(std::move(current));
          if (samples.size() >= kMaxMarkerFields) {
            return samples;
          }
        }
        current = nlohmann::json{{"marker", *token_value}, {"values", nlohmann::json::array()}};
        continue;
      }
      if (current.is_object() && current["values"].size() < kMaxMarkerFieldValues) {
        current["values"].push_back(token_sample_json(record, index));
      }
    }

    if (current.is_object()) {
      samples.push_back(std::move(current));
    }
    return samples;
  }

  [[nodiscard]] bool is_ship_id(const EntityHints& entity_hints, int64_t value)
  { return entity_hints.is_ship_id && entity_hints.is_ship_id(value); }

  [[nodiscard]] bool is_component_id(const EntityHints& entity_hints, int64_t value)
  { return entity_hints.is_component_id && entity_hints.is_component_id(value); }

  [[nodiscard]] bool is_hull_id(const EntityHints& entity_hints, int64_t value)
  { return entity_hints.is_hull_id && entity_hints.is_hull_id(value); }

  [[nodiscard]] bool is_officer_id(const EntityHints& entity_hints, int64_t value)
  { return entity_hints.is_officer_id && entity_hints.is_officer_id(value); }

  [[nodiscard]] std::string source_category_for(const EntityHints& entity_hints, int64_t source_id)
  {
    if (is_officer_id(entity_hints, source_id)) {
      return "officer";
    }
    if (is_hull_id(entity_hints, source_id)) {
      return "ship_ability";
    }
    if (is_component_id(entity_hints, source_id)) {
      return "component";
    }
    if (is_ship_id(entity_hints, source_id)) {
      return "ship";
    }
    return "unknown";
  }

  [[nodiscard]] std::optional<int64_t> owner_ship_id_from_pre_attack_group(const nlohmann::json& record, size_t start,
                                                                           size_t end, const EntityHints& entity_hints)
  {
    if (!record.is_array() || start > end || end >= record.size()) {
      return std::nullopt;
    }

    for (size_t index = start; index + 1 <= end; ++index) {
      if (!json_matches_i64(record[index], kCombatantRefMarker)) {
        continue;
      }
      if (const auto candidate = json_to_i64(record[index + 1]); candidate && is_ship_id(entity_hints, *candidate)) {
        return candidate;
      }
    }

    for (size_t index = start; index <= end; ++index) {
      if (const auto candidate = json_to_i64(record[index]); candidate && is_ship_id(entity_hints, *candidate)) {
        return candidate;
      }
    }

    return std::nullopt;
  }

  [[nodiscard]] nlohmann::json candidate_integer_tokens_json(const nlohmann::json& record, size_t start, size_t end,
                                                             const EntityHints& entity_hints)
  {
    auto candidates = nlohmann::json::array();
    if (!record.is_array() || start > end || end >= record.size()) {
      return candidates;
    }

    for (size_t index = start; index <= end; ++index) {
      const auto token_value = json_to_i64(record[index]);
      if (!token_value || *token_value <= 0 || is_ship_id(entity_hints, *token_value)
          || is_component_id(entity_hints, *token_value)) {
        continue;
      }
      candidates.push_back(token_sample_json(record, index));
      if (candidates.size() >= kMaxCandidateIntegers) {
        break;
      }
    }

    return candidates;
  }

  [[nodiscard]] bool has_meaningful_unparsed_group_evidence(const nlohmann::json& record, size_t start, size_t end,
                                                            const EntityHints& entity_hints)
  {
    // Owner/phase-only marker prefixes describe combat-log structure, not ability/effect rows.
    return !candidate_integer_tokens_json(record, start, end, entity_hints).empty();
  }

  [[nodiscard]] nlohmann::json build_candidate_json(const nlohmann::json& record, size_t start, size_t end,
                                                    size_t payload_start, const EntityHints& entity_hints)
  {
    const auto markers         = collect_negative_markers(json_slice(record, start, end - start + 1));
    const auto owner_ship_id   = owner_ship_id_from_pre_attack_group(record, start, end, entity_hints);
    const auto raw_token_count = end - start + 1;
    return nlohmann::json{{"schema", "stfc.runtime_ability_row_candidate.v0"},
                          {"phase", marker_list_contains(markers, kRoundStartMarker) ? "round_start" : "pre_attack"},
                          {"ownerShipId", json_optional_i64_string(owner_ship_id)},
                          {"ownerShipName", nlohmann::json()},
                          {"sourceCategory", "unknown"},
                          {"sourceId", nlohmann::json()},
                          {"sourceName", nlohmann::json()},
                          {"abilityName", nlohmann::json()},
                          {"effectName", nlohmann::json()},
                          {"targetShipId", nlohmann::json()},
                          {"targetScope", nlohmann::json()},
                          {"triggered", nlohmann::json()},
                          {"opportunityCount", nlohmann::json()},
                          {"occurrenceCount", nlohmann::json()},
                          {"chance", nlohmann::json()},
                          {"value", nlohmann::json()},
                          {"durationRounds", nlohmann::json()},
                          {"stackCount", nlohmann::json()},
                          {"rawMarkers", json_i64_array(markers)},
                          {"rawTokenCount", raw_token_count},
                          {"rawTokens", json_slice(record, start, std::min(raw_token_count, kMaxRawTokenSamples))},
                          {"rawTokensTruncated", raw_token_count > kMaxRawTokenSamples},
                          {"markerFields", marker_field_samples_json(record, start, end)},
                          {"candidateIntegerTokens", candidate_integer_tokens_json(record, start, end, entity_hints)},
                          {"tokenRange", {{"recordStart", start}, {"recordEnd", end}, {"payloadStart", payload_start}}},
                          {"confidence", "experimental_pre_attack_marker_group"}};
  }

  [[nodiscard]] nlohmann::json build_marker_field_candidate_json(const nlohmann::json& record, size_t marker_index,
                                                                 size_t payload_start, int64_t marker,
                                                                 std::optional<int64_t> owner_ship_id,
                                                                 bool                   is_round_start_group,
                                                                 const EntityHints&     entity_hints)
  {
    const auto source_id       = json_to_i64(record[marker_index + 1]);
    const auto effect_id       = json_to_i64(record[marker_index + 2]);
    const auto raw_token_count = size_t{4};
    auto       candidate       = nlohmann::json{
        {"schema", "stfc.runtime_ability_row_candidate.v0"},
        {"phase", is_round_start_group ? "round_start" : "pre_attack"},
        {"ownerShipId", json_optional_i64_string(owner_ship_id)},
        {"ownerShipName", nlohmann::json()},
        {"sourceCategory", source_id ? source_category_for(entity_hints, *source_id) : "unknown"},
        {"sourceId", json_optional_i64_string(source_id)},
        {"sourceName", nlohmann::json()},
        {"abilityId", nlohmann::json()},
        {"abilityName", nlohmann::json()},
        {"effectId", json_optional_i64_string(effect_id)},
        {"effectName", nlohmann::json()},
        {"targetShipId", nlohmann::json()},
        {"targetScope", nlohmann::json()},
        {"triggered", nlohmann::json()},
        {"opportunityCount", nlohmann::json()},
        {"occurrenceCount", nlohmann::json()},
        {"chance", nlohmann::json()},
        {"value", record[marker_index + 3]},
        {"durationRounds", nlohmann::json()},
        {"stackCount", nlohmann::json()},
        {"marker", marker},
        {"markerKind", marker == kSourceValueMarker ? "source_value" : "secondary_source_value"},
        {"rawMarkers", nlohmann::json::array({marker})},
        {"rawTokenCount", raw_token_count},
        {"rawTokens", json_slice(record, marker_index, raw_token_count)},
        {"rawTokensTruncated", false},
        {"markerFields",
         nlohmann::json::array({{{"marker", marker},
                                 {"values", nlohmann::json::array({token_sample_json(record, marker_index + 1),
                                                                   token_sample_json(record, marker_index + 2),
                                                                   token_sample_json(record, marker_index + 3)})}}})},
        {"candidateIntegerTokens", nlohmann::json::array({token_sample_json(record, marker_index + 1),
                                                          token_sample_json(record, marker_index + 2)})},
        {"tokenRange",
         {{"recordStart", marker_index}, {"recordEnd", marker_index + 3}, {"payloadStart", payload_start}}},
        {"confidence", "experimental_pre_attack_marker_field"}};
    return candidate;
  }

  [[nodiscard]] nlohmann::json build_triggered_effect_candidate_json(const nlohmann::json& record,
                                                                     size_t trigger_ship_index, size_t marker_index,
                                                                     const EntityHints& entity_hints)
  {
    const auto affected_ship_id = json_to_i64(record[trigger_ship_index]);
    const auto source_id        = json_to_i64(record[marker_index + 1]);
    const auto effect_id        = json_to_i64(record[marker_index + 2]);
    return nlohmann::json{
        {"schema", "stfc.runtime_ability_row_candidate.v0"},
        {"phase", "post_attack"},
        {"ownerShipId", json_optional_i64_string(affected_ship_id)},
        {"ownerShipName", nlohmann::json()},
        {"sourceCategory", source_id ? source_category_for(entity_hints, *source_id) : "unknown"},
        {"sourceId", json_optional_i64_string(source_id)},
        {"sourceName", nlohmann::json()},
        {"abilityId", nlohmann::json()},
        {"abilityName", nlohmann::json()},
        {"effectId", json_optional_i64_string(effect_id)},
        {"effectName", nlohmann::json()},
        {"targetShipId", json_optional_i64_string(affected_ship_id)},
        {"targetScope", nlohmann::json()},
        {"triggered", true},
        {"opportunityCount", nlohmann::json()},
        {"occurrenceCount", 1},
        {"chance", nlohmann::json()},
        {"value", record[marker_index + 3]},
        {"durationRounds", nlohmann::json()},
        {"stackCount", nlohmann::json()},
        {"marker", kTriggeredEffectValueMarker},
        {"markerKind", "triggered_effect_value"},
        {"rawMarkers", nlohmann::json::array(
                           {kTriggeredEffectStartMarker, kTriggeredEffectValueMarker, kTriggeredEffectValueEndMarker})},
        {"rawTokenCount", 5},
        {"rawTokens", json_slice(record, marker_index, 5)},
        {"rawTokensTruncated", false},
        {"markerFields",
         nlohmann::json::array({{{"marker", kTriggeredEffectValueMarker},
                                 {"values", nlohmann::json::array({token_sample_json(record, marker_index + 1),
                                                                   token_sample_json(record, marker_index + 2),
                                                                   token_sample_json(record, marker_index + 3)})}}})},
        {"candidateIntegerTokens", nlohmann::json::array({token_sample_json(record, marker_index + 1),
                                                          token_sample_json(record, marker_index + 2)})},
        {"tokenRange",
         {{"recordStart", marker_index}, {"recordEnd", marker_index + 4}, {"payloadStart", marker_index}}},
        {"confidence", "experimental_triggered_effect_marker"}};
  }

  [[nodiscard]] nlohmann::json build_marker_field_candidates_json(const nlohmann::json& record, size_t start,
                                                                  size_t end, size_t payload_start,
                                                                  const EntityHints& entity_hints)
  {
    auto candidates = nlohmann::json::array();
    if (!record.is_array() || start > end || end >= record.size()) {
      return candidates;
    }

    const auto group_markers         = collect_negative_markers(json_slice(record, start, end - start + 1));
    const auto is_round_start_group  = marker_list_contains(group_markers, kRoundStartMarker);
    auto       current_owner_ship_id = owner_ship_id_from_pre_attack_group(record, start, end, entity_hints);
    for (size_t index = start; index <= end; ++index) {
      const auto marker = json_to_i64(record[index]);
      if (!marker) {
        continue;
      }

      const auto can_read_next = index + 1 <= end;
      if ((*marker == kCombatantRefMarker || *marker == kOwnerShipMarker || *marker == kSecondaryOwnerMarker
           || *marker == kSecondaryOwnerEndMarker)
          && can_read_next) {
        if (const auto owner = json_to_i64(record[index + 1]); owner && is_ship_id(entity_hints, *owner)) {
          current_owner_ship_id = owner;
        }
      }

      if ((*marker != kSourceValueMarker && *marker != kSecondaryValueMarker) || index + 3 > end) {
        continue;
      }

      const auto source_id = json_to_i64(record[index + 1]);
      const auto effect_id = json_to_i64(record[index + 2]);
      if (!source_id || !effect_id) {
        continue;
      }
      candidates.push_back(build_marker_field_candidate_json(
          record, index, payload_start, *marker, current_owner_ship_id, is_round_start_group, entity_hints));
    }

    return candidates;
  }
} // namespace

nlohmann::json BuildPreAttackCandidates(const nlohmann::json& record, size_t payload_start,
                                        const EntityHints& entity_hints)
{
  auto candidates = nlohmann::json::array();
  if (!record.is_array() || payload_start == 0 || payload_start > record.size()) {
    return candidates;
  }

  size_t group_start = 0;
  while (group_start < payload_start) {
    auto group_end = payload_start - 1;
    for (size_t index = group_start; index < payload_start; ++index) {
      if (json_matches_i64(record[index], kAttackPreludeTerminator)) {
        group_end = index;
        break;
      }
    }
    auto field_candidates =
        build_marker_field_candidates_json(record, group_start, group_end, payload_start, entity_hints);
    if (field_candidates.empty()) {
      if (has_meaningful_unparsed_group_evidence(record, group_start, group_end, entity_hints)) {
        candidates.push_back(build_candidate_json(record, group_start, group_end, payload_start, entity_hints));
      }
    } else {
      for (auto& candidate : field_candidates) {
        candidates.push_back(std::move(candidate));
      }
    }
    group_start = group_end + 1;
  }

  return candidates;
}

nlohmann::json BuildTriggeredEffectCandidates(const nlohmann::json& record, size_t search_start,
                                              const EntityHints& entity_hints)
{
  auto candidates = nlohmann::json::array();
  if (!record.is_array() || search_start >= record.size()) {
    return candidates;
  }

  size_t index = search_start;
  while (index < record.size()) {
    if (!json_matches_i64(record[index], kTriggeredEffectStartMarker) || index + 1 >= record.size()) {
      ++index;
      continue;
    }

    const auto trigger_ship_index = index + 1;
    index += 2;
    while (index < record.size()) {
      if (json_matches_i64(record[index], kTriggeredEffectTerminator)) {
        ++index;
        break;
      }
      if (!json_matches_i64(record[index], kTriggeredEffectValueMarker) || index + 4 >= record.size()) {
        ++index;
        continue;
      }
      if (!json_matches_i64(record[index + 4], kTriggeredEffectValueEndMarker)) {
        ++index;
        continue;
      }
      candidates.push_back(build_triggered_effect_candidate_json(record, trigger_ship_index, index, entity_hints));
      index += 5;
    }
  }

  return candidates;
}

void AddCombatContext(nlohmann::json& candidates, size_t segment_index, size_t record_index, size_t round,
                      size_t sub_round)
{
  if (!candidates.is_array()) {
    return;
  }

  for (auto& candidate : candidates) {
    if (!candidate.is_object()) {
      continue;
    }
    candidate["round"]          = round;
    candidate["subRound"]       = sub_round;
    auto token_range            = candidate.contains("tokenRange") && candidate["tokenRange"].is_object()
                                      ? candidate["tokenRange"]
                                      : nlohmann::json::object();
    token_range["segmentIndex"] = segment_index;
    token_range["recordIndex"]  = record_index;
    candidate["tokenRange"]     = std::move(token_range);
  }
}

nlohmann::json CollectFromAttackRows(const nlohmann::json& attack_rows, const std::string& battle_id)
{
  auto candidates = nlohmann::json::array();
  if (!attack_rows.is_array()) {
    return candidates;
  }

  for (const auto& attack : attack_rows) {
    if (!attack.is_object() || !attack.contains("runtimeAbilityRowCandidates")
        || !attack["runtimeAbilityRowCandidates"].is_array()) {
      continue;
    }
    for (auto candidate : attack["runtimeAbilityRowCandidates"]) {
      if (candidate.is_object()) {
        candidate["battleId"] = battle_id;
      }
      candidates.push_back(std::move(candidate));
    }
  }

  return candidates;
}
} // namespace battle_runtime_ability_candidates
