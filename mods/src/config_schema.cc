#include "config_schema.h"

#include <sstream>

namespace config_schema
{
namespace
{
  std::vector<std::string_view> split_path(std::string_view path)
  {
    std::vector<std::string_view> segments;
    size_t                        start = 0;
    while (start <= path.size()) {
      const auto end = path.find('.', start);
      if (end == std::string_view::npos) {
        const auto segment = path.substr(start);
        if (!segment.empty()) {
          segments.push_back(segment);
        }
        break;
      }

      const auto segment = path.substr(start, end - start);
      if (!segment.empty()) {
        segments.push_back(segment);
      }
      start = end + 1;
    }

    return segments;
  }

  const char* toml_type_name(toml::node_type type)
  {
    switch (type) {
      case toml::node_type::none:
        return "none";
      case toml::node_type::table:
        return "table";
      case toml::node_type::array:
        return "array";
      case toml::node_type::string:
        return "string";
      case toml::node_type::integer:
        return "integer";
      case toml::node_type::floating_point:
        return "float";
      case toml::node_type::boolean:
        return "boolean";
      case toml::node_type::date:
        return "date";
      case toml::node_type::time:
        return "time";
      case toml::node_type::date_time:
        return "date_time";
    }

    return "unknown";
  }

  const toml::node* node_at_path(const toml::table& config, std::string_view path)
  {
    const toml::node* current = &config;
    for (const auto segment : split_path(path)) {
      const auto* table = current ? current->as_table() : nullptr;
      if (!table) {
        return nullptr;
      }

      current = table->get(segment);
      if (!current) {
        return nullptr;
      }
    }

    return current;
  }

  struct BoolPathRead {
    bool        exists = false;
    bool        valid  = false;
    bool        value  = false;
    std::string type_name;
  };

  BoolPathRead read_bool_at_path(const toml::table& config, std::string_view path)
  {
    const auto* node = node_at_path(config, path);
    if (!node) {
      return {};
    }

    if (auto value = node->value<bool>(); value.has_value()) {
      return {true, true, value.value(), {}};
    }

    return {true, false, false, toml_type_name(node->type())};
  }

  Diagnostic make_diagnostic(DiagnosticSeverity severity, std::string_view path, std::string_view source_path,
                             std::string message)
  { return {severity, std::string(path), std::string(source_path), std::move(message)}; }

  std::string invalid_bool_message(std::string_view source_path, const BoolPathRead& read, std::string_view description)
  {
    std::ostringstream message;
    message << "Invalid boolean config " << source_path;
    if (!description.empty()) {
      message << " (" << description << ")";
    }
    message << ". Found " << read.type_name << "; using default.";
    return message.str();
  }
} // namespace

std::string make_path(std::string_view section, std::string_view key)
{
  std::string path(section);
  if (!path.empty() && !key.empty()) {
    path.push_back('.');
  }
  path.append(key);
  return path;
}

bool path_exists(const toml::table& config, std::string_view path)
{ return node_at_path(config, path) != nullptr; }

BoolReadResult read_bool(const toml::table& config, const BoolSetting& setting)
{
  BoolReadResult result;
  result.value = setting.default_value;

  const auto canonical = read_bool_at_path(config, setting.path);
  if (canonical.exists) {
    for (const auto alias : setting.aliases) {
      if (path_exists(config, alias)) {
        std::ostringstream message;
        message << "Ignoring deprecated config key " << alias << " because canonical key " << setting.path
                << " is set.";
        result.diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning, setting.path, alias, message.str()));
      }
    }

    if (canonical.valid) {
      result.value        = canonical.value;
      result.used_default = false;
      result.source_path  = std::string(setting.path);
      return result;
    }

    result.diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning, setting.path, setting.path,
                                                 invalid_bool_message(setting.path, canonical, setting.description)));
    return result;
  }

  for (const auto alias : setting.aliases) {
    const auto alias_read = read_bool_at_path(config, alias);
    if (!alias_read.exists) {
      continue;
    }

    if (alias_read.valid) {
      result.value        = alias_read.value;
      result.used_default = false;
      result.source_path  = std::string(alias);

      std::ostringstream message;
      message << "Deprecated config key " << alias << " is set. Use " << setting.path << " instead.";
      result.diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Info, setting.path, alias, message.str()));
      return result;
    }

    result.diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning, setting.path, alias,
                                                 invalid_bool_message(alias, alias_read, setting.description)));
  }

  return result;
}

void write_bool(toml::table& config, std::string_view path, bool value)
{
  const auto segments = split_path(path);
  if (segments.empty()) {
    return;
  }

  auto* table = &config;
  for (size_t index = 0; index + 1 < segments.size(); ++index) {
    const auto segment = std::string(segments[index]);
    auto*      node    = table->get(segment);
    if (!node || !node->is_table()) {
      table->insert_or_assign(segment, toml::table{});
      node = table->get(segment);
    }

    table = node->as_table();
    if (!table) {
      return;
    }
  }

  table->insert_or_assign(std::string(segments.back()), value);
}
} // namespace config_schema