#include "config.h"
#include "settings/warp_mode.h"
#include "patches/instant_warp_policy.h"
#include <cassert>
#include <iostream>

namespace
{
bool installed = false;
unsigned saves = 0;
std::string saved;
}

// Substitute only the game consumer and disk boundaries. Exercise the actual
// settings reader, mutation path and shortcut below.
Config::Config()
{ auto_confirm_instant_warp = InstantWarpConfirmation::None; }
Config& Config::Get()
{ static Config config; return config; }
bool InstantWarpConfirmationAvailable()
{ return installed; }
std::string_view notification_sound_name(NotificationSound)
{ return "none"; } // Unused audio members in the Config fixture.
namespace runtime_config
{
void SaveWarpMode(const char* mode) noexcept
{ ++saves; saved = mode; }
}

int main()
{
  auto& setting = mod_settings::WarpModeSetting().state();
  // Skipped or failed installation must leave both UI and shortcut unavailable.
  assert(!setting.Observe().state.known());
  assert(!mod_settings::SetWarpMode(InstantWarpConfirmation::Warp));
  mod_settings::CycleWarpMode();
  assert(saves == 0 && Config::Get().auto_confirm_instant_warp == InstantWarpConfirmation::None);
  installed = true;
  assert(setting.Observe().state.known());
  mod_settings::CycleWarpMode();
  assert(saves == 1 && saved == "warp" && Config::Get().auto_confirm_instant_warp == InstantWarpConfirmation::Warp);
  assert(mod_settings::SetWarpMode(InstantWarpConfirmation::Jump));
  assert(saves == 2 && saved == "jump");
  installed = false;
  assert(!setting.Observe().state.known());
  assert(!mod_settings::SetWarpMode(InstantWarpConfirmation::None));
  mod_settings::CycleWarpMode();
  assert(saves == 2 && Config::Get().auto_confirm_instant_warp == InstantWarpConfirmation::Jump);
  std::cout << "Warp mode consumer-availability and shortcut regression passed\n";
}
