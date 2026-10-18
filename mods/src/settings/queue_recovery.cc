#include "queue_recovery.h"
#include "config.h"
#include "patches/parts/action_queue.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
bool KirsharaFixesAvailable()
{ return QueueRecoveryAvailable() || ThinQueueProtectionAvailable() || QueueAddressGuardAvailable(); }

BooleanSetting& KirsharaFixesSetting()
{
  static BooleanSetting setting({"community_mod.control.kirshara_fixes", "Kirshara queue fixes",
                                 [] {
                                   return KirsharaFixesAvailable()
                                              ? ReadResult::Known(Config::Get().kirshara_fixes, 1)
                                              : ReadResult{};
                                 },
                                 [](bool value, std::uint64_t generation) {
                                   if (generation != 1 || !KirsharaFixesAvailable())
                                     return ApplyResult::Rejected;
                                   // Clear weak requests on either transition so a re-enable cannot consume old work.
                                   ResetQueueRecoveryTracking();
                                   Config::Get().kirshara_fixes = value;
                                   runtime_config::SaveSetting("control", "kirshara_fixes", value);
                                   return ApplyResult::Applied;
                                 }});
  return setting;
}
} // namespace mod_settings
