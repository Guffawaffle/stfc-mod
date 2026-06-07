#include <doctest/doctest.h>

#include "config_release_validation.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
  std::filesystem::path find_repo_file(const std::string_view relative_path)
  {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
      const auto candidate = current / std::filesystem::path(relative_path);
      if (std::filesystem::exists(candidate)) {
        return candidate;
      }

      if (!current.has_parent_path()) {
        break;
      }

      const auto parent = current.parent_path();
      if (parent == current) {
        break;
      }

      current = parent;
    }

    return {};
  }

  std::string read_text_file(const std::string_view relative_path)
  {
    const auto path = find_repo_file(relative_path);
    REQUIRE_MESSAGE(!path.empty(), "Failed to find " << std::string(relative_path));

    std::ifstream input(path, std::ios::binary);
    REQUIRE_MESSAGE(input.good(), "Failed to open " << path.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
  }
}

TEST_CASE("example config covers public runtime settings")
{
  const auto source = read_text_file("example_community_patch_settings.toml");
  const auto config = toml::parse(source);
  const auto result = config_release_validation::ValidateExampleConfig(config);

  std::ostringstream error_summary;
  error_summary << "Missing keys: " << result.errors.size();
  for (const auto& error : result.errors) {
    error_summary << "\n" << error.path << " -> " << error.message;
  }

  CHECK_MESSAGE(result.ok(), error_summary.str());
}

TEST_CASE("keymapping generated compatibility section stays in sync")
{
  const auto markdown         = read_text_file("KEYMAPPING.md");
  const auto actual_section   = config_release_validation::ExtractGeneratedInputBindingCompatibilitySection(markdown);
  const auto expected_section = config_release_validation::RenderGeneratedInputBindingCompatibilitySection();

  REQUIRE_FALSE(actual_section.empty());
  CHECK(config_release_validation::NormalizeMarkdownNewlines(actual_section)
        == config_release_validation::NormalizeMarkdownNewlines(expected_section));
}

TEST_CASE("example config does not reintroduce abandoned ghost or manual refresh keys")
{
  const auto source = read_text_file("example_community_patch_settings.toml");
  const auto config = toml::parse(source);
  const auto* debug = config["debug"].as_table();
  const auto* advanced = config["advanced"]["diagnostics"].as_table();
  const auto* advanced_files = config["advanced"]["diagnostics"]["files"].as_table();
  const auto* advanced_kirshara_queue = config["advanced"]["kirshara_queue"].as_table();
  const auto* advanced_queue = config["advanced"]["queue"].as_table();

  CHECK(source.find("manual_navigation_refresh") == std::string::npos);
  CHECK(source.find("ghost_owner_diagnostics") == std::string::npos);
  REQUIRE(advanced != nullptr);
  REQUIRE(advanced_files != nullptr);
  REQUIRE(advanced_kirshara_queue != nullptr);
  REQUIRE(advanced_queue != nullptr);
  const auto debug_has_live_query = debug != nullptr && debug->contains("live_query");
  const auto debug_has_runtime_trace = debug != nullptr && debug->contains("runtime_trace");
  const auto debug_has_runtime_trace_track_overhead =
      debug != nullptr && debug->contains("runtime_trace_track_overhead");
  const auto debug_has_mod_impact_monitor = debug != nullptr && debug->contains("mod_impact_monitor");
  const auto debug_has_runtime_trace_report_interval_ms =
      debug != nullptr && debug->contains("runtime_trace_report_interval_ms");
  const auto debug_has_refinery_diagnostics = debug != nullptr && debug->contains("refinery_diagnostics");
  const auto debug_has_queue_add_direct_handler = debug != nullptr && debug->contains("queue_add_direct_handler");
  const auto debug_has_queue_add_hide_viewers = debug != nullptr && debug->contains("queue_add_hide_viewers");

  CHECK_FALSE(debug_has_live_query);
  CHECK_FALSE(debug_has_runtime_trace);
  CHECK_FALSE(debug_has_runtime_trace_track_overhead);
  CHECK_FALSE(debug_has_mod_impact_monitor);
  CHECK_FALSE(debug_has_runtime_trace_report_interval_ms);
  CHECK_FALSE(debug_has_refinery_diagnostics);
  CHECK_FALSE(debug_has_queue_add_direct_handler);
  CHECK_FALSE(debug_has_queue_add_hide_viewers);
  CHECK(advanced->contains("live_query"));
  CHECK(advanced->contains("runtime_trace"));
  CHECK(advanced->contains("runtime_trace_track_overhead"));
  CHECK(advanced->contains("mod_impact_monitor"));
  CHECK(advanced->contains("runtime_trace_report_interval_ms"));
  CHECK(advanced->contains("refinery_diagnostics"));
  CHECK(advanced_files->contains("root"));
  CHECK_FALSE(advanced_files->contains("main_log_max_kb"));
  CHECK_FALSE(advanced_files->contains("main_log_files"));
  CHECK(advanced_files->get("root")->value<std::string>().value_or("non-empty").empty());
  CHECK(advanced_kirshara_queue->contains("enabled"));
  CHECK_FALSE(advanced_kirshara_queue->get("enabled")->value<bool>().value_or(true));
  CHECK(advanced_queue->contains("queue_add_direct_handler"));
  CHECK(advanced_queue->contains("queue_add_hide_viewers"));
}
