#pragma once

// Player-facing build labels are descriptive. Their presence is not signature or
// publisher verification; source provenance and signing checks remain separate.
#include "release_version.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace build_identity
{
namespace detail
{
  inline bool Number(std::string_view text, bool positive = true) noexcept
  { return mod_version::Number(text, mod_version::max_build_number, positive).has_value(); }
  inline bool Run(std::string_view text) noexcept
  {
    std::array<uint64_t, 2> run{};
    return mod_version::Components(text, run, mod_version::max_build_number, true);
  }
  inline std::string_view DisplayBaseOrVersion(std::string_view version) noexcept
  { return mod_version::DisplayBase(version).value_or(version); }
  inline bool UnsignedChannel(std::string_view channel) noexcept
  {
    if (channel == " · DEV local" || channel == " · CI local")
      return true;
    for (const auto prefix : {std::string_view{" · DEV b"}, std::string_view{" · CI b"}, std::string_view{" · Pre b"}})
      if (channel.starts_with(prefix))
        return Run(channel.substr(prefix.size()));
    return false;
  }
  inline char Hex(char value) noexcept
  {
    if (value >= '0' && value <= '9')
      return value;
    if (value >= 'a' && value <= 'f')
      return value;
    return value >= 'A' && value <= 'F' ? static_cast<char>(value - 'A' + 'a') : '\0';
  }
} // namespace detail

inline bool ValidDisplayVersion(std::string_view label, std::string_view base) noexcept
{
  constexpr std::string_view separator    = " · ";
  const auto                 display_base = mod_version::DisplayBase(base);
  if (label.size() > 160 || !display_base)
    return false;
  const auto space = label.find(separator);
  if (space == std::string_view::npos)
    return false;
  const auto version_text = label.substr(0, space);
  const auto channel      = label.substr(space);
  const bool independent  = display_base->size() != base.size();
  if (version_text == *display_base) {
    if (detail::UnsignedChannel(channel))
      return true;
    return independent && channel == " · Release";
  }
  const auto version = mod_version::Parse(version_text, mod_version::max_build_number);
  if (!version || (version->family == mod_version::Family::Independent) != independent
      || version_text.substr(0, version_text.find('-')) != *display_base)
    return false;
  using mod_version::Stage;
  if (!independent && version->stage == Stage::Release)
    return channel == " · Release" || channel == " · Pre" || detail::UnsignedChannel(channel);
  switch (version->stage) {
    case Stage::Release:
      return channel == " · Release";
    case Stage::Rc:
      return channel == " · Pre";
    case Stage::Dev:
      return channel == " · DEV";
    case Stage::Ci:
      return channel == " · CI";
  }
  return false;
}

inline bool SourceMatches(std::string_view claimed, std::string_view compiled) noexcept
{
  if (claimed.size() != 40 || (compiled.size() != 8 && compiled.size() != 40))
    return false;
  for (std::size_t i = 0; i < claimed.size(); ++i) {
    const auto value = detail::Hex(claimed[i]);
    if (!value || (i < compiled.size() && value != detail::Hex(compiled[i])))
      return false;
  }
  return true;
}

inline bool KnownSource(std::string_view source) noexcept
{
  if (source.size() != 8 && source.size() != 40)
    return false;
  for (const auto c : source)
    if (!detail::Hex(c))
      return false;
  return true;
}

// The same commit may produce a core CI payload and a development package.
// Source matching alone must not relabel one as the other.
inline bool MatchesUnsignedPackage(std::string_view label, bool development_package) noexcept
{
  if (label.ends_with(" · DEV") || label.find(" · DEV ") != std::string_view::npos)
    return development_package;
  if (label.ends_with(" · CI") || label.find(" · CI ") != std::string_view::npos)
    return !development_package;
  return true;
}

inline std::string FallbackVersion(std::string_view version, std::string_view invocation = {},
                                   bool development_package = false, std::string_view development_tag = {})
{
  version                      = detail::DisplayBaseOrVersion(version);
  const std::string prefix_tag = "v" + std::string(version) + "-dev.";
  if (development_package && development_tag.starts_with(prefix_tag)) {
    const auto parsed = mod_version::Parse(development_tag.substr(1), mod_version::max_build_number);
    if (parsed && parsed->stage == mod_version::Stage::Dev)
      return std::string(development_tag.substr(1)) + " · DEV";
  }
  const std::string_view     channel = development_package ? " · DEV " : " · CI ";
  constexpr std::string_view prefix = "github:", suffix = ":windows";
  if (invocation.size() >= prefix.size() + suffix.size() && invocation.starts_with(prefix)
      && invocation.ends_with(suffix)) {
    const auto run   = invocation.substr(prefix.size(), invocation.size() - prefix.size() - suffix.size());
    const auto colon = run.find(':');
    if (colon != std::string_view::npos && detail::Number(run.substr(0, colon))
        && detail::Number(run.substr(colon + 1)))
      return std::string(version) + std::string(channel) + "b" + std::string(run.substr(0, colon)) + "."
             + std::string(run.substr(colon + 1));
  }
  return std::string(version) + std::string(channel) + "local";
}

inline std::string HandoffVersion(std::string_view base, std::string_view label, std::string_view source,
                                  std::string_view compiled_source, std::string_view invocation = {},
                                  bool handoff_present = false, std::string_view fork_version = {},
                                  bool development_package = false)
{
  const auto known_version = detail::DisplayBaseOrVersion(fork_version.empty() ? base : fork_version);
  if (ValidDisplayVersion(label, base) && MatchesUnsignedPackage(label, development_package)
      && SourceMatches(source, compiled_source))
    return std::string(label);
  if (handoff_present || !label.empty() || !source.empty() || KnownSource(compiled_source))
    return std::string(known_version);
  return FallbackVersion(known_version, invocation, development_package);
}

// Embedded metadata presence is an observation, never publisher/trust validation.
// Present/unknown but unusable metadata leaves the channel unspecified.
enum class SignatureState { Absent, Present, Unknown };

inline std::string DescriptionVersion(std::string_view base, std::string_view description, SignatureState state,
                                      std::string_view invocation = {}, std::string_view fork_version = {},
                                      bool development_package = false, std::string_view development_tag = {})
{
  const auto                 known_version = detail::DisplayBaseOrVersion(fork_version.empty() ? base : fork_version);
  constexpr std::string_view prefix        = "STFC Community Mod | ";
  if (state == SignatureState::Present && description.starts_with(prefix)) {
    const auto label = description.substr(prefix.size());
    if (ValidDisplayVersion(label, base) && MatchesUnsignedPackage(label, development_package))
      return std::string(label);
  }
  if (state != SignatureState::Absent || !description.empty())
    return std::string(known_version);
  return FallbackVersion(known_version, invocation, development_package, development_tag);
}

// Cached on first use. No per-frame filesystem, signature, or environment work.
std::string_view DisplayVersion();
#if _WIN32
// The tiny platform entry unit already receives the immutable CI provenance
// defines. Reading it here avoids recompiling all mod sources per invocation.
std::string_view CompiledInvocation();
std::string_view CompiledDevelopmentTag();
#endif
} // namespace build_identity
