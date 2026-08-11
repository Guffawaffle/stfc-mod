/**
 * @file sync_capability_snapshot.cc
 * @brief Emits a startup capability/config snapshot to Majel-envelope sync targets.
 */
#include "patches/sync_capability_snapshot.h"

#include "config.h"
#include "patches/sync_scheduler.h"
#include "patches/sync_transport_policy.h"
#include "version.h"

#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace
{
const char* current_platform_name()
{
#if _WIN32
  return "windows";
#elif __APPLE__
  return "macos";
#elif __linux__
  return "linux";
#else
  return "unknown";
#endif
}

std::vector<std::string> enabled_sync_type_names(const SyncTargetConfig& config)
{
  std::vector<std::string> names;
  names.reserve(SyncOptions.size());
  for (const auto& option : SyncOptions) {
    if (http::SyncTargetAcceptsType(config, option.type)) {
      names.push_back(std::string(option.type_str));
    }
  }

  return names;
}

std::vector<http::SyncTargetCapabilityInfo> majel_capability_targets(const Config& config)
{
  std::vector<http::SyncTargetCapabilityInfo> targets;

  for (const auto& [name, target] : config.sync_targets
       | std::views::filter([](const auto& item) { return http::SyncTargetUsesMajelEnvelope(item.second.mode); })) {
    targets.push_back({
        .name               = name,
        .mode               = target.mode,
        .enabled_sync_types = enabled_sync_type_names(target),
    });
  }

  return targets;
}
} // namespace

void queue_mod_capability_snapshot()
{
  const auto& config  = Config::Get();
  auto        targets = majel_capability_targets(config);
  if (targets.empty()) {
    return;
  }

  const auto snapshot = http::BuildModCapabilitySnapshot({
      .source_version = VER_RUNTIME_VERSION_STR,
      .platform       = current_platform_name(),
      .targets        = std::move(targets),
      .supported_schemas = http::MajelAdvertisedSchemas(),
  });

  queue_data(SyncConfig::Type::ModCapabilities, snapshot, true);
}
