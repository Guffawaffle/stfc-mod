#pragma once

#include "config.h"
#include "config_schema.h"

#include <string>
#include <string_view>
#include <vector>

struct SidecarRejectedSyncTarget {
  std::string target_name;
};

struct SidecarConfigParseResult {
  SidecarConfig                    config;
  AdvancedConfig                   advanced;
  std::vector<config_schema::Diagnostic> diagnostics;
  std::vector<SidecarRejectedSyncTarget> rejected_sync_targets;
  bool                             reject_legacy_sync_url = false;
};

[[nodiscard]] SidecarConfigParseResult ParseSidecarConfig(const toml::table& config);
void                                    WriteSidecarConfigRuntimeSnapshot(toml::table& runtime_config,
                                                                          const SidecarConfig& config);
void                                    WriteAdvancedConfigRuntimeSnapshot(toml::table& runtime_config,
                                                                           const AdvancedConfig& config);
void                                    OmitImplicitRuntimeTraceFromUserConfig(toml::table& user_config,
                                                                                bool runtime_trace_was_explicit);
[[nodiscard]] bool                      IsLoopbackSidecarSyncUrl(std::string_view url);
