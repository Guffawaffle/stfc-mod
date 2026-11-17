#pragma once

// An owned snapshot selected by the host's receipt policy before reading disk. This helper only overlays those
// protected values on a throw-away input table; it does not parse files, acknowledge saves or choose live timing.
#include <toml++/toml.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace runtime_config
{
struct ReloadValue {
  std::string                                           section, key;
  std::variant<bool, std::string, double, std::int64_t> value;
};
using ReloadSnapshot = std::vector<ReloadValue>;

inline void ApplyReloadSnapshot(toml::table& input, const ReloadSnapshot& snapshot)
{
  // Validate the entire known shape before changing the candidate. Allocation
  // failures can still interrupt assignment; callers must discard it on failure.
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    const auto& setting = snapshot[i];
    if (setting.section.empty() || setting.key.empty())
      throw std::invalid_argument("A protected setting has no registered identity");
    if (const auto* value = std::get_if<double>(&setting.value); value && !std::isfinite(*value))
      throw std::invalid_argument("A protected setting has no finite value");
    if (input.contains(setting.section) && !input.get_as<toml::table>(setting.section))
      throw std::invalid_argument("A protected setting section is not a table");
    for (std::size_t previous = 0; previous < i; ++previous)
      if (snapshot[previous].section == setting.section && snapshot[previous].key == setting.key)
        throw std::invalid_argument("A protected setting appears more than once");
  }
  for (const auto& setting : snapshot) {
    auto* section = input.get_as<toml::table>(setting.section);
    if (!section) {
      input.insert_or_assign(setting.section, toml::table{});
      section = input.get_as<toml::table>(setting.section);
    }
    // Both strings are literal path segments. A dot or Unicode key does not
    // become a different hierarchy, and false/zero/empty remain real values.
    std::visit([&](const auto& value) { section->insert_or_assign(setting.key, value); }, setting.value);
  }
}
} // namespace runtime_config
