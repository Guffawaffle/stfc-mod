#pragma once

// Player-facing build labels are descriptive. Their presence is not signature or
// publisher verification; source provenance and signing checks remain separate.
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

namespace build_identity
{
namespace detail
{
  inline bool Number(std::string_view text, bool positive = true) noexcept
  {
    if (text.empty() || text.size() > 19 || (text.size() > 1 && text.front() == '0'))
      return false;
    for (const auto c : text)
      if (c < '0' || c > '9')
        return false;
    uint64_t   value  = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size()
           && value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) && (!positive || value != 0);
  }
  inline bool Run(std::string_view text) noexcept
  {
    const auto dot = text.find('.');
    return dot != std::string_view::npos && Number(text.substr(0, dot)) && Number(text.substr(dot + 1));
  }
  inline bool Base(std::string_view text) noexcept
  {
    for (int component = 0; component < 4; ++component) {
      const auto dot = text.find('.');
      if ((component < 3) != (dot != std::string_view::npos) || !Number(text.substr(0, dot), false))
        return false;
      if (dot != std::string_view::npos)
        text.remove_prefix(dot + 1);
    }
    return true;
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
  constexpr std::string_view separator = " · ";
  if (label.size() > 160 || !detail::Base(base) || !label.starts_with(base))
    return false;
  label.remove_prefix(base.size());
  if (label.starts_with("-guffa.")) {
    label.remove_prefix(7);
    const auto space = label.find(separator);
    if (space == std::string_view::npos || !detail::Number(label.substr(0, space)))
      return false;
    const auto channel = label.substr(space + separator.size());
    return channel == "Release" || channel == "Pre";
  }
  if (label == " · DEV local")
    return true;
  for (const auto prefix : {std::string_view{" · DEV b"}, std::string_view{" · Pre b"}})
    if (label.starts_with(prefix))
      return detail::Run(label.substr(prefix.size()));
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

inline std::string FallbackVersion(std::string_view base, std::string_view invocation = {})
{
  constexpr std::string_view prefix = "github:", suffix = ":windows";
  if (invocation.size() >= prefix.size() + suffix.size() && invocation.starts_with(prefix)
      && invocation.ends_with(suffix)) {
    const auto run   = invocation.substr(prefix.size(), invocation.size() - prefix.size() - suffix.size());
    const auto colon = run.find(':');
    if (colon != std::string_view::npos && detail::Number(run.substr(0, colon))
        && detail::Number(run.substr(colon + 1)))
      return std::string(base) + " · DEV b" + std::string(run.substr(0, colon)) + "."
             + std::string(run.substr(colon + 1));
  }
  return std::string(base) + " · DEV local";
}

inline std::string HandoffVersion(std::string_view base, std::string_view label, std::string_view source,
                                  std::string_view compiled_source, std::string_view invocation = {},
                                  bool handoff_present = false)
{
  if (ValidDisplayVersion(label, base) && SourceMatches(source, compiled_source))
    return std::string(label);
  if (handoff_present || !label.empty() || !source.empty() || KnownSource(compiled_source))
    return std::string(base);
  return FallbackVersion(base, invocation);
}

// Embedded metadata presence is an observation, never publisher/trust validation.
// Present/unknown but unusable metadata leaves the channel unspecified (base only).
enum class SignatureState { Absent, Present, Unknown };

inline std::string DescriptionVersion(std::string_view base, std::string_view description, SignatureState state,
                                      std::string_view invocation = {})
{
  constexpr std::string_view prefix = "STFC Community Mod | ";
  if (state == SignatureState::Present && description.starts_with(prefix)) {
    const auto label = description.substr(prefix.size());
    if (ValidDisplayVersion(label, base))
      return std::string(label);
  }
  if (state != SignatureState::Absent || !description.empty())
    return std::string(base);
  return FallbackVersion(base, invocation);
}

// Cached on first use. No per-frame filesystem, signature, or environment work.
std::string_view DisplayVersion();
#if _WIN32
// The tiny platform entry unit already receives the immutable CI provenance
// defines. Reading it here avoids recompiling all mod sources per invocation.
std::string_view CompiledInvocation();
#endif
} // namespace build_identity
