#include "patches/action_queue_repair_config.h"

#include "defaultconfig.h"

KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config)
{
  KirsharaQueueRepairConfigParseResult result;

  const auto enabled = config_schema::read_bool(config, {kKirsharaQueueRepairEnabledPath,
                                                         DefaultConfig::Advanced::KirsharaQueue::enabled,
                                                         {},
                                                         "enable the Kir'shara queued-combat advancement repair"});

  result.config.enabled = enabled.value;
  result.diagnostics    = enabled.diagnostics;
  return result;
}

void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config)
{ config_schema::write_bool(runtime_config, kKirsharaQueueRepairEnabledPath, config.enabled); }

KirsharaQueueRepairInstallPlan BuildKirsharaQueueRepairInstallPlan(const bool enabled,
                                                                   const bool detailed_runtime_trace)
{
  if (!enabled) {
    return {};
  }

  return {
      .install_repair_hooks     = true,
      .emit_probe_logs          = detailed_runtime_trace,
      .install_diagnostic_hooks = false,
  };
}
