#include "settings/native/page_navigation.h"
#include "patches/runtime_config.h"
#include "settings/native/value_widgets.h"
#include <spdlog/spdlog.h>

void InstallNativeSettings()
{
  // Native extents are checked against Windows unwind records. Other platforms
  // omit this UI until equivalent hook evidence is available.
#if defined(_WIN32) && defined(_M_X64)
  using namespace mod_settings::native;
  try {
    // Settings saving must also work when the optional hotkey patch is off.
    // Install is idempotent when hotkeys already initialized persistence.
    runtime_config::Install();
    if (!InstallCoreValueWidgets())
      return;
    try {
      InstallPages();
    } catch (...) {
      DisablePages();
      spdlog::warn("[ModSettings] Navigation unavailable; native confirmation control remains available");
    }
    spdlog::info("[ModSettings] Native settings adapter installed (Windows x64)");
  } catch (...) {
    Warn();
  }
#endif
}
