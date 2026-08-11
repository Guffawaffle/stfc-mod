#include "patches/sidecar_local_ingest_policy.h"

#include <algorithm>

bool SidecarLocalNamedPipeNameValid(const std::string_view value)
{
  return !value.empty() && value.size() <= 128 && std::ranges::all_of(value, [](const char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
        || (character >= '0' && character <= '9') || character == '.' || character == '-' || character == '_';
  });
}

bool SidecarLocalNamedPipeCredentialValid(const std::string_view value)
{
  constexpr std::string_view valid_final_characters = "AEIMQUYcgkosw048";
  return value.size() == 43
      && std::ranges::all_of(value, [](const char character) {
           return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
               || (character >= '0' && character <= '9') || character == '-' || character == '_';
         })
      && valid_final_characters.contains(value.back());
}

bool SidecarLocalSyncUsesNamedPipe(const SidecarSyncConfig& config)
{ return config.transport == "named_pipe"; }

bool SidecarLocalSyncTransportReady(const SidecarSyncConfig& config)
{
  if (!config.enabled || config.token.empty()) {
    return false;
  }
  if (SidecarLocalSyncUsesNamedPipe(config)) {
#if _WIN32
    return SidecarLocalNamedPipeNameValid(config.pipe_name) && SidecarLocalNamedPipeCredentialValid(config.token);
#else
    return false;
#endif
  }
  return config.transport == "legacy_http" && !config.url.empty();
}

bool SidecarLocalSyncEnabledFor(const SidecarSyncConfig& config, const SidecarLocalIngestKind kind)
{
  if (!SidecarLocalSyncTransportReady(config)) {
    return false;
  }

  switch (kind) {
    case SidecarLocalIngestKind::BattleEvents:
      return config.battlelogs_realtime;
    case SidecarLocalIngestKind::FleetRuntime:
      return config.fleet_runtime;
  }

  return false;
}

bool BattleHeaderProcessingNeedsSidecarLocal(const SidecarSyncConfig& config)
{ return SidecarLocalSyncEnabledFor(config, SidecarLocalIngestKind::BattleEvents); }

bool BattleHeaderProcessingEnabledForSync(const bool sync_battlelogs, const bool sidecar_logging_jsonl,
                                          const bool               external_battles_enabled,
                                          const bool               external_battlelogs_realtime_enabled,
                                          const SidecarSyncConfig& sidecar_sync)
{
  return sync_battlelogs || sidecar_logging_jsonl || external_battles_enabled || external_battlelogs_realtime_enabled
         || BattleHeaderProcessingNeedsSidecarLocal(sidecar_sync);
}
