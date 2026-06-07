#pragma once

#include "config_schema.h"

#include <string_view>
#include <vector>

#include <toml++/toml.h>

struct KirsharaQueueRepairConfig {
  bool enabled = false;
};

struct KirsharaQueueRepairConfigParseResult {
  KirsharaQueueRepairConfig              config;
  std::vector<config_schema::Diagnostic> diagnostics;
};

struct KirsharaQueueRepairInstallPlan {
  bool install_repair_hooks     = false;
  bool emit_probe_logs          = false;
  bool install_diagnostic_hooks = false;
};

constexpr std::string_view kKirsharaQueueRepairEnabledPath = "advanced.kirshara_queue.enabled";
constexpr int              kKirsharaQueueRepairHookCount   = 3;

[[nodiscard]] KirsharaQueueRepairConfigParseResult ParseKirsharaQueueRepairConfig(const toml::table& config);
void WriteKirsharaQueueRepairRuntimeSnapshot(toml::table& runtime_config, const KirsharaQueueRepairConfig& config);
[[nodiscard]] KirsharaQueueRepairInstallPlan BuildKirsharaQueueRepairInstallPlan(bool enabled,
                                                                                 bool detailed_runtime_trace);
