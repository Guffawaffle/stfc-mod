#include "patches/parts/runtime_config_keys.h"
#include "settings/settings_search.h"
#include <cassert>
#include <iostream>
using namespace mod_settings;
int main()
{
  int        reads = 0, writes = 0;
  const auto definition = [&](std::string id, std::string label) {
    return Definition{std::move(id), std::move(label),
                      [&] {
                        ++reads;
                        return ReadResult::Known(true, 1);
                      },
                      [&](bool, std::uint64_t) {
                        ++writes;
                        return ApplyResult::Applied;
                      }};
  };
  BooleanSetting cargo(definition("community_mod.ui.instant_cargo_counter", "Instant ship cargo counter"));
  BooleanSetting tech(definition("community_mod.ui.show_ship_tech_indicators", "Show equipped FT/CT"));
  BooleanSetting labels(definition("community_mod.labels.player.detail", "Label visibility"));
  BooleanSetting hidden(definition("community_mod.ui.show_hostile_cargo", "Hostile cargo"));
  ActionSetting  change{"shortcut.change", "Change", [](std::size_t) { return ActionSetting::Presentation{}; },
                        [](std::size_t) {}};
  ActionSetting  remove{"shortcut.remove", "Remove", [](std::size_t) { return ActionSetting::Presentation{}; },
                        [](std::size_t) {}};
  PageCatalog    pages("community_mod.settings", "Mod Settings");
  pages.AddPage("community_mod.previews", "Previews & Cargo", "community_mod.settings");
  pages.AddHeading("community_mod.previews", "ship.selection", "Ship selection");
  pages.AddBoolean("community_mod.previews", tech);
  pages.AddBoolean("community_mod.previews", cargo);
  pages.AddHeading("community_mod.previews", "cargo.targets", "Target types", false, [] { return false; });
  pages.AddBoolean("community_mod.previews", hidden);
  pages.AddPage("community_mod.labels", "Fleet Labels", "community_mod.settings");
  pages.AddHeading("community_mod.labels", "player", "Player", true);
  pages.AddBoolean("community_mod.labels", labels);
  pages.AddPage("community_mod.shortcuts", "Shortcuts", "community_mod.settings");
  pages.AddPage("community_mod.shortcuts.camera", "Camera", "community_mod.shortcuts");
  pages.AddPage("community_mod.shortcuts.pan_left", "Pan left", "community_mod.shortcuts.camera");
  pages.AddAction("community_mod.shortcuts.pan_left", change);
  pages.AddAction("community_mod.shortcuts.pan_left", remove);
  SettingsSearch search;
  auto           plan = pages.Build();
  ActionSetting  notice{"community_mod.save_notice", "Save notice",
                        [](std::size_t) { return ActionSetting::Presentation{}; }, [](std::size_t) {}};
  for (auto& page : plan)
    page.items.insert(page.items.begin(), &notice);
  search.Build(plan);
  assert(search.Find("").empty());
  assert(search.Find("   ").empty());
  assert(search.Find("no such thing").empty());
  assert(search.Find("cargo").size() == 3);
  assert(search.Find("show ship tech indicators").front()->item == tech.id());
  assert(search.Find("UI.INSTANT_CARGO_COUNTER").size() == 1);
  assert(search.Find("instant cargo").front()->item == cargo.id());
  assert(search.Find("zoom_label_player_detail").front()->item == labels.id());
  assert(search.Find("fleet player visibility").front()->key == "graphics.zoom_label_player_detail");
  assert(search.Find("hostile cargo").front()->item == hidden.id()); // Discoverable even while its parent is off.
  assert(search.Find("shortcuts pan_left").size() == 1);             // Not one hit per editor command.
  assert(search.Find("shortcuts pan_left").front()->key == "shortcuts.pan_left");
  assert(search.Find("pan left").front()->location == "Shortcuts > Camera > Pan left");
  assert(search.Find("Remove").empty());
  assert(search.Find("shortcuts.camera").size() == 1); // Category contributes context, never a fake TOML key.
  assert(reads == 0 && writes == 0);                   // Search is presentation-only.
  assert(SearchTomlKey("community_mod.hud.q_trials") == "ui.hud_q_trials");
  assert(SearchTomlKey("community_mod.galaxy.overlays.hostiles") == "graphics.galaxy_overlay_hostiles");
  assert(SearchTomlKey("unrelated") == "");
  for (const auto* id : {"community_mod.labels.player.detail", "community_mod.labels.other.threshold",
                         "community_mod.galaxy.minor.detail", "community_mod.galaxy.major.threshold",
                         "community_mod.galaxy.overlays.default", "community_mod.galaxy.multi_select",
                         "community_mod.hud.field_training", "community_mod.navigation.galactic_anomaly_timer",
                         "community_mod.ui.show_ship_tech_indicators"}) {
    const auto key = SearchTomlKey(id);
    assert(std::ranges::any_of(config_edit::persisted_settings, [&](const auto& setting) {
      return key == std::string(setting.first) + "." + setting.second;
    }));
  }
  std::cout << "Settings search tests passed\n";
}
