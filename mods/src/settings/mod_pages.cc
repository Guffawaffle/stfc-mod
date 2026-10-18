#include "mod_pages.h"
#include "shortcut_settings.h"
#include "upstream_settings.h"
#include "audio_alerts.h"

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
  RegisterUpstreamSettings(catalog);
  RegisterShortcutPages(catalog);
  RegisterAudioAlertPages(catalog);
}
} // namespace mod_settings
