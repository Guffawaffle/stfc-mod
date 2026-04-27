/**
 * @file battle_log_decoder.cc
 * @brief Pure battle journal token-stream decoder used by runtime probes and AX tools.
 */
#include "patches/battle_log_decoder.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace battle_log_decoder
{
namespace
{
constexpr int64_t kSegmentTerminator = -89;
constexpr int64_t kRecordTerminator = -99;
constexpr int64_t kRoundStartMarker = -96;
constexpr int64_t kComponentRefMarker = -98;
constexpr int64_t kScalarMarker = -95;
constexpr int64_t kAttackPreludeTerminator = -83;
constexpr int64_t kTriggeredEffectStartMarker = -93;
constexpr int64_t kTriggeredEffectShipMarker = -91;
constexpr int64_t kTriggeredEffectEndMarker = -92;
constexpr int64_t kTriggeredEffectTerminator = -94;

struct ParticipantInfo {
  std::string          side;
  std::string          uid;
  std::string          name;
  std::string          display_name;
  std::string          display_name_source;
  std::string          participant_kind;
  std::optional<int64_t> fleet_id;
  std::optional<int64_t> fleet_type;
  std::optional<int64_t> ship_level;
  std::vector<int64_t> ship_ids;
  std::vector<int64_t> hull_ids;
  std::vector<int64_t> component_ids;
  std::optional<double> offense_rating;
  std::optional<double> defense_rating;
  std::optional<double> officer_rating;
};

struct EntityIndex {
  std::vector<ParticipantInfo>         participants;
  std::unordered_map<int64_t, size_t>  ship_to_participant;
  std::unordered_map<int64_t, int64_t> component_to_ship;
  std::unordered_set<int64_t>          ship_ids;
  std::unordered_set<int64_t>          component_ids;
  std::unordered_set<std::string>      participant_keys;
};

struct DerivedCombatAnalytics {
  nlohmann::json rounds = nlohmann::json::array();
  nlohmann::json attack_rows = nlohmann::json::array();
};

[[nodiscard]] const nlohmann::json& journal_from_probe_or_journal(const nlohmann::json& probe_or_journal)
{
  if (probe_or_journal.is_object() && probe_or_journal.contains("journal") && probe_or_journal["journal"].is_object()) {
    return probe_or_journal["journal"];
  }

  return probe_or_journal;
}

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
      const auto text = value.get<std::string>();
      size_t     consumed = 0;
      const auto parsed = std::stoll(text, &consumed, 10);
      if (consumed == text.size()) {
        return parsed;
      }
    }
  } catch (...) {
  }

  return std::nullopt;
}

[[nodiscard]] std::optional<double> json_to_double(const nlohmann::json& value)
{
  try {
    if (value.is_number()) {
      return value.get<double>();
    }

    if (value.is_string()) {
      const auto text = value.get<std::string>();
      size_t     consumed = 0;
      const auto parsed = std::stod(text, &consumed);
      if (consumed == text.size()) {
        return parsed;
      }
    }
  } catch (...) {
  }

  return std::nullopt;
}

[[nodiscard]] bool json_number_equals(const nlohmann::json& value, double expected)
{
  const auto parsed = json_to_double(value);
  return parsed && std::abs(*parsed - expected) < 1e-9;
}

[[nodiscard]] bool json_matches_i64(const nlohmann::json& value, int64_t expected)
{
  const auto parsed = json_to_i64(value);
  return parsed && *parsed == expected;
}

[[nodiscard]] uint64_t json_to_u64_or_zero(const nlohmann::json& value)
{
  try {
    if (value.is_number_unsigned()) {
      return value.get<uint64_t>();
    }

    if (value.is_number_integer()) {
      const auto signed_value = value.get<int64_t>();
      return signed_value >= 0 ? static_cast<uint64_t>(signed_value) : uint64_t{0};
    }

    if (value.is_string()) {
      return static_cast<uint64_t>(std::stoull(value.get<std::string>()));
    }
  } catch (...) {
  }

  return 0;
}

[[nodiscard]] std::string json_id_to_string(const nlohmann::json& value)
{
  if (value.is_string()) {
    return value.get<std::string>();
  }

  if (value.is_number_integer() || value.is_number_unsigned()) {
    return value.dump();
  }

  return {};
}

[[nodiscard]] std::string trim_copy(std::string value)
{
  const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };

  value.erase(value.begin(), std::ranges::find_if_not(value, is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
  return value;
}

[[nodiscard]] std::string to_lower_ascii(std::string value)
{
  for (auto& character : value) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return value;
}

[[nodiscard]] bool is_placeholder_display_name(const std::string& value)
{
  const auto normalized = to_lower_ascii(trim_copy(value));
  return normalized.empty() || normalized == "--" || normalized == "retrieving..." || normalized == "retrieving";
}

[[nodiscard]] std::vector<int64_t> read_int_values(const nlohmann::json& value)
{
  std::vector<int64_t> values;

  if (value.is_array()) {
    values.reserve(value.size());
    for (const auto& entry : value) {
      if (auto parsed = json_to_i64(entry)) {
        values.push_back(*parsed);
      }
    }
    return values;
  }

  if (auto parsed = json_to_i64(value)) {
    values.push_back(*parsed);
    return values;
  }

  if (value.is_string()) {
    std::stringstream stream(value.get<std::string>());
    int64_t           parsed = 0;
    while (stream >> parsed) {
      values.push_back(parsed);
    }
  }

  return values;
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

[[nodiscard]] nlohmann::json json_i64_string_array(const std::vector<int64_t>& values)
{
  auto result = nlohmann::json::array();
  for (const auto value : values) {
    result.push_back(std::to_string(value));
  }
  return result;
}

[[nodiscard]] nlohmann::json json_token_string_array(const nlohmann::json& values)
{
  auto result = nlohmann::json::array();
  if (!values.is_array()) {
    return result;
  }

  for (const auto& value : values) {
    if (value.is_string()) {
      result.push_back(value.get<std::string>());
      continue;
    }
    result.push_back(value.dump());
  }
  return result;
}

[[nodiscard]] nlohmann::json lossless_integer_json(const nlohmann::json& value)
{
  if (value.is_object()) {
    auto result = nlohmann::json::object();
    for (const auto& [key, entry] : value.items()) {
      result[key] = lossless_integer_json(entry);
    }
    return result;
  }

  if (value.is_array()) {
    auto result = nlohmann::json::array();
    for (const auto& entry : value) {
      result.push_back(lossless_integer_json(entry));
    }
    return result;
  }

  if (value.is_number_integer() || value.is_number_unsigned()) {
    return json_id_to_string(value);
  }

  return value;
}

[[nodiscard]] nlohmann::json json_slice(const nlohmann::json& values, size_t start, size_t count)
{
  auto result = nlohmann::json::array();
  if (!values.is_array() || values.empty() || start >= values.size() || count == 0) {
    return result;
  }

  const auto end = std::min(values.size(), start + count);
  for (size_t index = start; index < end; ++index) {
    result.push_back(values[index]);
  }
  return result;
}

[[nodiscard]] nlohmann::json json_tail(const nlohmann::json& values, size_t count)
{
  if (!values.is_array() || values.empty() || count == 0) {
    return nlohmann::json::array();
  }

  const auto start = values.size() > count ? values.size() - count : size_t{0};
  return json_slice(values, start, count);
}

[[nodiscard]] std::string participant_name_for_uid(const nlohmann::json& names, const std::string& uid)
{
  if (uid.empty() || !names.is_object() || !names.contains(uid) || !names[uid].is_object()) {
    return {};
  }

  auto name = names[uid].value("name", std::string{});
  return is_placeholder_display_name(name) ? std::string{} : trim_copy(std::move(name));
}

[[nodiscard]] std::optional<int64_t> parse_object_key_i64(const std::string& key)
{
  try {
    size_t consumed = 0;
    const auto value = std::stoll(key, &consumed, 10);
    if (consumed == key.size()) {
      return value;
    }
  } catch (...) {
  }

  return std::nullopt;
}

[[nodiscard]] std::optional<int64_t> first_ship_level(const nlohmann::json& fleet, const std::vector<int64_t>& ship_ids)
{
  if (!fleet.contains("ship_levels") || !fleet["ship_levels"].is_object()) {
    return std::nullopt;
  }

  const auto& ship_levels = fleet["ship_levels"];
  for (const auto ship_id : ship_ids) {
    const auto ship_key = std::to_string(ship_id);
    if (ship_levels.contains(ship_key)) {
      return json_to_i64(ship_levels[ship_key]);
    }
  }

  for (const auto& [_ship_id, level] : ship_levels.items()) {
    if (auto parsed = json_to_i64(level)) {
      return parsed;
    }
  }

  return std::nullopt;
}

[[nodiscard]] std::optional<int64_t> hostile_level_from_uid(const std::string& uid)
{
  if (!uid.starts_with("mar_")) {
    return std::nullopt;
  }

  const auto separator = uid.find_last_of('_');
  if (separator == std::string::npos || separator + 1 >= uid.size()) {
    return std::nullopt;
  }

  try {
    size_t consumed = 0;
    const auto level = std::stoll(uid.substr(separator + 1), &consumed, 10);
    if (consumed == uid.size() - separator - 1) {
      return level;
    }
  } catch (...) {
  }

  return std::nullopt;
}

[[nodiscard]] bool is_hostile_uid(const std::string& uid)
{
  return uid.starts_with("mar_");
}

[[nodiscard]] std::string primary_reward_name_key(const nlohmann::json& journal)
{
  if (!journal.contains("chest_drop") || !journal["chest_drop"].is_object()) {
    return {};
  }

  const auto& chest_drop = journal["chest_drop"];
  auto loot_roll_key = chest_drop.value("loot_roll_key", std::string{});
  if (!loot_roll_key.empty()) {
    return loot_roll_key;
  }

  if (!chest_drop.contains("chests_gained") || !chest_drop["chests_gained"].is_array()) {
    return {};
  }

  for (const auto& chest : chest_drop["chests_gained"]) {
    if (!chest.is_object() || !chest.contains("params") || !chest["params"].is_object()) {
      continue;
    }

    auto chest_name = chest["params"].value("chest_name", std::string{});
    if (!chest_name.empty()) {
      return chest_name;
    }
  }

  return {};
}

[[nodiscard]] std::vector<std::string> split_key_tokens(const std::string& key)
{
  std::vector<std::string> tokens;
  std::stringstream        stream(key);
  std::string              token;
  while (std::getline(stream, token, '_')) {
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

[[nodiscard]] bool is_numeric_token(const std::string& token)
{
  return !token.empty() && std::ranges::all_of(token, [](unsigned char character) {
           return std::isdigit(character) != 0;
         });
}

[[nodiscard]] bool is_reward_noise_token(const std::string& token)
{
  const auto normalized = to_lower_ascii(token);
  return normalized == "common" || normalized == "uncommon" || normalized == "rare" || normalized == "epic"
         || normalized == "leader";
}

[[nodiscard]] std::string join_words(const std::vector<std::string>& words)
{
  std::string result;
  for (const auto& word : words) {
    if (!result.empty()) {
      result += ' ';
    }
    result += word;
  }
  return result;
}

[[nodiscard]] std::string with_level_prefix(const std::string& label, std::optional<int64_t> level)
{
  if (!level || label.empty()) {
    return label;
  }

  return "Lv." + std::to_string(*level) + " " + label;
}

[[nodiscard]] std::string derive_hostile_label_from_key(const std::string& key, std::optional<int64_t> level)
{
  const auto tokens = split_key_tokens(key);
  if (tokens.empty()) {
    return {};
  }

  size_t start_index = 0;
  if (tokens[0] == "MAR") {
    start_index = tokens.size() > 1 && is_numeric_token(tokens[1]) ? 2 : 1;
  }

  bool saw_armada = false;
  bool saw_cardassian_armada = false;
  std::vector<std::string> words;
  for (size_t index = start_index; index < tokens.size(); ++index) {
    const auto& token = tokens[index];
    if (is_reward_noise_token(token)) {
      continue;
    }

    if (token == "Armada") {
      saw_armada = true;
    }
    if (token == "Car") {
      saw_cardassian_armada = true;
      continue;
    }

    words.push_back(token);
  }

  if (saw_armada) {
    return saw_cardassian_armada ? std::string{"Central Command Station"} : with_level_prefix("Armada Target", level);
  }

  return with_level_prefix(join_words(words), level);
}

void populate_participant_display_fields(ParticipantInfo& participant, const nlohmann::json& journal)
{
  const auto hostile = is_hostile_uid(participant.uid);
  participant.participant_kind = hostile ? "hostile" : (!participant.name.empty() ? "player" : "unknown");

  if (!participant.name.empty()) {
    participant.display_name = participant.name;
    participant.display_name_source = "names";
    return;
  }

  const auto level = participant.ship_level ? participant.ship_level : hostile_level_from_uid(participant.uid);
  if (hostile) {
    participant.display_name = derive_hostile_label_from_key(primary_reward_name_key(journal), level);
    if (!participant.display_name.empty()) {
      participant.display_name_source = "derived_hostile_key";
      return;
    }

    participant.display_name = with_level_prefix("Hostile", level);
    participant.display_name_source = "derived_hostile_uid";
    return;
  }

  if (!participant.hull_ids.empty()) {
    participant.display_name = "Hull#" + std::to_string(participant.hull_ids.front());
    participant.display_name_source = "hull_id";
  }
}

void add_participant(EntityIndex& index, const nlohmann::json& names, const nlohmann::json& journal,
                     const nlohmann::json& fleet, std::string side)
{
  if (!fleet.is_object()) {
    return;
  }

  ParticipantInfo participant;
  participant.side = std::move(side);
  participant.uid = fleet.value("uid", std::string{});
  participant.name = participant_name_for_uid(names, participant.uid);

  if (fleet.contains("fleet_id")) {
    participant.fleet_id = json_to_i64(fleet["fleet_id"]);
  }
  if (fleet.contains("type")) {
    participant.fleet_type = json_to_i64(fleet["type"]);
  }
  if (fleet.contains("ship_ids")) {
    participant.ship_ids = read_int_values(fleet["ship_ids"]);
  }
  if (fleet.contains("hull_ids")) {
    participant.hull_ids = read_int_values(fleet["hull_ids"]);
  }
  participant.ship_level = first_ship_level(fleet, participant.ship_ids);
  if (fleet.contains("offense_rating")) {
    participant.offense_rating = json_to_double(fleet["offense_rating"]);
  }
  if (fleet.contains("defense_rating")) {
    participant.defense_rating = json_to_double(fleet["defense_rating"]);
  }
  if (fleet.contains("officer_rating")) {
    participant.officer_rating = json_to_double(fleet["officer_rating"]);
  }

  const auto key = participant.side + ":" + (participant.fleet_id ? std::to_string(*participant.fleet_id) : participant.uid);
  if (!index.participant_keys.insert(key).second) {
    return;
  }

  const auto participant_index = index.participants.size();

  for (const auto ship_id : participant.ship_ids) {
    index.ship_ids.insert(ship_id);
    index.ship_to_participant.emplace(ship_id, participant_index);
  }

  if (fleet.contains("ship_components") && fleet["ship_components"].is_object()) {
    for (const auto& [ship_key, component_value] : fleet["ship_components"].items()) {
      const auto parsed_ship_id = parse_object_key_i64(ship_key);
      const auto components = read_int_values(component_value);
      for (const auto component_id : components) {
        if (component_id < 0) {
          continue;
        }
        append_unique(participant.component_ids, component_id);
        index.component_ids.insert(component_id);
        if (parsed_ship_id) {
          index.component_to_ship.emplace(component_id, *parsed_ship_id);
        }
      }
    }
  }

  populate_participant_display_fields(participant, journal);

  index.participants.push_back(std::move(participant));
}

void collect_fleet_data(EntityIndex& index, const nlohmann::json& names, const nlohmann::json& journal,
                        const nlohmann::json& fleet_data, std::string_view side)
{
  if (!fleet_data.is_object()) {
    return;
  }

  bool found_nested_fleet = false;

  if (fleet_data.contains("deployed_fleets") && fleet_data["deployed_fleets"].is_object()) {
    for (const auto& [_fleet_id, fleet] : fleet_data["deployed_fleets"].items()) {
      add_participant(index, names, journal, fleet, std::string(side));
      found_nested_fleet = true;
    }
  }

  if (fleet_data.contains("deployed_fleet") && fleet_data["deployed_fleet"].is_object()) {
    add_participant(index, names, journal, fleet_data["deployed_fleet"], std::string(side));
    found_nested_fleet = true;
  }

  if (!found_nested_fleet && (fleet_data.contains("ship_ids") || fleet_data.contains("fleet_id"))) {
    add_participant(index, names, journal, fleet_data, std::string(side));
  }
}

[[nodiscard]] EntityIndex build_entity_index(const nlohmann::json& journal, const nlohmann::json& names)
{
  EntityIndex index;
  if (journal.contains("initiator_fleet_data")) {
    collect_fleet_data(index, names, journal, journal["initiator_fleet_data"], "initiator");
  }
  if (journal.contains("target_fleet_data")) {
    collect_fleet_data(index, names, journal, journal["target_fleet_data"], "target");
  }
  return index;
}

[[nodiscard]] nlohmann::json participants_to_json(const EntityIndex& index)
{
  auto participants = nlohmann::json::array();
  for (const auto& participant : index.participants) {
    auto entry = nlohmann::json{{"side", participant.side},
                                {"uid", participant.uid},
                                {"name", participant.name},
                                {"display_name", participant.display_name},
                                {"display_name_source", participant.display_name_source},
                                {"participant_kind", participant.participant_kind},
                                {"ship_ids", json_i64_array(participant.ship_ids)},
                                {"hull_ids", json_i64_array(participant.hull_ids)},
                                {"component_ids", json_i64_array(participant.component_ids)}};

    entry["fleet_id"] = participant.fleet_id ? nlohmann::json(*participant.fleet_id) : nlohmann::json();
    entry["fleet_type"] = participant.fleet_type ? nlohmann::json(*participant.fleet_type) : nlohmann::json();
    entry["ship_level"] = participant.ship_level ? nlohmann::json(*participant.ship_level) : nlohmann::json();
    entry["offense_rating"] = participant.offense_rating ? nlohmann::json(*participant.offense_rating) : nlohmann::json();
    entry["defense_rating"] = participant.defense_rating ? nlohmann::json(*participant.defense_rating) : nlohmann::json();
    entry["officer_rating"] = participant.officer_rating ? nlohmann::json(*participant.officer_rating) : nlohmann::json();
    participants.push_back(std::move(entry));
  }
  return participants;
}

[[nodiscard]] nlohmann::json participants_to_capture_json(const EntityIndex& index)
{
  auto participants = nlohmann::json::array();
  for (const auto& participant : index.participants) {
    auto entry = nlohmann::json{{"side", participant.side},
                                {"uid", participant.uid},
                                {"name", participant.name},
                                {"displayName", participant.display_name},
                                {"displayNameSource", participant.display_name_source},
                                {"participantKind", participant.participant_kind},
                                {"shipIds", json_i64_string_array(participant.ship_ids)},
                                {"hullIds", json_i64_string_array(participant.hull_ids)},
                                {"componentIds", json_i64_string_array(participant.component_ids)}};

    entry["fleetId"] = participant.fleet_id ? nlohmann::json(std::to_string(*participant.fleet_id)) : nlohmann::json();
    entry["fleetType"] = participant.fleet_type ? nlohmann::json(*participant.fleet_type) : nlohmann::json();
    entry["shipLevel"] = participant.ship_level ? nlohmann::json(*participant.ship_level) : nlohmann::json();
    entry["offenseRating"] = participant.offense_rating ? nlohmann::json(*participant.offense_rating) : nlohmann::json();
    entry["defenseRating"] = participant.defense_rating ? nlohmann::json(*participant.defense_rating) : nlohmann::json();
    entry["officerRating"] = participant.officer_rating ? nlohmann::json(*participant.officer_rating) : nlohmann::json();
    participants.push_back(std::move(entry));
  }
  return participants;
}

[[nodiscard]] const ParticipantInfo* participant_for_ship_id(const EntityIndex& index, int64_t ship_id)
{
  const auto found = index.ship_to_participant.find(ship_id);
  if (found == index.ship_to_participant.end() || found->second >= index.participants.size()) {
    return nullptr;
  }
  return &index.participants[found->second];
}

[[nodiscard]] nlohmann::json build_ship_ref_json(const EntityIndex& index, const std::optional<int64_t>& ship_id)
{
  if (!ship_id) {
    return nlohmann::json();
  }

  auto result = nlohmann::json{{"shipId", *ship_id}};
  const auto* participant = participant_for_ship_id(index, *ship_id);
  if (participant == nullptr) {
    return result;
  }

  result["uid"] = participant->uid;
  result["displayName"] = participant->display_name;
  result["participantKind"] = participant->participant_kind;
  result["side"] = participant->side;
  result["fleetId"] = participant->fleet_id ? nlohmann::json(*participant->fleet_id) : nlohmann::json();
  result["shipLevel"] = participant->ship_level ? nlohmann::json(*participant->ship_level) : nlohmann::json();
  result["hullIds"] = json_i64_array(participant->hull_ids);
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
    if (!parsed || *parsed >= 0) {
      continue;
    }
    append_unique(markers, *parsed);
  }

  return markers;
}

[[nodiscard]] bool marker_list_contains(const std::vector<int64_t>& markers, int64_t marker)
{
  return std::ranges::find(markers, marker) != markers.end();
}

[[nodiscard]] nlohmann::json marker_hints()
{
  return nlohmann::json{{"-99", "record_or_subrecord_end_candidate"},
                        {"-98", "component_or_hardpoint_ref_candidate"},
                        {"-97", "stream_or_block_end_candidate"},
                        {"-96", "stream_or_block_start_candidate"},
                        {"-95", "scalar_or_share_candidate"},
                        {"-94", "trigger_or_followup_end_candidate"},
                        {"-93", "trigger_or_followup_start_candidate"},
                        {"-92", "trigger_or_followup_value_end_candidate"},
                        {"-91", "trigger_or_followup_ref_candidate"},
                        {"-90", "segment_or_event_start_candidate"},
                        {"-89", "segment_terminator_observed"},
                        {"-88", "combatant_ref_candidate"},
                        {"-80", "special_block_start_candidate"},
                        {"-79", "special_block_value_candidate"}};
}

[[nodiscard]] nlohmann::json build_signature(const nlohmann::json& battle_log, const DecodeOptions& options)
{
  auto negative_tokens = std::vector<int64_t>{};
  auto seen_negative_tokens = std::unordered_set<int64_t>{};
  auto segment_lengths = std::unordered_map<size_t, size_t>{};
  size_t segment_start = 0;
  size_t integer_count = 0;
  size_t float_count = 0;
  size_t zero_count = 0;

  if (battle_log.is_array()) {
    for (size_t index = 0; index < battle_log.size(); ++index) {
      const auto& token = battle_log[index];
      if (token.is_number_float()) {
        ++float_count;
      }

      if (!token.is_number_integer() && !token.is_number_unsigned()) {
        continue;
      }

      ++integer_count;
      const auto value = json_to_i64(token);
      if (!value) {
        continue;
      }

      if (*value == 0) {
        ++zero_count;
      }

      if (*value < 0 && seen_negative_tokens.insert(*value).second) {
        negative_tokens.push_back(*value);
      }

      if (*value == kSegmentTerminator) {
        ++segment_lengths[index - segment_start + 1];
        segment_start = index + 1;
      }
    }
  }

  std::vector<std::pair<size_t, size_t>> ranked_lengths;
  ranked_lengths.reserve(segment_lengths.size());
  for (const auto& [length, frequency] : segment_lengths) {
    ranked_lengths.emplace_back(length, frequency);
  }
  std::ranges::sort(ranked_lengths, [](const auto& lhs, const auto& rhs) {
    if (lhs.second != rhs.second) {
      return lhs.second > rhs.second;
    }
    return lhs.first < rhs.first;
  });

  auto top_lengths = nlohmann::json::array();
  const auto top_limit = options.top_segment_length_limit == 0 ? ranked_lengths.size()
                                                              : std::min(options.top_segment_length_limit, ranked_lengths.size());
  for (size_t index = 0; index < top_limit; ++index) {
    top_lengths.push_back({{"length", ranked_lengths[index].first}, {"count", ranked_lengths[index].second}});
  }

  return nlohmann::json{{"token_count", battle_log.is_array() ? battle_log.size() : size_t{0}},
                        {"integer_count", integer_count},
                        {"float_count", float_count},
                        {"zero_count", zero_count},
                        {"first_token", battle_log.is_array() && !battle_log.empty() ? battle_log.front() : nlohmann::json()},
                        {"last_token", battle_log.is_array() && !battle_log.empty() ? battle_log.back() : nlohmann::json()},
                        {"first_tokens", json_slice(battle_log, 0, options.edge_token_count)},
                        {"last_tokens", json_tail(battle_log, options.edge_token_count)},
                        {"negative_tokens", json_i64_array(negative_tokens)},
                        {"segment_count", segment_lengths.size() == 0 ? size_t{0}
                                                                       : std::accumulate(segment_lengths.begin(), segment_lengths.end(), size_t{0},
                                                                                         [](size_t sum, const auto& entry) { return sum + entry.second; })},
                        {"top_segment_lengths", top_lengths}};
}

[[nodiscard]] nlohmann::json build_segment_json(const nlohmann::json& battle_log, size_t segment_index,
                                                size_t start, size_t end, const EntityIndex& entity_index,
                                                const DecodeOptions& options)
{
  std::vector<int64_t> markers;
  std::vector<int64_t> ship_ids;
  std::vector<int64_t> component_ids;
  std::vector<int64_t> unknown_positive_ids;
  std::unordered_set<int64_t> seen_unknown_positive_ids;
  size_t zero_count = 0;

  for (size_t token_index = start; token_index <= end && token_index < battle_log.size(); ++token_index) {
    const auto token_value = json_to_i64(battle_log[token_index]);
    if (!token_value) {
      continue;
    }

    if (*token_value < 0) {
      append_unique(markers, *token_value);
      continue;
    }

    if (*token_value == 0) {
      ++zero_count;
    }

    if (entity_index.ship_ids.contains(*token_value)) {
      append_unique(ship_ids, *token_value);
      continue;
    }

    if (entity_index.component_ids.contains(*token_value)) {
      append_unique(component_ids, *token_value);
      continue;
    }

    if (*token_value > 0 && seen_unknown_positive_ids.insert(*token_value).second && unknown_positive_ids.size() < 12) {
      unknown_positive_ids.push_back(*token_value);
    }
  }

  auto component_refs = nlohmann::json::array();
  for (const auto component_id : component_ids) {
    const auto ship = entity_index.component_to_ship.find(component_id);
    component_refs.push_back({{"component_id", component_id},
                              {"ship_id", ship != entity_index.component_to_ship.end() ? nlohmann::json(ship->second)
                                                                                          : nlohmann::json()}});
  }

  const auto length = end >= start ? end - start + 1 : size_t{0};
  return nlohmann::json{{"index", segment_index},
                        {"start", start},
                        {"end", end},
                        {"length", length},
                        {"terminated", battle_log[end].is_number_integer() && battle_log[end].get<int64_t>() == kSegmentTerminator},
                        {"first_token", length > 0 ? battle_log[start] : nlohmann::json()},
                        {"last_token", length > 0 ? battle_log[end] : nlohmann::json()},
                        {"first_tokens", json_slice(battle_log, start, options.edge_token_count)},
                        {"last_tokens", length > options.edge_token_count ? json_slice(battle_log, end + 1 - options.edge_token_count, options.edge_token_count)
                                                                            : json_slice(battle_log, start, length)},
                        {"markers", json_i64_array(markers)},
                        {"ship_ids", json_i64_array(ship_ids)},
                        {"component_refs", component_refs},
                        {"zero_count", zero_count},
                        {"unknown_positive_integer_sample", json_i64_array(unknown_positive_ids)}};
}

[[nodiscard]] std::optional<size_t> find_attack_payload_start(const nlohmann::json& record)
{
  if (!record.is_array() || record.size() < 16) {
    return std::nullopt;
  }

  for (size_t index = 0; index + 15 < record.size(); ++index) {
    const auto attacker_ship_id = json_to_i64(record[index]);
    const auto component_id = json_to_i64(record[index + 2]);
    const auto target_ship_id = json_to_i64(record[index + 3]);
    const auto critical_flag = json_to_i64(record[index + 7]);
    const auto hull_damage = json_to_i64(record[index + 8]);
    const auto shield_damage = json_to_i64(record[index + 10]);

    if (!json_matches_i64(record[index + 1], kComponentRefMarker) || !component_id || !attacker_ship_id || !target_ship_id
        || !json_number_equals(record[index + 4], 1.0) || !json_number_equals(record[index + 5], 0.0)
        || !json_matches_i64(record[index + 6], 1) || !critical_flag || (*critical_flag != 0 && *critical_flag != 1)
        || !hull_damage || !shield_damage) {
      continue;
    }

    return index;
  }

  return std::nullopt;
}

[[nodiscard]] nlohmann::json build_triggered_effects_json(const nlohmann::json& record, size_t start,
                                                          const EntityIndex& entity_index)
{
  auto effects = nlohmann::json::array();
  if (!record.is_array() || start >= record.size()) {
    return effects;
  }

  for (size_t index = start; index + 7 < record.size(); ++index) {
    if (!json_matches_i64(record[index], kTriggeredEffectStartMarker)
        || !json_matches_i64(record[index + 2], kTriggeredEffectShipMarker)
        || !json_matches_i64(record[index + 6], kTriggeredEffectEndMarker)
        || !json_matches_i64(record[index + 7], kTriggeredEffectTerminator)) {
      continue;
    }

    const auto ship_id = json_to_i64(record[index + 1]);
    effects.push_back({{"shipId", ship_id ? nlohmann::json(*ship_id) : nlohmann::json()},
                       {"ship", build_ship_ref_json(entity_index, ship_id)},
                       {"refA", record[index + 3]},
                       {"refB", record[index + 4]},
                       {"value", record[index + 5]}});
    index += 7;
  }

  return effects;
}

[[nodiscard]] nlohmann::json empty_record_summary_json()
{
  return nlohmann::json{{"recordCount", 0},
                        {"attackCount", 0},
                        {"criticalCount", 0},
                        {"componentScalarCount", 0},
                        {"opaqueCount", 0},
                        {"triggeredEffectCount", 0},
                        {"hullDamageTotal", 0.0},
                        {"shieldDamageTotal", 0.0},
                        {"mitigatedDamageTotal", 0.0},
                        {"totalIsolyticDamageTotal", 0.0}};
}

void merge_record_summary(nlohmann::json& destination, const nlohmann::json& source)
{
  destination["recordCount"] = destination.value("recordCount", 0) + source.value("recordCount", 0);
  destination["attackCount"] = destination.value("attackCount", 0) + source.value("attackCount", 0);
  destination["criticalCount"] = destination.value("criticalCount", 0) + source.value("criticalCount", 0);
  destination["componentScalarCount"] = destination.value("componentScalarCount", 0) + source.value("componentScalarCount", 0);
  destination["opaqueCount"] = destination.value("opaqueCount", 0) + source.value("opaqueCount", 0);
  destination["triggeredEffectCount"] = destination.value("triggeredEffectCount", 0) + source.value("triggeredEffectCount", 0);
  destination["hullDamageTotal"] = destination.value("hullDamageTotal", 0.0) + source.value("hullDamageTotal", 0.0);
  destination["shieldDamageTotal"] = destination.value("shieldDamageTotal", 0.0) + source.value("shieldDamageTotal", 0.0);
  destination["mitigatedDamageTotal"] = destination.value("mitigatedDamageTotal", 0.0) + source.value("mitigatedDamageTotal", 0.0);
  destination["totalIsolyticDamageTotal"] = destination.value("totalIsolyticDamageTotal", 0.0)
                                               + source.value("totalIsolyticDamageTotal", 0.0);
}

[[nodiscard]] nlohmann::json build_record_json(const nlohmann::json& record, size_t record_index,
                                               const EntityIndex& entity_index, const DecodeOptions& options)
{
  std::vector<int64_t> markers;
  std::vector<int64_t> ship_ids;
  std::vector<int64_t> component_ids;
  size_t zero_count = 0;

  if (record.is_array()) {
    for (const auto& token : record) {
      const auto token_value = json_to_i64(token);
      if (!token_value) {
        continue;
      }

      if (*token_value < 0) {
        append_unique(markers, *token_value);
        continue;
      }

      if (*token_value == 0) {
        ++zero_count;
      }

      if (entity_index.ship_ids.contains(*token_value)) {
        append_unique(ship_ids, *token_value);
        continue;
      }

      if (entity_index.component_ids.contains(*token_value)) {
        append_unique(component_ids, *token_value);
      }
    }
  }

  auto result = nlohmann::json{{"index", record_index},
                               {"length", record.is_array() ? record.size() : size_t{0}},
                               {"firstTokens", json_slice(record, 0, options.edge_token_count)},
                               {"lastTokens", json_tail(record, options.edge_token_count)},
                               {"markers", json_i64_array(markers)},
                               {"shipIds", json_i64_array(ship_ids)},
                               {"componentIds", json_i64_array(component_ids)},
                               {"zeroCount", zero_count}};

  if (record.is_array() && record.size() == 6 && json_matches_i64(record[1], kComponentRefMarker)
      && json_matches_i64(record[3], kScalarMarker)) {
    const auto ship_id = json_to_i64(record[0]);
    result["kind"] = "component_scalar";
    result["shipId"] = ship_id ? nlohmann::json(*ship_id) : nlohmann::json();
    result["ship"] = build_ship_ref_json(entity_index, ship_id);
    result["componentId"] = record[2];
    result["scalar"] = record[4];
    return result;
  }

  const auto attack_payload_start = find_attack_payload_start(record);
  if (!attack_payload_start) {
    result["kind"] = "opaque";
    return result;
  }

  const auto payload_index = *attack_payload_start;
  const auto attacker_ship_id = json_to_i64(record[payload_index]);
  const auto component_id = json_to_i64(record[payload_index + 2]);
  const auto target_ship_id = json_to_i64(record[payload_index + 3]);
  const auto critical_flag = json_to_i64(record[payload_index + 7]);
  const auto triggered_effects = build_triggered_effects_json(record, payload_index + 16, entity_index);

  result["kind"] = "attack";
  result["payloadStart"] = payload_index;
  result["preAttackTokenCount"] = payload_index;
  result["preAttackMarkers"] = json_i64_array(collect_negative_markers(json_slice(record, 0, payload_index)));
  result["attackerShipId"] = attacker_ship_id ? nlohmann::json(*attacker_ship_id) : nlohmann::json();
  result["targetShipId"] = target_ship_id ? nlohmann::json(*target_ship_id) : nlohmann::json();
  result["attacker"] = build_ship_ref_json(entity_index, attacker_ship_id);
  result["target"] = build_ship_ref_json(entity_index, target_ship_id);
  result["componentId"] = component_id ? nlohmann::json(*component_id) : nlohmann::json();
  result["critical"] = critical_flag ? nlohmann::json(*critical_flag == 1) : nlohmann::json(false);
  result["damage"] = nlohmann::json{{"hull", record[payload_index + 8]},
                                     {"targetHullRemaining", record[payload_index + 9]},
                                     {"shield", record[payload_index + 10]},
                                     {"targetShieldRemaining", record[payload_index + 11]},
                                     {"mitigated", record[payload_index + 12]},
                                     {"totalIsolytic", record[payload_index + 13]},
                                     {"unknownScalarA", record[payload_index + 14]},
                                     {"unknownScalarB", record[payload_index + 15]}};
  result["triggeredEffects"] = triggered_effects;
  result["triggeredEffectCount"] = triggered_effects.size();
  return result;
}

[[nodiscard]] bool has_meaningful_record_tail(const nlohmann::json& record)
{
  if (!record.is_array()) {
    return false;
  }

  for (const auto& token : record) {
    if (token.is_number_float()) {
      return true;
    }

    const auto token_value = json_to_i64(token);
    if (!token_value) {
      continue;
    }

    if (*token_value >= 0) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] nlohmann::json build_segment_records_json(const nlohmann::json& battle_log, size_t start, size_t end,
                                                        const EntityIndex& entity_index, const DecodeOptions& options)
{
  auto records = nlohmann::json::array();
  if (!battle_log.is_array() || start > end || end >= battle_log.size()) {
    return records;
  }

  auto current = nlohmann::json::array();
  size_t record_index = 0;

  for (size_t token_index = start; token_index <= end; ++token_index) {
    current.push_back(battle_log[token_index]);
    if (!json_matches_i64(battle_log[token_index], kRecordTerminator)) {
      continue;
    }

    records.push_back(build_record_json(current, record_index, entity_index, options));
    current = nlohmann::json::array();
    ++record_index;
  }

  if (has_meaningful_record_tail(current)) {
    records.push_back(build_record_json(current, record_index, entity_index, options));
  }

  return records;
}

[[nodiscard]] nlohmann::json build_segment_record_summary_json(const nlohmann::json& records)
{
  auto summary = empty_record_summary_json();
  if (!records.is_array()) {
    return summary;
  }

  summary["recordCount"] = records.size();
  for (const auto& record : records) {
    if (!record.is_object()) {
      continue;
    }

    const auto kind = record.value("kind", std::string{});
    if (kind == "attack") {
      summary["attackCount"] = summary.value("attackCount", 0) + 1;
      if (record.value("critical", false)) {
        summary["criticalCount"] = summary.value("criticalCount", 0) + 1;
      }
      const auto& damage = record.contains("damage") && record["damage"].is_object() ? record["damage"] : nlohmann::json::object();
      summary["hullDamageTotal"] = summary.value("hullDamageTotal", 0.0) + damage.value("hull", 0.0);
      summary["shieldDamageTotal"] = summary.value("shieldDamageTotal", 0.0) + damage.value("shield", 0.0);
      summary["mitigatedDamageTotal"] = summary.value("mitigatedDamageTotal", 0.0) + damage.value("mitigated", 0.0);
      summary["totalIsolyticDamageTotal"] = summary.value("totalIsolyticDamageTotal", 0.0)
                                              + damage.value("totalIsolytic", 0.0);
      summary["triggeredEffectCount"] = summary.value("triggeredEffectCount", 0) + record.value("triggeredEffectCount", 0);
      continue;
    }

    if (kind == "component_scalar") {
      summary["componentScalarCount"] = summary.value("componentScalarCount", 0) + 1;
      continue;
    }

    summary["opaqueCount"] = summary.value("opaqueCount", 0) + 1;
  }

  return summary;
}

[[nodiscard]] DerivedCombatAnalytics derive_combat_analytics(nlohmann::json& segments)
{
  DerivedCombatAnalytics analytics;
  if (!segments.is_array()) {
    return analytics;
  }

  size_t round_index = 0;
  size_t sub_round_index = 0;
  bool pending_round_start = true;
  size_t current_round_position = 0;

  for (auto& segment : segments) {
    if (!segment.is_object() || segment.value("truncated", false)) {
      continue;
    }

    const auto markers = collect_negative_markers(segment.value("markers", nlohmann::json::array()));
    if (marker_list_contains(markers, kRoundStartMarker)) {
      pending_round_start = true;
    }

    const auto record_count = segment.value("recordCount", 0);
    if (record_count == 0) {
      continue;
    }

    if (pending_round_start || round_index == 0) {
      ++round_index;
      sub_round_index = 0;
      analytics.rounds.push_back({{"round", round_index},
                                  {"subRounds", nlohmann::json::array()},
                                  {"summary", empty_record_summary_json()}});
      current_round_position = analytics.rounds.size() - 1;
      pending_round_start = false;
    }

    ++sub_round_index;
    segment["round"] = round_index;
    segment["subRound"] = sub_round_index;

    if (segment.contains("records") && segment["records"].is_array()) {
      for (auto& record : segment["records"]) {
        if (!record.is_object()) {
          continue;
        }
        record["segmentIndex"] = segment.value("index", size_t{0});
        record["round"] = round_index;
        record["subRound"] = sub_round_index;
        if (record.value("kind", std::string{}) == "attack") {
          analytics.attack_rows.push_back(record);
        }
      }
    }

    analytics.rounds[current_round_position]["subRounds"].push_back(
        {{"round", round_index},
         {"subRound", sub_round_index},
         {"segmentIndex", segment.value("index", size_t{0})},
         {"recordCount", record_count},
         {"attackCount", segment["summary"].value("attackCount", 0)},
         {"criticalCount", segment["summary"].value("criticalCount", 0)},
         {"markers", segment.value("markers", nlohmann::json::array())},
         {"shipIds", segment.value("ship_ids", nlohmann::json::array())},
         {"summary", segment.value("summary", empty_record_summary_json())}});
    merge_record_summary(analytics.rounds[current_round_position]["summary"],
                         segment.value("summary", empty_record_summary_json()));
  }

  for (auto& round : analytics.rounds) {
    if (!round.is_object()) {
      continue;
    }
    round["subRoundCount"] = round.contains("subRounds") && round["subRounds"].is_array() ? round["subRounds"].size() : size_t{0};
  }

  return analytics;
}

[[nodiscard]] nlohmann::json decode_segments(const nlohmann::json& battle_log, const EntityIndex& entity_index,
                                             const DecodeOptions& options)
{
  auto segments = nlohmann::json::array();
  if (!battle_log.is_array() || battle_log.empty()) {
    return segments;
  }

  size_t segment_start = 0;
  size_t segment_index = 0;
  bool   emitted_trailing_segment = false;

  for (size_t token_index = 0; token_index < battle_log.size(); ++token_index) {
    const auto token_value = json_to_i64(battle_log[token_index]);
    if (!token_value || *token_value != kSegmentTerminator) {
      continue;
    }

    if (options.segment_limit == 0 || segments.size() < options.segment_limit) {
      auto segment = build_segment_json(battle_log, segment_index, segment_start, token_index, entity_index, options);
      const auto records = build_segment_records_json(battle_log, segment_start, token_index, entity_index, options);
      segment["records"] = records;
      segment["recordCount"] = records.size();
      segment["summary"] = build_segment_record_summary_json(records);
      segments.push_back(std::move(segment));
    }
    ++segment_index;
    segment_start = token_index + 1;
  }

  if (segment_start < battle_log.size()) {
    emitted_trailing_segment = true;
    if (options.segment_limit == 0 || segments.size() < options.segment_limit) {
      auto segment = build_segment_json(battle_log, segment_index, segment_start, battle_log.size() - 1, entity_index, options);
      const auto records = build_segment_records_json(battle_log, segment_start, battle_log.size() - 1, entity_index, options);
      segment["records"] = records;
      segment["recordCount"] = records.size();
      segment["summary"] = build_segment_record_summary_json(records);
      segments.push_back(std::move(segment));
    }
  }

  if (options.segment_limit != 0 && segment_index + (emitted_trailing_segment ? 1 : 0) > segments.size()) {
    segments.push_back({{"truncated", true}, {"available_segments", segment_index + (emitted_trailing_segment ? 1 : 0)}});
  }

  return segments;
}

[[nodiscard]] std::vector<int64_t> json_array_to_i64_vector(const nlohmann::json& values)
{
  std::vector<int64_t> result;
  if (!values.is_array()) {
    return result;
  }

  for (const auto& value : values) {
    if (auto parsed = json_to_i64(value)) {
      result.push_back(*parsed);
    }
  }
  return result;
}

[[nodiscard]] nlohmann::json vector_difference_json(std::vector<int64_t> left, std::vector<int64_t> right)
{
  std::ranges::sort(left);
  std::ranges::sort(right);
  std::vector<int64_t> difference;
  std::ranges::set_difference(left, right, std::back_inserter(difference));
  return json_i64_array(difference);
}

[[nodiscard]] nlohmann::json json_object_keys(const nlohmann::json& object)
{
  auto keys = nlohmann::json::array();
  if (!object.is_object()) {
    return keys;
  }

  for (const auto& [key, _value] : object.items()) {
    keys.push_back(key);
  }
  return keys;
}

void append_resource_rewards(nlohmann::json& rewards, const nlohmann::json& resources, std::string_view source)
{
  if (!resources.is_object()) {
    return;
  }

  for (const auto& [resource_id, count] : resources.items()) {
    rewards.push_back({{"kind", "resource"}, {"source", source}, {"resourceId", resource_id}, {"count", count}});
  }
}

void append_chest_rewards(nlohmann::json& rewards, const nlohmann::json& chest_drop)
{
  if (!chest_drop.is_object() || !chest_drop.contains("chests_gained") || !chest_drop["chests_gained"].is_array()) {
    return;
  }

  for (const auto& chest : chest_drop["chests_gained"]) {
    if (!chest.is_object()) {
      continue;
    }

    const auto& params = chest.contains("params") && chest["params"].is_object() ? chest["params"]
                                                                                   : nlohmann::json::object();
    rewards.push_back({{"kind", "chest"},
                       {"source", "chest_drop"},
                       {"count", chest.contains("count") ? chest["count"] : nlohmann::json()},
                       {"refId", params.contains("ref_id") ? params["ref_id"] : nlohmann::json()},
                       {"nameKey", params.value("chest_name", std::string{})},
                       {"items", params.contains("items") ? params["items"] : nlohmann::json::array()}});
  }
}

[[nodiscard]] nlohmann::json build_report_rewards(const nlohmann::json& journal)
{
  auto rewards = nlohmann::json::array();
  if (journal.contains("resources_transferred")) {
    append_resource_rewards(rewards, journal["resources_transferred"], "resources_transferred");
  }
  if (journal.contains("resources_dropped")) {
    append_resource_rewards(rewards, journal["resources_dropped"], "resources_dropped");
  }
  if (journal.contains("chest_drop")) {
    append_chest_rewards(rewards, journal["chest_drop"]);
  }
  return rewards;
}

[[nodiscard]] nlohmann::json build_report_summary(const nlohmann::json& journal, const nlohmann::json& decoded)
{
  const auto initiator_wins = journal.value("initiator_wins", false);
  const auto& chest_drop = journal.contains("chest_drop") && journal["chest_drop"].is_object() ? journal["chest_drop"]
                                                                                                  : nlohmann::json::object();
  const auto round_count = decoded.contains("rounds") && decoded["rounds"].is_array() ? decoded["rounds"].size() : size_t{0};
  const auto attack_row_count = decoded.contains("attack_rows") && decoded["attack_rows"].is_array() ? decoded["attack_rows"].size()
                                                                                                         : size_t{0};

  return nlohmann::json{{"battleId", journal.contains("id") ? json_id_to_string(journal["id"]) : std::string{}},
                        {"battleType", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                        {"battleTime", journal.value("battle_time", std::string{})},
                        {"battleDuration", journal.value("battle_duration", 0)},
                        {"initiatorId", journal.value("initiator_id", std::string{})},
                        {"targetId", journal.value("target_id", std::string{})},
                        {"initiatorWins", initiator_wins},
                        {"outcome", initiator_wins ? "initiator_victory" : "target_victory"},
                        {"systemId", journal.contains("system_id") ? journal["system_id"] : nlohmann::json()},
                        {"coords", journal.contains("coords") ? journal["coords"] : nlohmann::json()},
                        {"lootRollKey", chest_drop.value("loot_roll_key", std::string{})},
                        {"participantCount", decoded.value("participant_count", size_t{0})},
                        {"shipCount", decoded.value("ship_count", size_t{0})},
                        {"componentCount", decoded.value("component_count", size_t{0})},
                        {"roundCount", round_count},
                        {"attackRowCount", attack_row_count}};
}

[[nodiscard]] nlohmann::json build_parity_status(const nlohmann::json& decoded)
{
  const auto has_attack_rows = decoded.contains("attack_rows") && decoded["attack_rows"].is_array() && !decoded["attack_rows"].empty();
  return nlohmann::json{{"reference", "stfc_client_csv_export"},
                        {"sections",
                         {{"battleSummary", "structured"},
                          {"rewards", "structured_ids"},
                          {"fleetStats", "structured_ids"},
                          {"battleEvents", has_attack_rows ? "partial" : "decoded_segments"}}},
                        {"notes",
                         has_attack_rows
                             ? nlohmann::json::array({"Round, sub-round, and per-attack rows are now derived from stable battle_log record markers.",
                                                      "Resource, ship, officer, ability, and location display names still require a later id-to-loca resolver.",
                                                      "Some auxiliary attack scalars remain unlabeled until more marker semantics are confirmed."})
                             : nlohmann::json::array({"Resource, ship, officer, ability, and location display names require a later id-to-loca resolver.",
                                                      "CSV battle event rows are represented as decoded battle_log segments until marker semantics are complete."})}};
}

[[nodiscard]] nlohmann::json build_raw_summary(const nlohmann::json& journal, const nlohmann::json& decoded)
{
  const auto& signature = decoded.contains("signature") && decoded["signature"].is_object() ? decoded["signature"]
                                                                                              : nlohmann::json::object();
  return nlohmann::json{{"journalKeys", json_object_keys(journal)},
                        {"battleLogTokenCount", signature.value("token_count", size_t{0})},
                        {"battleLogFirstTokens", signature.contains("first_tokens") ? signature["first_tokens"] : nlohmann::json::array()},
                        {"battleLogLastTokens", signature.contains("last_tokens") ? signature["last_tokens"] : nlohmann::json::array()}};
}

[[nodiscard]] nlohmann::json build_capture_summary(const nlohmann::json& journal)
{
  const auto initiator_wins = journal.value("initiator_wins", false);
  const auto& chest_drop = journal.contains("chest_drop") && journal["chest_drop"].is_object() ? journal["chest_drop"]
                                                                                                  : nlohmann::json::object();

  return nlohmann::json{{"battleType", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                        {"battleTime", journal.value("battle_time", std::string{})},
                        {"battleDuration", journal.value("battle_duration", 0)},
                        {"initiatorId", journal.value("initiator_id", std::string{})},
                        {"targetId", journal.value("target_id", std::string{})},
                        {"initiatorWins", initiator_wins},
                        {"outcome", initiator_wins ? "initiator_victory" : "target_victory"},
                        {"systemId", journal.contains("system_id") ? json_id_to_string(journal["system_id"]) : std::string{}},
                        {"coords", journal.contains("coords") ? journal["coords"] : nlohmann::json()},
                        {"lootRollKey", chest_drop.value("loot_roll_key", std::string{})}};
}

[[nodiscard]] nlohmann::json build_lossless_journal_without_battle_log(const nlohmann::json& journal)
{
  auto journal_without_battle_log = journal;
  if (journal_without_battle_log.is_object()) {
    journal_without_battle_log.erase("battle_log");
  }

  return nlohmann::json{{"encoding", "lossless_integer_strings.v1"},
                        {"omittedKeys", nlohmann::json::array({"battle_log"})},
                        {"data", lossless_integer_json(journal_without_battle_log)}};
}
} // namespace

bool journal_matches_options(const nlohmann::json& probe_or_journal, const DecodeOptions& options)
{
  if (options.battle_type_filter.empty()) {
    return true;
  }

  const auto& journal = journal_from_probe_or_journal(probe_or_journal);
  if (!journal.is_object() || !journal.contains("battle_type")) {
    return false;
  }

  const auto battle_type = json_to_i64(journal["battle_type"]);
  if (!battle_type) {
    return false;
  }

  return std::ranges::find(options.battle_type_filter, *battle_type) != options.battle_type_filter.end();
}

nlohmann::json decode_probe_entry(const nlohmann::json& probe_entry, const DecodeOptions& options)
{
  if (!probe_entry.is_object() || !probe_entry.contains("journal") || !probe_entry["journal"].is_object()) {
    return nlohmann::json{{"ok", false}, {"reason", "probe entry does not contain a journal object"}};
  }

  const auto journal_id = probe_entry.contains("journal_id") ? json_to_u64_or_zero(probe_entry["journal_id"]) : uint64_t{0};
  const auto names = probe_entry.contains("names") && probe_entry["names"].is_object() ? probe_entry["names"]
                                                                                         : nlohmann::json::object();
  return decode_journal(probe_entry["journal"], names, options, journal_id);
}

nlohmann::json decode_journal(const nlohmann::json& journal, const nlohmann::json& names, const DecodeOptions& options,
                              uint64_t journal_id_override)
{
  if (!journal.is_object()) {
    return nlohmann::json{{"ok", false}, {"reason", "journal is not an object"}};
  }

  if (!journal_matches_options(journal, options)) {
    return nlohmann::json{{"ok", false}, {"reason", "journal battle_type filtered out"}};
  }

  if (!journal.contains("battle_log") || !journal["battle_log"].is_array()) {
    return nlohmann::json{{"ok", false}, {"reason", "journal does not contain battle_log array"}};
  }

  const auto& battle_log = journal["battle_log"];
  const auto entity_index = build_entity_index(journal, names);
  const auto journal_id = journal_id_override != 0 ? journal_id_override
                                                  : (journal.contains("id") ? json_to_u64_or_zero(journal["id"]) : uint64_t{0});

  auto decoded = nlohmann::json{{"ok", true},
                                {"type", "battle_log_decode"},
                                {"journal_id", journal_id},
                                {"battle_id", journal.contains("id") ? journal["id"] : nlohmann::json(journal_id)},
                                {"battle_type", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                                {"battle_time", journal.value("battle_time", std::string{})},
                                {"initiator_id", journal.value("initiator_id", std::string{})},
                                {"target_id", journal.value("target_id", std::string{})},
                                {"initiator_wins", journal.value("initiator_wins", false)},
                                {"participant_count", entity_index.participants.size()},
                                {"ship_count", entity_index.ship_ids.size()},
                                {"component_count", entity_index.component_ids.size()},
                                {"participants", participants_to_json(entity_index)},
                                {"marker_hints", marker_hints()},
                                {"signature", build_signature(battle_log, options)}};

  if (options.include_segments) {
    auto segments = decode_segments(battle_log, entity_index, options);
    auto analytics = derive_combat_analytics(segments);
    decoded["segments"] = std::move(segments);
    decoded["rounds"] = std::move(analytics.rounds);
    decoded["attack_rows"] = std::move(analytics.attack_rows);
  }

  return decoded;
}

nlohmann::json build_sidecar_battle_report_event(const nlohmann::json& journal, const nlohmann::json& decoded,
                                                 uint64_t journal_id_override, int64_t captured_at_unix_ms)
{
  if (!journal.is_object()) {
    return nlohmann::json{{"ok", false}, {"reason", "journal is not an object"}};
  }

  if (!decoded.is_object() || !decoded.value("ok", false)) {
    return nlohmann::json{{"ok", false}, {"reason", "decoded battle log is not available"}, {"decoded", decoded}};
  }

  const auto journal_id = journal_id_override != 0 ? std::to_string(journal_id_override)
                                                   : (journal.contains("id") ? json_id_to_string(journal["id"]) : std::string{});
  const auto battle_id = journal.contains("id") ? json_id_to_string(journal["id"]) : journal_id;
  const auto timestamp = journal.value("battle_time", std::string{});
  const auto events = decoded.contains("segments") && decoded["segments"].is_array() ? decoded["segments"]
                                                                                         : nlohmann::json::array();
  const auto rounds = decoded.contains("rounds") && decoded["rounds"].is_array() ? decoded["rounds"] : nlohmann::json::array();
  const auto attack_rows = decoded.contains("attack_rows") && decoded["attack_rows"].is_array() ? decoded["attack_rows"]
                                                                                                   : nlohmann::json::array();
  const auto& signature = decoded.contains("signature") && decoded["signature"].is_object() ? decoded["signature"]
                                                                                              : nlohmann::json::object();

  auto event = nlohmann::json{{"protocolVersion", "stfc.sidecar.events.v0"},
                              {"type", "battle.report"},
                              {"schemaVersion", "stfc.sidecar.battle-report.v0"},
                              {"timestamp", timestamp},
                              {"source", "stfc-community-mod"},
                              {"journalId", journal_id},
                              {"battleId", battle_id},
                              {"battleType", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                              {"report",
                               {{"summary", build_report_summary(journal, decoded)},
                                {"rewards", build_report_rewards(journal)},
                                {"fleets", decoded.contains("participants") ? decoded["participants"] : nlohmann::json::array()},
                                {"events", events},
                                  {"rounds", rounds},
                                  {"attackRows", attack_rows},
                                {"decode",
                                   {{"status", attack_rows.empty() ? "decoded_segments" : "decoded_segments_with_attack_rows"},
                                  {"signature", signature},
                                  {"markerHints", decoded.contains("marker_hints") ? decoded["marker_hints"] : nlohmann::json::object()}}},
                                  {"parity", build_parity_status(decoded)},
                                {"raw", build_raw_summary(journal, decoded)}}}};

  if (captured_at_unix_ms > 0) {
    event["capturedAtUnixMs"] = captured_at_unix_ms;
  }

  return event;
}

nlohmann::json build_sidecar_battle_capture_event(const nlohmann::json& journal, const nlohmann::json& names,
                                                  uint64_t journal_id_override, int64_t captured_at_unix_ms)
{
  if (!journal.is_object()) {
    return nlohmann::json{{"ok", false}, {"reason", "journal is not an object"}};
  }

  if (!journal.contains("battle_log") || !journal["battle_log"].is_array()) {
    return nlohmann::json{{"ok", false}, {"reason", "journal does not contain battle_log array"}};
  }

  const auto journal_id = journal_id_override != 0 ? std::to_string(journal_id_override)
                                                   : (journal.contains("id") ? json_id_to_string(journal["id"]) : std::string{});
  const auto battle_id = journal.contains("id") ? json_id_to_string(journal["id"]) : journal_id;
  const auto timestamp = journal.value("battle_time", std::string{});
  const auto entity_index = build_entity_index(journal, names);
  const auto& battle_log = journal["battle_log"];

  auto event = nlohmann::json{{"protocolVersion", "stfc.sidecar.events.v0"},
                              {"type", "battle.capture"},
                              {"schemaVersion", "stfc.battle.capture.v1"},
                              {"timestamp", timestamp},
                              {"source", "stfc-community-mod"},
                              {"journalId", journal_id},
                              {"battleId", battle_id},
                              {"battleType", journal.contains("battle_type") ? journal["battle_type"] : nlohmann::json()},
                              {"capture",
                               {{"sourceKind", "scopely.journal.battle"},
                                {"summary", build_capture_summary(journal)},
                                {"participants", participants_to_capture_json(entity_index)},
                                {"battleLog",
                                 {{"encoding", "string_tokens.v1"},
                                  {"tokenCount", battle_log.size()},
                                  {"tokens", json_token_string_array(battle_log)}}},
                                {"names", lossless_integer_json(names)},
                                {"journal", build_lossless_journal_without_battle_log(journal)}}}};

  if (captured_at_unix_ms > 0) {
    event["capturedAtUnixMs"] = captured_at_unix_ms;
    event["capture"]["capturedAtUnixMs"] = captured_at_unix_ms;
  }

  return event;
}

nlohmann::json compare_probe_entries(const nlohmann::json& left, const nlohmann::json& right, const DecodeOptions& options)
{
  auto summary_options = options;
  summary_options.include_segments = false;

  const auto left_decoded = decode_probe_entry(left, summary_options);
  const auto right_decoded = decode_probe_entry(right, summary_options);
  if (!left_decoded.value("ok", false) || !right_decoded.value("ok", false)) {
    return nlohmann::json{{"ok", false}, {"left", left_decoded}, {"right", right_decoded}};
  }

  const auto left_markers = json_array_to_i64_vector(left_decoded["signature"]["negative_tokens"]);
  const auto right_markers = json_array_to_i64_vector(right_decoded["signature"]["negative_tokens"]);
  const auto left_token_count = left_decoded["signature"].value("token_count", size_t{0});
  const auto right_token_count = right_decoded["signature"].value("token_count", size_t{0});
  const auto left_segment_count = left_decoded["signature"].value("segment_count", size_t{0});
  const auto right_segment_count = right_decoded["signature"].value("segment_count", size_t{0});

  return nlohmann::json{{"ok", true},
                        {"type", "battle_log_compare"},
                        {"left", left_decoded},
                        {"right", right_decoded},
                        {"diff",
                         {{"token_count_delta", static_cast<int64_t>(right_token_count) - static_cast<int64_t>(left_token_count)},
                          {"segment_count_delta", static_cast<int64_t>(right_segment_count) - static_cast<int64_t>(left_segment_count)},
                          {"markers_added", vector_difference_json(right_markers, left_markers)},
                          {"markers_removed", vector_difference_json(left_markers, right_markers)}}}};
}
} // namespace battle_log_decoder