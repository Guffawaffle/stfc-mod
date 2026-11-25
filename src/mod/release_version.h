#pragma once

#include <array>
#include <charconv>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace mod_version
{
inline constexpr uint64_t max_version_number = std::numeric_limits<uint32_t>::max();
inline constexpr uint64_t max_build_number   = std::numeric_limits<int64_t>::max();

inline std::optional<uint64_t> Number(std::string_view text, uint64_t maximum, bool positive = true) noexcept
{
  if (text.empty() || (text.size() > 1 && text.front() == '0'))
    return {};
  uint64_t   value  = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > maximum
      || (positive && value == 0))
    return {};
  return value;
}

inline bool Components(std::string_view text, std::span<uint64_t> parts, uint64_t maximum,
                       bool positive = false) noexcept
{
  for (size_t i = 0; i < parts.size(); ++i) {
    const auto dot = text.find('.');
    if ((i + 1 < parts.size()) != (dot != std::string_view::npos))
      return false;
    const auto value = Number(text.substr(0, dot), maximum, positive);
    if (!value)
      return false;
    parts[i] = *value;
    if (dot != std::string_view::npos)
      text.remove_prefix(dot + 1);
  }
  return true;
}

enum class Family { Legacy, Independent };
enum class Stage { Ci, Dev, Rc, Release };

struct Version {
  Family                  family = Family::Legacy;
  std::array<uint64_t, 4> base{};
  uint64_t                lineage  = 0;
  Stage                   stage    = Stage::Release;
  uint64_t                sequence = 0;
  uint64_t                attempt  = 0;

  std::strong_ordering operator<=>(const Version& other) const noexcept
  {
    if (const auto order = family <=> other.family; order != 0)
      return order;
    if (const auto order = base <=> other.base; order != 0)
      return order;
    if (family == Family::Legacy) {
      if (const auto order = lineage <=> other.lineage; order != 0)
        return order;
      return sequence <=> other.sequence;
    }
    if (const auto order = stage <=> other.stage; order != 0)
      return order;
    if (const auto order = sequence <=> other.sequence; order != 0)
      return order;
    return attempt <=> other.attempt;
  }
  bool operator==(const Version& other) const noexcept
  { return (*this <=> other) == 0; }
};

// Legacy display metadata historically allowed int64 components; updater tags
// retain their uint32 bound. Independent versions share one strict grammar.
inline std::optional<Version> Parse(std::string_view text, uint64_t legacy_maximum = max_version_number) noexcept
{
  Version    version;
  const auto fork = text.find("-guffa.");
  if (fork != std::string_view::npos) {
    if (!Components(text.substr(0, fork), version.base, legacy_maximum))
      return {};
    text.remove_prefix(fork + 7);
    const auto dev     = text.find("-dev.");
    const auto lineage = Number(text.substr(0, dev), legacy_maximum);
    if (!lineage)
      return {};
    version.lineage = *lineage;
    if (dev != std::string_view::npos) {
      const auto sequence = Number(text.substr(dev + 5), legacy_maximum);
      if (!sequence)
        return {};
      version.stage    = Stage::Dev;
      version.sequence = *sequence;
    }
    return version;
  }

  const auto suffix = text.find('-');
  if (!Components(text.substr(0, suffix), std::span{version.base}.first<3>(), max_version_number)
      || version.base[0] < 2)
    return {};
  version.family = Family::Independent;
  if (suffix == std::string_view::npos)
    return version;
  text.remove_prefix(suffix + 1);
  if (text.starts_with("ci.")) {
    std::array<uint64_t, 2> run{};
    if (!Components(text.substr(3), run, max_build_number, true))
      return {};
    version.stage    = Stage::Ci;
    version.sequence = run[0];
    version.attempt  = run[1];
    return version;
  }
  if (text.starts_with("rc."))
    version.stage = Stage::Rc;
  else if (text.starts_with("dev."))
    version.stage = Stage::Dev;
  else
    return {};
  const auto sequence = Number(text.substr(version.stage == Stage::Rc ? 3 : 4), max_version_number);
  if (!sequence)
    return {};
  version.sequence = *sequence;
  return version;
}

// Windows keeps a four-component numeric resource. Independent releases expose
// its three-component product version and reserve the fourth component as zero.
inline std::optional<std::string_view> DisplayBase(std::string_view base) noexcept
{
  std::array<uint64_t, 4> parts{};
  if (!Components(base, parts, max_build_number))
    return {};
  if (parts[0] < 2)
    return base;
  for (const auto part : parts)
    if (part > max_version_number)
      return {};
  if (parts[3] != 0)
    return {};
  return base.substr(0, base.size() - 2);
}
} // namespace mod_version
