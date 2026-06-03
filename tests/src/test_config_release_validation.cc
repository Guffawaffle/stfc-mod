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

  CHECK(source.find("manual_navigation_refresh") == std::string::npos);
  CHECK(source.find("ghost_owner_diagnostics") == std::string::npos);
  REQUIRE(debug != nullptr);
  REQUIRE(advanced != nullptr);
  CHECK_FALSE(debug->contains("live_query"));
  CHECK_FALSE(debug->contains("runtime_trace"));
  CHECK_FALSE(debug->contains("runtime_trace_track_overhead"));
  CHECK_FALSE(debug->contains("mod_impact_monitor"));
  CHECK_FALSE(debug->contains("runtime_trace_report_interval_ms"));
  CHECK_FALSE(debug->contains("refinery_diagnostics"));
  CHECK(advanced->contains("live_query"));
  CHECK(advanced->contains("runtime_trace"));
  CHECK(advanced->contains("runtime_trace_track_overhead"));
  CHECK(advanced->contains("mod_impact_monitor"));
  CHECK(advanced->contains("runtime_trace_report_interval_ms"));
  CHECK(advanced->contains("refinery_diagnostics"));
}
