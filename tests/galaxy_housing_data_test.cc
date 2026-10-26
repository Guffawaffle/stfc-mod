#include "galaxy_housing_data.h"
#include <cassert>
#include <iostream>
#include <string_view>
std::optional<galaxy_controls::HousingData> Parse(std::string_view value)
{ return galaxy_controls::HousingData::Parse(reinterpret_cast<const unsigned char*>(value.data()), value.size()); }
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
  std::cout << "Housing evidence: positive/negative/unknown coverage, schema, ordering and exact IDs passed\n";
}
