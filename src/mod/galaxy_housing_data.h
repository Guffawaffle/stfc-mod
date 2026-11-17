#pragma once
#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>
namespace galaxy_controls
{
struct HousingData {
  std::vector<int64_t>              housing, covered;
  static std::optional<HousingData> Parse(const unsigned char* bytes, size_t length)
  {
    try {
      const auto source = nlohmann::json::parse(bytes, bytes + length);
      if (source.at("schema_version") != 1)
        return std::nullopt;
      const auto read = [](const auto& values) -> std::optional<std::vector<int64_t>> {
        if (!values.is_array() || values.empty())
          return std::nullopt;
        std::vector<int64_t> result;
        result.reserve(values.size());
        for (const auto& value : values) {
          if (!value.is_number_integer() || (value.is_number_unsigned() && value.template get<uint64_t>() > INT64_MAX))
            return std::nullopt;
          const auto id = value.template get<int64_t>();
          if (id <= 0 || (!result.empty() && id <= result.back()))
            return std::nullopt;
          result.push_back(id);
        }
        return result;
      };
      auto housing = read(source.at("housing_system_ids")), covered = read(source.at("covered_system_ids"));
      if (!housing || !covered || !std::includes(covered->begin(), covered->end(), housing->begin(), housing->end()))
        return std::nullopt;
      return HousingData{std::move(*housing), std::move(*covered)};
    } catch (const nlohmann::json::exception&) {
      return std::nullopt;
    }
  }
  std::optional<bool> Contains(int64_t id) const
  {
    if (!std::binary_search(covered.begin(), covered.end(), id))
      return std::nullopt;
    return std::binary_search(housing.begin(), housing.end(), id);
  }
};
} // namespace galaxy_controls
