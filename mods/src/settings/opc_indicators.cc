#include "opc_indicators.h"
#include "config.h"
#include "patches/opc_indicators.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
BooleanSetting& OpcHighlightSetting()
{
  static BooleanSetting setting({
      "community_mod.ui.highlight_opc_fleets", "Highlight over-protected cargo",
      [] { return OpcHighlightAvailable() ? ReadResult::Known(Config::Get().highlight_opc_fleets, 1) : ReadResult{}; },
      [](bool enabled, std::uint64_t generation) {
        if (generation != 1 || !OpcHighlightAvailable()) return ApplyResult::Rejected;
        Config::Get().highlight_opc_fleets = enabled;
        RefreshOpcIndicatorSettings();
        runtime_config::SaveSetting("ui", "highlight_opc_fleets", enabled);
        return ApplyResult::Applied;
      }});
  return setting;
}

BooleanSetting& OpcEtaSetting()
{
  static BooleanSetting setting({
      "community_mod.ui.fleet_hud_opc_eta", "Time until over-protected cargo",
      [] { return OpcEtaAvailable() ? ReadResult::Known(Config::Get().fleet_hud_opc_eta, 1) : ReadResult{}; },
      [](bool enabled, std::uint64_t generation) {
        if (generation != 1 || !OpcEtaAvailable()) return ApplyResult::Rejected;
        Config::Get().fleet_hud_opc_eta = enabled;
        RefreshOpcIndicatorSettings();
        runtime_config::SaveSetting("ui", "fleet_hud_opc_eta", enabled);
        return ApplyResult::Applied;
      }});
  return setting;
}
}
