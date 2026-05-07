/**
 * @file config_schema.h
 * @brief Small schema-driven helpers for loading typed config values.
 */
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.h>

namespace config_schema
{
enum class DiagnosticSeverity {
  Info,
  Warning,
  Error,
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Warning;
  std::string        path;
  std::string        source_path;
  std::string        message;
};

struct BoolSetting {
  std::string_view                  path;
  bool                              default_value = false;
  std::span<const std::string_view> aliases;
  std::string_view                  description;
};

struct BoolReadResult {
  bool                    value        = false;
  bool                    used_default = true;
  std::string             source_path;
  std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::string    make_path(std::string_view section, std::string_view key);
[[nodiscard]] bool           path_exists(const toml::table& config, std::string_view path);
[[nodiscard]] BoolReadResult read_bool(const toml::table& config, const BoolSetting& setting);
void                         write_bool(toml::table& config, std::string_view path, bool value);
} // namespace config_schema