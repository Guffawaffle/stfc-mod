#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pinned_ship_state
{
inline constexpr auto Key = "pinned_ship_ids";
inline constexpr auto LegacyNamesKey = "legacy_pinned_ship_names";

inline std::optional<std::vector<int64_t>> ReadOrder(const nlohmann::json& state)
{
  if (!state.is_object() || !state.contains(Key))
    return std::nullopt;

  std::vector<int64_t> result;
  const auto&          values = state[Key];
  if (!values.is_array())
    return std::nullopt;
  result.reserve(std::min<std::size_t>(values.size(), 2000));
  for (const auto& value : values) {
    if (!value.is_string() || result.size() == 2000)
      continue;
    const auto& text = value.get_ref<const std::string&>();
    int64_t     id   = 0;
    const auto  parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id > 0
        && std::find(result.begin(), result.end(), id) == result.end())
      result.push_back(id);
  }
  return result;
}

inline void WriteOrder(nlohmann::json& state, const std::vector<int64_t>& pins)
{
  auto values = nlohmann::json::array();
  for (const auto id : pins)
    values.push_back(std::to_string(id));
  state[Key] = std::move(values);
}

inline std::vector<std::string> ReadLegacyNames(const nlohmann::json& state)
{
  std::vector<std::string> result;
  if (!state.is_object() || !state.contains(LegacyNamesKey) || !state[LegacyNamesKey].is_array())
    return result;
  for (const auto& value : state[LegacyNamesKey]) {
    if (!value.is_string() || result.size() == 2000)
      continue;
    const auto& name = value.get_ref<const std::string&>();
    if (!name.empty() && std::find(result.begin(), result.end(), name) == result.end())
      result.push_back(name);
  }
  return result;
}

inline void WriteLegacyNames(nlohmann::json& state, const std::vector<std::string>& names)
{ state[LegacyNamesKey] = names; }
} // namespace pinned_ship_state
