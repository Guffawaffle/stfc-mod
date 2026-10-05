#include "upstream_features.h"
#include "boolean_settings.h"
#include "config.h"
#include "patches/runtime_config.h"
#include "preview_settings.h"
#include <memory>

namespace mod_settings
{
namespace
{
  Definition Feature(const char* section, const char* key, const char* label, bool Config::* member,
                     bool (*available)(), bool restart = false)
  {
    auto requested = std::make_shared<bool>(Config::Get().*member);
    return {std::string("community_mod.") + section + "." + key, label,
            [member, requested, available, restart] {
              return available() ? ReadResult::Known(restart ? *requested : Config::Get().*member, 1) : ReadResult{};
            },
            [section, key, member, requested, available, restart](bool value, std::uint64_t generation) {
              if (generation != 1 || !available())
                return ApplyResult::Rejected;
              if (restart)
                *requested = value;
              else
                Config::Get().*member = value;
              runtime_config::SaveSetting(section, key, value);
              return ApplyResult::Applied;
            }};
  }
  bool HotkeysAvailable()
  { return Config::Get().installHotkeyHooks && PreviewShortcutsAvailable(); }
} // namespace

void RegisterUpstreamFeaturePages(PageCatalog& catalog)
{
  static BooleanSetting exchange(Feature("ui", "disable_exchange_all", "Hide artifact Exchange All (reopen popup)",
                                         &Config::disable_exchange_all, ArtifactExchangeAvailable));
  static BooleanSetting cargo(Feature("ui", "format_cargo_values", "Format cargo values (restart required)",
                                      &Config::format_cargo_values, CargoFormatAvailable, true));
  static BooleanSetting sort(Feature("ui", "officer_sort", "Below Deck Ability sorting (restart required)",
                                     &Config::officer_sort, OfficerSortAvailable, true));
  static BooleanSetting reorder(Feature("ui", "allow_officer_preset_reordering",
                                        "Reorder officer presets (restart required)",
                                        &Config::allow_officer_preset_reordering, OfficerPresetReorderAvailable, true));
  static BooleanSetting assign(Feature("ui", "double_click_to_assign_ship", "Double-click to assign ship",
                                       &Config::double_click_to_assign_ship, DoubleClickAssignShipAvailable));
  static BooleanSetting focus(
      Feature("ui", "focus_search", "Enable focus search shortcut", &Config::focus_search, HotkeysAvailable));
  if (ArtifactExchangeAvailable()) {
    catalog.AddPage("community_mod.artifacts", "Artifacts", "community_mod.settings");
    catalog.AddBoolean("community_mod.artifacts", exchange);
  }
  if (CargoFormatAvailable()) {
    catalog.AddHeading("community_mod.previews", "community_mod.previews.formatting", "Cargo formatting");
    catalog.AddBoolean("community_mod.previews", cargo);
  }
  if (OfficerSortAvailable() || OfficerPresetReorderAvailable() || DoubleClickAssignShipAvailable()) {
    catalog.AddPage("community_mod.officers_ships", "Officers & Ships", "community_mod.settings");
    if (OfficerSortAvailable())
      catalog.AddBoolean("community_mod.officers_ships", sort);
    if (OfficerPresetReorderAvailable())
      catalog.AddBoolean("community_mod.officers_ships", reorder);
    if (DoubleClickAssignShipAvailable())
      catalog.AddBoolean("community_mod.officers_ships", assign);
  }
  if (HotkeysAvailable())
    catalog.AddBoolean("community_mod.shortcuts.interface", focus);
}
} // namespace mod_settings
