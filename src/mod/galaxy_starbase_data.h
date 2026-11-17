#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>
#include <vector>

namespace galaxy_controls
{
enum class PlasmaKindMask : uint8_t {
  Collisional = 1,
  Magnetic    = 2,
  Active      = 4,
};

constexpr PlasmaKindMask operator|(PlasmaKindMask left, PlasmaKindMask right)
{ return static_cast<PlasmaKindMask>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right)); }

// The community catalog supplies positives only. Public name/ID mapping coverage
// does not establish negative membership for any system omitted from this data.
struct StarbaseData {
  std::vector<int64_t> collisional, magnetic, active;

  static std::optional<StarbaseData> Parse(const unsigned char* bytes, size_t length)
  {
    if (!bytes || !length)
      return std::nullopt;
    try {
      const auto source = nlohmann::json::parse(bytes, bytes + length);
      if (!source.is_object() || !source.at("schema_version").is_number_integer() || source.at("schema_version") != 1)
        return std::nullopt;
      const auto read = [](const auto& values) -> std::optional<std::vector<int64_t>> {
        if (!values.is_array())
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
      auto collisional = read(source.at("collisional_system_ids"));
      auto magnetic    = read(source.at("magnetic_system_ids"));
      auto active      = read(source.at("active_system_ids"));
      if (!collisional || !magnetic || !active || (collisional->empty() && magnetic->empty() && active->empty()))
        return std::nullopt;
      // Provenance fields are documentary metadata, as in HousingData; only the
      // explicit per-kind positive arrays determine runtime membership.
      return StarbaseData{std::move(*collisional), std::move(*magnetic), std::move(*active)};
    } catch (const nlohmann::json::exception&) {
      return std::nullopt;
    }
  }

  std::optional<PlasmaKindMask> Find(int64_t id) const
  {
    if (id <= 0)
      return std::nullopt;
    uint8_t kinds = 0;
    if (std::binary_search(collisional.begin(), collisional.end(), id))
      kinds |= static_cast<uint8_t>(PlasmaKindMask::Collisional);
    if (std::binary_search(magnetic.begin(), magnetic.end(), id))
      kinds |= static_cast<uint8_t>(PlasmaKindMask::Magnetic);
    if (std::binary_search(active.begin(), active.end(), id))
      kinds |= static_cast<uint8_t>(PlasmaKindMask::Active);
    return kinds ? std::optional(static_cast<PlasmaKindMask>(kinds)) : std::nullopt;
  }
};
} // namespace galaxy_controls
