// Science isolation: install only the verified cargo text snap hook.
// The accepted combined implementation remains at play-dev 57d8d843.
#include "settings/preview_settings.h"
#include <spdlog/spdlog.h>

bool InstallInstantCargoTextHooks();

namespace
{
bool installed{};
}

bool mod_settings::InstantCargoCounterAvailable()
{ return installed; }

void InstallInstantCargoCounterHooks()
{
  installed = InstallInstantCargoTextHooks();
  spdlog::info("[InstantCargo] text-only experiment {}; fill-bar duration hooks omitted",
               installed ? "installed" : "unavailable");
}
