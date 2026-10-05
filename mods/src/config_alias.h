#pragma once
#include <string_view>
#include <toml++/toml.h>

// Aliases affect the resolved preference, never the disk-state snapshot used by
// the runtime writer's conflict checks. An explicit canonical key wins.
template <typename T>
T ConfigAliasDefault(const toml::table& config, std::string_view section, std::string_view key,
                     std::string_view alias_section, std::string_view alias, T fallback)
{ return config[section][key] ? fallback : config[alias_section][alias].value<T>().value_or(fallback); }
