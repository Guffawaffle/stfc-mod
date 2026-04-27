/**
 * @file battle_log_decode_tool.cc
 * @brief Offline JSONL battle probe decoder for AX workflows.
 */
#include "patches/battle_log_decoder.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct CliOptions {
  std::string          probe_path;
  std::string          journal_id;
  std::optional<std::string> compare_left;
  std::optional<std::string> compare_right;
  bool                 latest = true;
  bool                 pretty = false;
  bool                 sidecar_feed = false;
  battle_log_decoder::DecodeOptions decode;
};

[[nodiscard]] nlohmann::json error_json(std::string reason)
{
  return nlohmann::json{{"ok", false}, {"reason", std::move(reason)}};
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

[[nodiscard]] std::string probe_entry_id(const nlohmann::json& entry)
{
  if (!entry.is_object()) {
    return {};
  }

  if (entry.contains("journal_id")) {
    return json_id_to_string(entry["journal_id"]);
  }

  if (entry.contains("journal") && entry["journal"].is_object() && entry["journal"].contains("id")) {
    return json_id_to_string(entry["journal"]["id"]);
  }

  return {};
}

[[nodiscard]] nlohmann::json read_probe_entries(const CliOptions& options)
{
  std::ifstream file(options.probe_path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return error_json("failed to open probe file: " + options.probe_path);
  }

  nlohmann::json latest_match;
  nlohmann::json id_match;
  nlohmann::json compare_left;
  nlohmann::json compare_right;
  size_t         line_number = 0;
  size_t         parsed_count = 0;
  std::string    line;

  while (std::getline(file, line)) {
    ++line_number;
    if (line.empty()) {
      continue;
    }

    nlohmann::json entry;
    try {
      entry = nlohmann::json::parse(line);
    } catch (const std::exception& error) {
      return error_json("failed to parse probe JSONL line " + std::to_string(line_number) + ": " + error.what());
    }

    ++parsed_count;
    const auto entry_id = probe_entry_id(entry);

    if (options.compare_left && entry_id == *options.compare_left) {
      compare_left = entry;
    }
    if (options.compare_right && entry_id == *options.compare_right) {
      compare_right = entry;
    }

    if (!options.journal_id.empty() && entry_id == options.journal_id) {
      id_match = entry;
    }

    if (battle_log_decoder::journal_matches_options(entry, options.decode)) {
      latest_match = entry;
    }
  }

  if (options.compare_left || options.compare_right) {
    if (compare_left.is_null() || compare_right.is_null()) {
      return nlohmann::json{{"ok", false},
                            {"reason", "compare journal id not found"},
                            {"left_found", !compare_left.is_null()},
                            {"right_found", !compare_right.is_null()},
                            {"parsed_count", parsed_count}};
    }

    auto compared = battle_log_decoder::compare_probe_entries(compare_left, compare_right, options.decode);
    compared["parsed_count"] = parsed_count;
    return compared;
  }

  const auto selected = !options.journal_id.empty() ? id_match : latest_match;
  if (selected.is_null()) {
    return nlohmann::json{{"ok", false}, {"reason", "matching journal not found"}, {"parsed_count", parsed_count}};
  }

  auto decoded = battle_log_decoder::decode_probe_entry(selected, options.decode);
  decoded["parsed_count"] = parsed_count;
  decoded["selected_journal_id"] = probe_entry_id(selected);
  if (options.sidecar_feed) {
    if (!selected.is_object() || !selected.contains("journal") || !selected["journal"].is_object()) {
      return error_json("selected probe entry does not contain a journal object");
    }

    auto event = battle_log_decoder::build_sidecar_battle_report_event(selected["journal"], decoded);
    event["parsedCount"] = parsed_count;
    return event;
  }
  return decoded;
}

[[nodiscard]] std::optional<CliOptions> parse_args(int argc, char** argv, nlohmann::json& error)
{
  CliOptions options;

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    auto require_value = [&](std::string_view name) -> std::optional<std::string> {
      if (index + 1 >= argc) {
        error = error_json("missing value for " + std::string(name));
        return std::nullopt;
      }
      ++index;
      return std::string(argv[index]);
    };

    try {
      if (arg == "--probe") {
        auto value = require_value(arg);
        if (!value) return std::nullopt;
        options.probe_path = *value;
      } else if (arg == "--journal-id") {
        auto value = require_value(arg);
        if (!value) return std::nullopt;
        options.journal_id = *value;
        options.latest = false;
      } else if (arg == "--compare") {
        auto left = require_value(arg);
        if (!left) return std::nullopt;
        auto right = require_value(arg);
        if (!right) return std::nullopt;
        options.compare_left = *left;
        options.compare_right = *right;
        options.latest = false;
      } else if (arg == "--latest") {
        options.latest = true;
      } else if (arg == "--segments" || arg == "--dump-segments") {
        options.decode.include_segments = true;
      } else if (arg == "--sidecar-feed") {
        options.sidecar_feed = true;
        options.decode.include_segments = true;
      } else if (arg == "--summary-only") {
        options.decode.include_segments = false;
      } else if (arg == "--segment-limit") {
        auto value = require_value(arg);
        if (!value) return std::nullopt;
        options.decode.segment_limit = static_cast<size_t>(std::stoull(*value));
        options.decode.include_segments = true;
      } else if (arg == "--battle-type") {
        auto value = require_value(arg);
        if (!value) return std::nullopt;
        options.decode.battle_type_filter.push_back(std::stoll(*value));
      } else if (arg == "--pretty") {
        options.pretty = true;
      } else if (arg == "--help" || arg == "-h") {
        error = nlohmann::json{{"ok", true},
                               {"usage", "battle-log-decode --probe <jsonl> [--latest|--journal-id <id>|--compare <left> <right>] [--segments|--sidecar-feed] [--segment-limit <n>] [--battle-type <n>] [--pretty]"}};
        return std::nullopt;
      } else {
        error = error_json("unknown argument: " + arg);
        return std::nullopt;
      }
    } catch (const std::exception& exception) {
      error = error_json("invalid value for " + arg + ": " + exception.what());
      return std::nullopt;
    }
  }

  if (options.probe_path.empty()) {
    error = error_json("missing required --probe <path>");
    return std::nullopt;
  }

  return options;
}
} // namespace

int main(int argc, char** argv)
{
  nlohmann::json parse_error;
  const auto options = parse_args(argc, argv, parse_error);
  if (!options) {
    std::cout << parse_error.dump(parse_error.value("ok", false) ? 2 : -1) << '\n';
    return parse_error.value("ok", false) ? 0 : 2;
  }

  const auto result = read_probe_entries(*options);
  std::cout << result.dump(options->pretty ? 2 : -1) << '\n';
  return result.value("ok", false) || result.value("type", std::string{}) == "battle.report" ? 0 : 1;
}