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
  // Native effect context is the ability BuffSpec's short description, never
  // the owning technology item's generic name/description (IDs can collide).
  const auto effect = Key(NameKind::TechnologyEffect);
  assert(std::string_view(effect.method) == "GetBuffSpec" && effect.arguments == 2);
  assert(std::string_view(effect.identifier) == "forbidden_tech_short_desc_{0}");
  assert(std::string_view(effect.category) == "forbidden_tech");
  assert(PlainDescription("Increases Critical Hit Chance <color=#40FF60>15%</color>")
         == "Increases Critical Hit Chance 15%");
  assert(PlainDescription("Decreases opponent's Shield Mitigation every round <color=#40FF60>2.5%</color>")
         == "Decreases opponent's Shield Mitigation every round 2.5%");
  assert(PlainDescription("Increases base Weapon Damage against hostiles by <color=#40FF60>100%</color>.")
         == "Increases base Weapon Damage against hostiles by 100%.");
  assert(PlainDescription("<b>Решимость</b>: hull < 50%, x > y; <unknown>keep</unknown>; <color=")
         == "Решимость: hull < 50%, x > y; <unknown>keep</unknown>; <color=");
  assert(!IsResolvedName(PlainDescription("Boost <color=#40FF60>{0:#.#%}</color>"), effect.identifier, 70302));
  assert(!IsResolvedName("Missing forbidden_tech_short_desc_70302", effect.identifier, 70302));
  // A shared numeric ID is insufficient. Verify all three lookup results and
  // do not override a valid officer title or a missing/failed BuffSpec lookup.
  const NameLookup unavailable{"", 60011, NameStatus::TranslationMissing};
  const NameLookup hull{"U.S.S. ENTERPRISE-E", 60011, NameStatus::Resolved};
  const NameLookup ability{"Drawing The Line", 60011, NameStatus::Resolved};
  assert(CanUseShipAbility(unavailable, hull, ability));
  assert(!CanUseShipAbility({"Officer title", 60011, NameStatus::Resolved}, hull, ability));
  assert(!CanUseShipAbility({"", 60011, NameStatus::SpecLookupFailed}, hull, ability));
  assert(!CanUseShipAbility({"", 0, NameStatus::TranslationMissing}, hull, ability));
  assert(!CanUseShipAbility(unavailable, {"Other hull", 70301, NameStatus::Resolved}, ability));
  assert(!CanUseShipAbility(unavailable, hull, {"Other title", 76101, NameStatus::Resolved}));
  assert(!CanUseShipAbility(unavailable, {"", 60011, NameStatus::TranslationMissing}, ability));
  std::cout << "PASS: distinct native ability categories, optional spec argument, UTF-8 titles, unresolved/missing "
               "keys, placeholders, bounds, wide IDs, hull-verified fallback and readable technology effect text\n";
}
