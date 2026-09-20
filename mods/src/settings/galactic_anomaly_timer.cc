#include "galactic_anomaly_timer.h"
#include "config.h"
#include "patches/galactic_anomaly_timer.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
BooleanSetting& GalacticAnomalyTimerSetting()
{
  static BooleanSetting setting({
      "community_mod.navigation.galactic_anomaly_timer", "Galactic Anomaly countdown",
      [] {
        return GalacticAnomalyTimerAvailable() ? ReadResult::Known(Config::Get().galactic_anomaly_timer, 1)
                                              : ReadResult{};
      },
      [](bool enabled, std::uint64_t generation) {
        if (generation != 1 || !GalacticAnomalyTimerAvailable()) return ApplyResult::Rejected;
        Config::Get().galactic_anomaly_timer = enabled;
        runtime_config::SaveSetting("graphics", "galactic_anomaly_timer", enabled);
        return ApplyResult::Applied;
      }});
  return setting;
}
}
