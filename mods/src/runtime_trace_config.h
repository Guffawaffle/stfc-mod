#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

enum class RuntimeTraceLevel : uint8_t {
  Off = 0,
  Summary,
  Detailed,
  Verbose,
};

constexpr const char* RuntimeTraceLevelName(const RuntimeTraceLevel level)
{
  switch (level) {
    case RuntimeTraceLevel::Off:
      return "off";
    case RuntimeTraceLevel::Summary:
      return "summary";
    case RuntimeTraceLevel::Detailed:
      return "detailed";
    case RuntimeTraceLevel::Verbose:
      return "verbose";
  }

  return "off";
}

constexpr std::optional<RuntimeTraceLevel> ParseRuntimeTraceLevel(const std::string_view value)
{
  if (value == "off" || value == "false" || value == "0") {
    return RuntimeTraceLevel::Off;
  }

  if (value == "summary" || value == "basic" || value == "true" || value == "1") {
    return RuntimeTraceLevel::Summary;
  }

  if (value == "detailed" || value == "detail" || value == "timers") {
    return RuntimeTraceLevel::Detailed;
  }

  if (value == "verbose" || value == "all") {
    return RuntimeTraceLevel::Verbose;
  }

  return std::nullopt;
}