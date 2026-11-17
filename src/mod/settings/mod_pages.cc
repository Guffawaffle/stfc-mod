#include "mod_pages.h"
#include "audio_alerts.h"
#include "config_settings.h"
#include "maintenance_pages.h"
#include "shortcut_settings.h"

namespace mod_settings
{
PageCatalog& BuildModPages()
{
  static PageCatalog catalog("community_mod.settings", "Mod Settings");
  static const bool  registered = [&] {
    RegisterConfigSettings(catalog);
    RegisterAudioAlertPages(catalog);
    RegisterShortcutPages(catalog);
    RegisterMaintenanceTools(catalog);
    return true;
  }();
  (void)registered;
  return catalog;
}
} // namespace mod_settings
