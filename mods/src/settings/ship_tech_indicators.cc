#include "ship_tech_indicators.h"
#include "config.h"
#include "patches/runtime_config.h"
#include "patches/ship_tech_indicators.h"

namespace mod_settings
{
BooleanSetting& ShipTechIndicatorSetting()
{
  static BooleanSetting setting({"community_mod.ui.show_ship_tech_indicators", "Show equipped FT/CT",
                                 [] {
                                   return ship_tech_indicators::Available()
                                              ? ReadResult::Known(Config::Get().show_ship_tech_indicators, 1)
                                              : ReadResult{};
                                 },
                                 [](bool enabled, std::uint64_t generation) {
                                   if (generation != 1 || !ship_tech_indicators::Available())
                                     return ApplyResult::Rejected;
                                   Config::Get().show_ship_tech_indicators = enabled;
                                   runtime_config::SaveSetting("ui", "show_ship_tech_indicators", enabled);
                                   return ApplyResult::Applied;
                                 }});
  return setting;
}
} // namespace mod_settings
