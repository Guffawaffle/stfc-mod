#include <doctest/doctest.h>

#include "config_release_validation.h"
#include "patches/notification_catalog.h"

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
} // namespace

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
  const auto  source         = read_text_file("example_community_patch_settings.toml");
  const auto  config         = toml::parse(source);
  const auto* debug          = config["debug"].as_table();
  const auto* advanced       = config["advanced"]["diagnostics"].as_table();
  const auto* advanced_files = config["advanced"]["diagnostics"]["files"].as_table();

  CHECK(source.find("manual_navigation_refresh") == std::string::npos);
  CHECK(source.find("ghost_owner_diagnostics") == std::string::npos);
  CHECK(source.find("[advanced.diagnostics.kirshara_queue]") == std::string::npos);
  CHECK(source.find("[advanced.kirshara_queue]") == std::string::npos);
  CHECK(source.find("[advanced.queue]") != std::string::npos);
  CHECK(source.find("example_science_patch_settings.toml") != std::string::npos);
  REQUIRE(advanced != nullptr);
  REQUIRE(advanced_files != nullptr);
  const auto debug_has_live_query    = debug != nullptr && debug->contains("live_query");
  const auto debug_has_runtime_trace = debug != nullptr && debug->contains("runtime_trace");
  const auto debug_has_runtime_trace_track_overhead =
      debug != nullptr && debug->contains("runtime_trace_track_overhead");
  const auto debug_has_mod_impact_monitor = debug != nullptr && debug->contains("mod_impact_monitor");
  const auto debug_has_runtime_trace_report_interval_ms =
      debug != nullptr && debug->contains("runtime_trace_report_interval_ms");
  const auto debug_has_refinery_diagnostics     = debug != nullptr && debug->contains("refinery_diagnostics");
  const auto debug_has_queue_add_direct_handler = debug != nullptr && debug->contains("queue_add_direct_handler");
  const auto debug_has_queue_add_hide_viewers   = debug != nullptr && debug->contains("queue_add_hide_viewers");

  CHECK_FALSE(debug_has_live_query);
  CHECK_FALSE(debug_has_runtime_trace);
  CHECK_FALSE(debug_has_runtime_trace_track_overhead);
  CHECK_FALSE(debug_has_mod_impact_monitor);
  CHECK_FALSE(debug_has_runtime_trace_report_interval_ms);
  CHECK_FALSE(debug_has_refinery_diagnostics);
  CHECK_FALSE(debug_has_queue_add_direct_handler);
  CHECK_FALSE(debug_has_queue_add_hide_viewers);
  CHECK(advanced->contains("live_query"));
  CHECK(advanced->contains("hotkey_suppression_logging"));
  CHECK_FALSE(advanced->get("hotkey_suppression_logging")->value<bool>().value_or(true));
  CHECK(advanced->contains("notification_skip_logging"));
  CHECK_FALSE(advanced->get("notification_skip_logging")->value<bool>().value_or(true));
  CHECK(advanced->contains("fleet_selection_timing_logging"));
  CHECK_FALSE(advanced->get("fleet_selection_timing_logging")->value<bool>().value_or(true));
  CHECK_FALSE(advanced->contains("runtime_trace"));
  CHECK_FALSE(advanced->contains("runtime_trace_track_overhead"));
  CHECK_FALSE(advanced->contains("mod_impact_monitor"));
  CHECK_FALSE(advanced->contains("runtime_trace_report_interval_ms"));
  CHECK_FALSE(advanced->contains("action_queue_guard_logging"));
  CHECK(advanced->contains("refinery_diagnostics"));
  CHECK(advanced_files->contains("root"));
  CHECK_FALSE(advanced_files->contains("main_log_max_kb"));
  CHECK_FALSE(advanced_files->contains("main_log_files"));
  CHECK(advanced_files->get("root")->value<std::string>().value_or("non-empty").empty());
  CHECK(config["advanced"]["queue"]["thin_queue_protection"].value<bool>().value_or(false));
}

TEST_CASE("example config exposes only the public supported notification event surface")
{
  const auto  source        = read_text_file("example_community_patch_settings.toml");
  const auto  config        = toml::parse(source);
  const auto* notifications = config["notifications"].as_table();

  REQUIRE(notifications != nullptr);
  CHECK(notifications->size() == public_notification_kinds().size());
  for (const auto kind : public_notification_kinds()) {
    const auto* spec = notification_catalog_entry(kind);
    REQUIRE(spec != nullptr);
    REQUIRE(notifications->contains(spec->canonical_key));
    CHECK(notifications->get(spec->canonical_key)->value<bool>().value_or(true) == false);
  }

  CHECK_FALSE(notifications->contains("partial_victory"));
  CHECK_FALSE(notifications->contains("standard"));
  CHECK(source.find("notifications_enabled") == std::string::npos);
  CHECK(source.find("notifications_audio_enabled") == std::string::npos);
  CHECK(source.find("[notifications.system") == std::string::npos);
  CHECK(source.find("[notifications.audio") == std::string::npos);
  CHECK(source.find("[notifications.events") == std::string::npos);
  CHECK(source.find("default_sound") == std::string::npos);
}

TEST_CASE("science config captures dormant queue repair surfaces")
{
  const auto  source                              = read_text_file("example_science_patch_settings.toml");
  const auto  config                              = toml::parse(source);
  const auto* advanced_kirshara_queue_diagnostics = config["advanced"]["diagnostics"]["kirshara_queue"].as_table();
  const auto* advanced_kirshara_queue             = config["advanced"]["kirshara_queue"].as_table();
  const auto* advanced_queue                      = config["advanced"]["queue"].as_table();

  REQUIRE(advanced_kirshara_queue_diagnostics != nullptr);
  REQUIRE(advanced_kirshara_queue != nullptr);
  REQUIRE(advanced_queue != nullptr);

  CHECK(advanced_kirshara_queue_diagnostics->contains("enabled"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("enabled")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("dump_interesting_methods"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("dump_interesting_methods")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("on_strike_complete"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("on_strike_complete")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("remove_target_and_attack_next"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("remove_target_and_attack_next")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("check_to_clear_action_queue"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("check_to_clear_action_queue")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("is_target_valid"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("is_target_valid")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("process_queue_deployed"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("process_queue_deployed")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("process_queue_target"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("process_queue_target")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("on_set_course_response"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("on_set_course_response")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("on_player_fleet_state_changed"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("on_player_fleet_state_changed")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("on_fleet_state_change"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("on_fleet_state_change")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue_diagnostics->contains("on_fleets_disposed"));
  CHECK_FALSE(advanced_kirshara_queue_diagnostics->get("on_fleets_disposed")->value<bool>().value_or(true));

  CHECK(advanced_kirshara_queue->contains("enabled"));
  CHECK_FALSE(advanced_kirshara_queue->get("enabled")->value<bool>().value_or(true));
  CHECK(advanced_kirshara_queue->contains("course_target_completion"));
  CHECK_FALSE(advanced_kirshara_queue->get("course_target_completion")->value<bool>().value_or(true));

  CHECK(advanced_queue->contains("queue_repair_enabled"));
  CHECK_FALSE(advanced_queue->get("queue_repair_enabled")->value<bool>().value_or(true));
  CHECK_FALSE(advanced_queue->contains("thin_queue_protection"));
  CHECK(advanced_queue->contains("queue_add_direct_handler"));
  CHECK_FALSE(advanced_queue->get("queue_add_direct_handler")->value<bool>().value_or(true));
  CHECK_FALSE(advanced_queue->contains("queue_add_hide_viewers"));
}
