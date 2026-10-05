#include "runtime_config_writer.h"
#include "config_alias.h"
#include "patches/parts/runtime_config_keys.h"
#include <toml++/toml.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace
{
void Check(bool value)
{
  if (!value) throw std::runtime_error("HUD persistence fixture failed");
}
} // namespace

int main(int argc, char** argv)
{
  Check(argc == 2);
  const std::filesystem::path directory(argv[1]);
  std::filesystem::create_directories(directory);
  const auto path = directory / "hud.toml";
  {
    std::ofstream output(path);
    output << "# keep this comment\n[ui]\nunrelated = true\n";
    output << "hide_artifact_exchange_all = true\ncargo_format = true\n";
    for (const char* key : {"hud_q_trials", "hud_field_training", "hud_outposts", "hud_missions"})
      output << key << " = 'auto'\n";
  }
  const auto initial = toml::parse_file(path.string());
  // Alias resolution must not invent a canonical key in the loaded disk state:
  // the writer needs an absent-key expectation when it first saves the new key.
  const auto before = initial;
  Check(ConfigAliasDefault(initial, "ui", "disable_exchange_all", "ui", "hide_artifact_exchange_all", false));
  Check(ConfigAliasDefault(initial, "ui", "format_cargo_values", "ui", "cargo_format", false));
  Check(initial == before);
  Check(!initial["ui"]["disable_exchange_all"]);
  const auto legacy_focus = toml::parse("[patches]\nfocussearch = false\n");
  Check(!ConfigAliasDefault(legacy_focus, "ui", "focus_search", "patches", "focussearch", true));
  const auto explicit_focus = toml::parse("[patches]\nfocussearch = false\n[ui]\nfocus_search = true\n");
  Check(explicit_focus["ui"]["focus_search"].value_or(
      ConfigAliasDefault(explicit_focus, "ui", "focus_search", "patches", "focussearch", true)));
  Check(ConfigAliasDefault(toml::table{}, "ui", "focus_search", "patches", "focussearch", true));
  config_edit::RuntimeConfigWriter writer(path, std::nullopt);
  for (const auto& [section, key] : config_edit::persisted_settings) {
    const auto value = initial[section][key].value<std::string>();
    Check(writer.Register(section, key, value ? std::optional<config_edit::Value>(*value) : std::nullopt));
  }
  for (const char* mode : {"always", "never", "auto"}) {
    for (const char* key : {"hud_q_trials", "hud_field_training", "hud_outposts", "hud_missions"}) {
      const auto revision = writer.Submit("ui", key, std::string(mode));
      Check(revision != 0);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (writer.LastCompletion().revision != revision || writer.HasWork()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasFailures());
      const auto loaded = toml::parse_file(path.string());
      Check(loaded["ui"][key].value<std::string>() == mode);
      Check(loaded["ui"]["unrelated"].value<bool>() == true);
    }
  }
  for (const char* key : {"galactic_anomaly_timer", "ship_hotkey_badges", "galaxy_station_housing"}) {
    for (bool enabled : {true, false, true}) {
      const auto revision = writer.Submit("graphics", key, enabled);
      Check(revision != 0);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (writer.LastCompletion().revision != revision || writer.HasWork()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasFailures());
      const auto loaded = toml::parse_file(path.string());
      Check(loaded["graphics"][key].value<bool>() == enabled);
      Check(loaded["ui"]["unrelated"].value<bool>() == true);
    }
  }
  for (const char* key : {"highlight_opc_fleets", "fleet_hud_opc_eta", "show_ship_tech_indicators",
                          "show_ship_tech_indicator_backgrounds", "disable_exchange_all", "format_cargo_values",
                          "officer_sort", "allow_officer_preset_reordering", "double_click_to_assign_ship", "focus_search"}) {
    for (bool enabled : {false, true, false}) {
      const auto revision = writer.Submit("ui", key, enabled);
      Check(revision != 0);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (writer.LastCompletion().revision != revision || writer.HasWork()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasFailures());
      const auto loaded = toml::parse_file(path.string());
      Check(loaded["ui"][key].value<bool>() == enabled);
      Check(loaded["ui"]["hide_artifact_exchange_all"].value<bool>() == true);
      Check(loaded["ui"]["cargo_format"].value<bool>() == true);
      Check(loaded["ui"]["unrelated"].value<bool>() == true);
    }
  }
  for (const auto& [section, key] : config_edit::persisted_settings) {
    if (std::string_view(section) != "audio") continue;
    const bool coalescing = std::string_view(key) == "coalescing";
    for (const std::string value : {coalescing ? std::string("same") : std::string("ping"),
                                   coalescing ? std::string("all") : std::string("C:\\My Sounds\\clip.mp3"),
                                   std::string("none")}) {
      const auto revision = writer.Submit(section, key, value);
      Check(revision != 0);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (writer.LastCompletion().revision != revision || writer.HasWork()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasFailures());
      const auto loaded = toml::parse_file(path.string());
      Check(loaded[section][key].value<std::string>() == value);
      Check(loaded["ui"]["unrelated"].value<bool>() == true);
    }
  }
  writer.Stop(false);
  while (!writer.PollStopped()) std::this_thread::yield();
  std::ifstream input(path);
  std::string first_line;
  std::getline(input, first_line);
  Check(first_line == "# keep this comment");
}
