#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <charconv>
#include <compare>
#include <cstdint>
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

struct ReleaseVersion {
  std::array<uint32_t, 6> parts{}; // Four base components, fork revision, DEV sequence.
  auto                    operator<=>(const ReleaseVersion&) const = default;
};

inline std::optional<ReleaseVersion> ParseReleaseTag(std::string_view tag, Channel channel)
{
  if (!tag.starts_with('v'))
    return {};
  tag.remove_prefix(1);
  ReleaseVersion                            version;
  constexpr std::array<std::string_view, 5> separators{".", ".", ".", "-guffa.", "-dev."};
  const size_t count = channel == Channel::Dev ? version.parts.size() : version.parts.size() - 1;
  for (size_t i = 0; i < count; ++i) {
    const auto separator = i + 1 == count ? std::string_view{} : separators[i];
    const auto end       = separator.empty() ? tag.size() : tag.find(separator);
    if (end == 0 || end == std::string_view::npos || (end > 1 && tag.front() == '0'))
      return {};
    const auto last   = tag.data() + end;
    const auto parsed = std::from_chars(tag.data(), last, version.parts[i]);
    if (parsed.ec != std::errc{} || parsed.ptr != last || (i >= 4 && version.parts[i] == 0))
      return {};
    tag.remove_prefix(end + separator.size());
  }
  return version;
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

inline bool IsNewerReleaseTag(std::string_view tag, std::string_view previous, Channel channel)
{
  const auto available = ParseReleaseTag(tag, channel);
  const auto offered   = ParseReleaseTag(previous, channel);
  return available && (!offered || *available > *offered);
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
  const auto& tag     = entry->get_ref<const std::string&>();
  const auto  version = ParseReleaseTag(tag, installed.channel);
  if (!version)
    return {};
  if (*version > installed.version)
    return {CheckStatus::Available, tag};
  return {CheckStatus::Current, {}};
}
} // namespace mod_updates
