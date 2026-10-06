#include "queue_recovery.h"
#include "config.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
BooleanSetting& QueueRecoverySetting()
{
  static BooleanSetting setting({"community_mod.control.faster_queue_recovery", "Faster Kirshara queue recovery",
                                 [] {
                                   return QueueRecoveryAvailable()
                                              ? ReadResult::Known(Config::Get().faster_queue_recovery, 1)
                                              : ReadResult{};
                                 },
                                 [](bool value, std::uint64_t generation) {
                                   if (generation != 1 || !QueueRecoveryAvailable())
                                     return ApplyResult::Rejected;
                                   // Clear weak requests on either transition so a re-enable cannot consume old work.
                                   ResetQueueRecovery();
                                   Config::Get().faster_queue_recovery = value;
                                   runtime_config::SaveSetting("control", "faster_queue_recovery", value);
                                   return ApplyResult::Applied;
                                 }});
  return setting;
}
} // namespace mod_settings
