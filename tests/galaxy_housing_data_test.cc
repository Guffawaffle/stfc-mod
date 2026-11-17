#include "galaxy_housing_data.h"
#include "galaxy_starbase_data.h"
#include <cassert>
#include <iostream>
#include <string_view>
std::optional<galaxy_controls::HousingData> Parse(std::string_view value)
{ return galaxy_controls::HousingData::Parse(reinterpret_cast<const unsigned char*>(value.data()), value.size()); }

std::optional<galaxy_controls::StarbaseData> ParseStarbase(std::string_view value)
{ return galaxy_controls::StarbaseData::Parse(reinterpret_cast<const unsigned char*>(value.data()), value.size()); }

void StarbaseDataCases()
{
  using galaxy_controls::PlasmaKindMask;
  auto data = ParseStarbase(
      R"({"schema_version":1,"collisional_system_ids":[54,9007199254740993,9223372036854775807],"magnetic_system_ids":[55,9007199254740993],"active_system_ids":[56,9007199254740993],"source":{"kind":"community-positive-list"},"covered_system_ids":[54,55,56,57]})");
  assert(data && data->Find(54) == std::optional(PlasmaKindMask::Collisional));
  assert(data->Find(55) == std::optional(PlasmaKindMask::Magnetic));
  assert(data->Find(56) == std::optional(PlasmaKindMask::Active));
  assert(data->Find(9007199254740993LL)
         == std::optional(PlasmaKindMask::Collisional | PlasmaKindMask::Magnetic | PlasmaKindMask::Active));
  assert(data->Find(INT64_MAX) == std::optional(PlasmaKindMask::Collisional));
  assert(!data->Find(9007199254740992LL) && !data->Find(9007199254740994LL));
  assert(!data->Find(0) && !data->Find(-1) && !data->Find(57) && !data->Find(999));

  auto one_kind = ParseStarbase(
      R"({"schema_version":1,"collisional_system_ids":[],"magnetic_system_ids":[55],"active_system_ids":[]})");
  assert(one_kind && one_kind->Find(55) == std::optional(PlasmaKindMask::Magnetic) && !one_kind->Find(54));
  assert(!galaxy_controls::StarbaseData::Parse(nullptr, 0));

  // A covered-but-no-housing system can independently be a storm positive.
  // Invalid/unavailable data for either feature must not alter the other parser.
  auto housing = Parse(R"({"schema_version":1,"housing_system_ids":[54],"covered_system_ids":[54,55,57]})");
  assert(housing && housing->Contains(55) == std::optional(false) && one_kind->Find(55).has_value());
  assert(housing->Contains(57) == std::optional(false) && !data->Find(57));
  assert(!Parse("{}") && one_kind->Find(55).has_value());
  assert(!ParseStarbase("{}") && housing->Contains(54) == std::optional(true));

  for (
      const auto bad : {
          "",
          "null",
          "[]",
          "{",
          R"({"schema_version":2,"collisional_system_ids":[54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1.0,"collisional_system_ids":[54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":true,"collisional_system_ids":[54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"collisional_system_ids":[54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[54],"magnetic_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[54,54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[],"magnetic_system_ids":[55,54],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[],"magnetic_system_ids":[],"active_system_ids":[0]})",
          R"({"schema_version":1,"collisional_system_ids":[-54],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":["54"],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[54.0],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[true],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[null],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[9223372036854775808],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[18446744073709551615],"magnetic_system_ids":[],"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[54],"magnetic_system_ids":null,"active_system_ids":[]})",
          R"({"schema_version":1,"collisional_system_ids":[54],"magnetic_system_ids":[],"active_system_ids":{}})",
      })
    assert(!ParseStarbase(bad));
  std::cout << "Starbase evidence: per-kind positives, unknown omissions, exact IDs and independent housing passed\n";
}
int main()
{
  auto valid = Parse(R"({"schema_version":1,"housing_system_ids":[54,71005],"covered_system_ids":[54,55,71005]})");
  assert(valid && valid->Contains(54) == std::optional(true));
  assert(valid->Contains(55) == std::optional(false) && !valid->Contains(999));
  for (const auto bad :
       {R"({"schema_version":2,"housing_system_ids":[54],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":[54,54],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":[55,54],"covered_system_ids":[54,55]})",
        R"({"schema_version":1,"housing_system_ids":[0],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":[55],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":["54"],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":[18446744073709551615],"covered_system_ids":[54]})",
        R"({"schema_version":1,"housing_system_ids":[],"covered_system_ids":[54]})"})
    assert(!Parse(bad));
  StarbaseDataCases();
  std::cout << "Housing evidence: positive/negative/unknown coverage, schema, ordering and exact IDs passed\n";
}
