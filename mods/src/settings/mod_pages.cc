#include "mod_pages.h"
#include "fleet_labels.h"
#include "preview_settings.h"
#include "warp_mode.h"

namespace mod_settings
{
PageCatalog& ModPages()
{
  static PageCatalog catalog("community_mod.settings", "Mod Settings");
  return catalog;
}
void RegisterModPages()
{
  auto& catalog = ModPages();
  // Group by the existing TOML section only when there is a working control.
  // Placement and display names do not change setting or storage identities.
  catalog.AddPage("community_mod.graphics", "Graphics", "community_mod.settings");
  catalog.AddPage("community_mod.ui", "User Interface", "community_mod.settings");
  catalog.AddPage("community_mod.navigation.warp", "Instant warp mode", "community_mod.ui");
  catalog.AddChoice("community_mod.navigation.warp", WarpModeSetting());
  if (PreviewShortcutsAvailable()) {
    catalog.AddPage("community_mod.ui.preview_shortcuts", "Preview shortcuts", "community_mod.ui");
    for (auto option : {PreviewOption::Locate, PreviewOption::Recall})
      catalog.AddBoolean("community_mod.ui.preview_shortcuts", PreviewSetting(option));
  }
  if (CargoPreviewsAvailable()) {
    catalog.AddPage("community_mod.ui.cargo_previews", "Cargo previews", "community_mod.ui");
    catalog.AddBoolean("community_mod.ui.cargo_previews", PreviewSetting(PreviewOption::Cargo));
    // Keep target preferences editable while auto-open is off. Re-enabling the
    // master must reuse them, not replace them with a new set of defaults.
    catalog.AddHeading("community_mod.ui.cargo_previews", "community_mod.ui.cargo_targets",
                       "Target types (when auto-open is on)");
    for (auto option : {PreviewOption::PlayerCargo, PreviewOption::StationCargo, PreviewOption::HostileCargo,
                        PreviewOption::ArmadaCargo})
      catalog.AddBoolean("community_mod.ui.cargo_previews", PreviewSetting(option));
  }
  catalog.AddPage("community_mod.labels", "Fleet Labels", "community_mod.graphics");
  for (bool player : {true, false}) {
    catalog.AddHeading("community_mod.labels", player ? "community_mod.labels.player" : "community_mod.labels.other",
                       player ? "Player" : "Non-player", true);
    catalog.AddChoice("community_mod.labels", FleetLabelDetailSetting(player));
    catalog.AddSlider("community_mod.labels", FleetLabelThresholdSetting(player));
  }
}
} // namespace mod_settings
