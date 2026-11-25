#pragma once

#include "release_version.h"

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mod_updates
{
enum class Channel { Release, Pre, Dev };

inline std::string_view ChannelKey(Channel channel)
{
  switch (channel) {
    case Channel::Release:
      return "release";
    case Channel::Pre:
      return "pre";
    case Channel::Dev:
      return "dev";
  }
  return {};
}

using ReleaseVersion = mod_version::Version;

inline std::optional<ReleaseVersion> ParseReleaseTag(std::string_view tag, Channel channel)
{
  if (!tag.starts_with('v'))
    return {};
  const auto version = mod_version::Parse(tag.substr(1));
  if (!version)
    return {};
  using mod_version::Family;
  using mod_version::Stage;
  const auto expected = channel == Channel::Release         ? Stage::Release
                        : channel == Channel::Dev           ? Stage::Dev
                        : version->family == Family::Legacy ? Stage::Release
                                                            : Stage::Rc;
  return version->stage == expected ? version : std::nullopt;
}

struct InstalledRelease {
  Channel        channel;
  ReleaseVersion version;
};

inline std::optional<InstalledRelease> IdentifyRelease(std::string_view display)
{
  constexpr std::array labels{std::pair{Channel::Release, std::string_view{" · Release"}},
                              std::pair{Channel::Pre, std::string_view{" · Pre"}},
                              std::pair{Channel::Dev, std::string_view{" · DEV"}}};
  for (const auto& [channel, suffix] : labels) {
    if (!display.ends_with(suffix))
      continue;
    const auto tag = "v" + std::string(display.substr(0, display.size() - suffix.size()));
    if (const auto version = ParseReleaseTag(tag, channel))
      return InstalledRelease{channel, *version};
  }
  return {};
}

enum class CheckStatus { Idle, Current, Available, Failed };

struct CheckResult {
  CheckStatus status = CheckStatus::Failed;
  std::string tag;
};

inline bool IsIgnoredUpdate(const CheckResult& result, std::string_view ignored_tag)
{ return result.status == CheckStatus::Available && !ignored_tag.empty() && result.tag == ignored_tag; }

inline CheckResult RestoreCheck(std::string_view tag, const InstalledRelease& installed)
{
  const auto available = ParseReleaseTag(tag, installed.channel);
  if (!available)
    return {CheckStatus::Idle, {}};
  return {*available > installed.version ? CheckStatus::Available : CheckStatus::Current, std::string(tag)};
}

inline constexpr size_t max_index_bytes = 4096;

inline CheckResult CheckIndex(std::string_view body, const InstalledRelease& installed)
{
  if (body.empty() || body.size() > max_index_bytes)
    return {};
  const auto index = nlohmann::json::parse(body, nullptr, false);
  if (!index.is_object() || !index.contains("schema") || !index["schema"].is_number_integer() || index["schema"] != 1
      || !index.contains("channels") || !index["channels"].is_object())
    return {};
  const auto& channels = index["channels"];
  const auto  entry    = channels.find(ChannelKey(installed.channel));
  if (entry == channels.end() || !entry->is_string())
    return {};
  auto result = RestoreCheck(entry->get_ref<const std::string&>(), installed);
  return result.status == CheckStatus::Idle ? CheckResult{} : std::move(result);
}
} // namespace mod_updates
