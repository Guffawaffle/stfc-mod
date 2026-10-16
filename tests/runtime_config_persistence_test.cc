#include "runtime_config_writer.h"
#include "patches/parts/runtime_config_keys.h"
#include <string_view>
#include "config_save.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
  assert(argc == 2);
  const auto path = std::filesystem::path(argv[1]) / "community_patch_settings.toml";
  std::filesystem::create_directories(path.parent_path());
  const std::string original = "# keep this comment\n[graphics]\ngalaxy_multi_select = false\n"
      "galaxy_label_major_detail = \"native\"\ngalaxy_label_major_threshold = 0.5\n"
      "[unrelated]\nvalue = 42\n";
  ReplaceConfigText(path, original);
  {
    config_edit::RuntimeConfigWriter writer(path, std::nullopt);
    bool base_card_registered = false;
    bool primary_registered = false;
    for (const auto& [section, key] : config_edit::persisted_settings) {
      if (std::string_view(section) == "ui" && std::string_view(key) == "keep_base_card_open") {
        base_card_registered = writer.Register(section, key, std::nullopt);
      }
      if (std::string_view(section) == "control" && std::string_view(key) == "primary_requires_visible_target") {
        primary_registered = writer.Register(section, key, std::nullopt);
      }
    }
    assert(base_card_registered);
    assert(primary_registered);
    assert(writer.Register("graphics", "galaxy_multi_select", false));
    assert(writer.Register("graphics", "galaxy_label_major_detail", std::string("native")));
    assert(writer.Register("graphics", "galaxy_label_major_threshold", 0.5));
    assert(writer.Submit("ui", "keep_base_card_open", true));
    assert(writer.Submit("control", "primary_requires_visible_target", false));
    assert(writer.Submit("graphics", "galaxy_multi_select", true));
    assert(writer.Submit("graphics", "galaxy_label_major_detail", std::string("threshold")));
    // Quit immediately with a slider write still debounced: orderly stop must flush it.
    assert(writer.Submit("graphics", "galaxy_label_major_threshold", 0.95, std::chrono::seconds(30)));
    writer.Stop(false);
  }
  // Fresh parse models a new process, with no access to live Config or writer state.
  const auto bytes = ReadConfigText(path);
  const auto loaded = toml::parse(bytes);
  assert(loaded["graphics"]["galaxy_multi_select"].value<bool>() == true);
  assert(loaded["graphics"]["galaxy_label_major_detail"].value<std::string>() == "threshold");
  assert(loaded["graphics"]["galaxy_label_major_threshold"].value<double>() == 0.95);
  assert(loaded["ui"]["keep_base_card_open"].value<bool>() == true);
  const auto primary = loaded["control"]["primary_requires_visible_target"].value<bool>();
  assert(primary && !*primary);
  assert(loaded["unrelated"]["value"].value<int>() == 42);
  assert(bytes.starts_with("# keep this comment"));
  {
    config_edit::RuntimeConfigWriter writer(path, std::nullopt);
    assert(writer.Register("control", "primary_requires_visible_target", config_edit::Value{*primary}));
    assert(writer.Submit("control", "primary_requires_visible_target", true));
    writer.Stop(false);
  }
  const auto restored = toml::parse(ReadConfigText(path));
  assert(restored["control"]["primary_requires_visible_target"].value<bool>() == true);
  assert(restored["unrelated"]["value"].value<int>() == 42);
  std::cout << "Runtime settings disk/reload regression passed\n";
}
