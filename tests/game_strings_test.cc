#include "game_strings_data.h"

#include <cassert>
#include <iostream>

int main()
{
  using namespace game_strings;
  const auto officer = Key(NameKind::OfficerAbility), ship = Key(NameKind::ShipAbility);
  // The two native contexts can share a numeric ID without sharing a title.
  assert(std::string_view(officer.category) != ship.category);
  assert(std::string_view(officer.identifier) != ship.identifier);
  assert(officer.arguments == 2 && ship.arguments == 2); // GetBuffSpec's optional bool is still a native argument.
  assert(!Key(static_cast<NameKind>(99)).method);
  assert(IsResolvedName("The Boimler Effect", officer.identifier, 123));
  assert(IsResolvedName("Yay, Friends!", officer.identifier, 123));
  assert(IsResolvedName("[PRO] NEURAL NEUTRALIZER", Key(NameKind::ForbiddenTech).identifier, 123));
  assert(IsResolvedName("Решимость", ship.identifier, 123));
  assert(!IsResolvedName("", officer.identifier, 123));
  assert(!IsResolvedName("  \r\n\t", officer.identifier, 123));
  assert(!IsResolvedName("officer_ability_name_{0}", officer.identifier, 123));
  assert(!IsResolvedName("officer_ability_name_123", officer.identifier, 123));
  assert(!IsResolvedName("Missing translation: officer_ability_name_123", officer.identifier, 123));
  assert(!IsResolvedName("Boost {0:0.#%}", officer.identifier, 123));
  assert(!IsResolvedName("ship_ability_name_9223372036854775807", ship.identifier, INT64_MAX));
  assert(!IsResolvedName(std::string(4097, 'x'), officer.identifier, 123));
  assert(!IsResolvedName("A title", "", 123));
  std::cout << "PASS: distinct native ability categories, optional spec argument, UTF-8 titles, unresolved/missing "
               "keys, placeholders, bounds and wide IDs\n";
}
