#include "patches/parts/battle_log_analysis_data.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

namespace a = battle_analysis;

int main()
{
  a::Battle      battle;
  a::Participant local;
  local.ship  = 100;
  local.name  = "Local";
  local.local = true;
  a::Participant target;
  target.ship       = 200;
  target.name       = "Target";
  const auto player = battle.participant(local), npc = battle.participant(target);
  assert(battle.participant(local) == player);

  // Anonymous numerical fixture from the independently checked Armada export.
  // Fields: hull, shield, post-isolytic, standard mitigation, isolytic mitigation.
  constexpr std::int64_t fixture[][5] = {
      {218536016, 34106776, 119735712, 254577104, 1811485440}, {29725272, 4639204, 16286453, 34627580, 246398272},
      {29735180, 4640751, 16291883, 34639128, 246480432},      {220287808, 34380180, 120695520, 256617824, 1826006528},
      {152906256, 23863982, 83777216, 178123648, 1267468288},  {18459284, 2880929, 10113827, 21503600, 153012416},
      {15211277, 2374014, 8334247, 17719930, 126089096},       {13670284, 2133513, 7489938, 15924796, 113315520},
      {325441344, 50791424, 178309072, 379113344, 2697643776}, {354944000, 55395884, 194473552, 413481600, 2942196480}};
  for (std::size_t i = 0; i < std::size(fixture); ++i) {
    const auto& f = fixture[i];
    a::Event    e;
    e.kind      = a::Kind::Attack;
    e.actor     = player;
    e.target    = npc;
    e.round     = 1;
    e.sub_round = i < 4 ? 1 : 2;
    e.critical  = i == 0 || i == 3 || i == 4 || i == 8 || i == 9;
    e.damage    = a::reported_damage(f[0], f[1], f[2], f[3], f[4], 0, 0, 0);
    assert(battle.append(e));
  }
  a::Event incoming;
  incoming.kind      = a::Kind::Attack;
  incoming.actor     = npc;
  incoming.target    = player;
  incoming.round     = 1;
  incoming.sub_round = 2;
  incoming.destroyed = true;
  incoming.damage =
      a::reported_damage(29067880448LL, 137034293248LL, 135825055744LL, 5846572032LL, 1309122363392LL, 0, 0, 0);
  assert(battle.append(incoming));

  // Six additional ships must never inflate the local player's aggregates.
  for (int i = 0; i < 6; ++i) {
    a::Participant ally;
    ally.ship        = 300 + i;
    ally.name        = "Same display name";
    const auto index = battle.participant(ally);
    a::Event   e     = incoming;
    e.actor          = index;
    e.target         = npc;
    e.round          = 3;
    e.destroyed      = false;
    assert(battle.append(e));
  }
  const auto& p = battle.participants[player];
  assert(p.outgoing.shots == 10 && p.outgoing.critical == 5);
  assert(p.outgoing.hull == 1378916721.L && p.outgoing.shield == 215206657.L);
  assert(p.outgoing.landed() == 1594123378.L);
  assert(p.outgoing.standard == 2444944512.L && p.outgoing.isolytic == 12185603668.L);
  assert(p.outgoing.raw() == 14630548180.L);
  assert(p.outgoing.critical_percent() == 50.L);
  assert(std::abs(p.outgoing.average() - 159412337.8L) < .01L);
  assert(p.incoming.shots == 1 && p.incoming.landed() == 166102173696.L);
  assert(p.incoming.raw() == 1481071109120.L && p.destroyed_round == 1);
  assert(p.active_rounds == std::vector<int>{1});
  assert(battle.rounds == 3 && battle.local() == player);
  assert(battle.attacks(player, 2).empty());
  assert(battle.attacks(player, 1).size() == 10);
  assert(battle.events[3].sub_round == 1 && battle.events[4].sub_round == 2);
  assert(battle.peak_attack(player, false) == 9 && battle.peak_attack(player, true) == 9);
  assert(battle.timeline(player, 1, false).size() == 11);
  assert(battle.timeline(player, 3, false).empty());
  assert(battle.timeline(player, 3, true).size() == 6);

  // A second local ship remains a distinct selectable combatant.
  auto second      = local;
  second.ship      = 999;
  const auto other = battle.participant(second);
  assert(other != player);
  a::Event repair;
  repair.kind      = a::Kind::Repair;
  repair.actor     = other;
  repair.round     = 2;
  repair.sub_round = 3;
  repair.value     = 500;
  assert(battle.append(repair));
  assert(battle.participants[other].repairs == 500 && battle.participants[player].repairs == 0);

  a::Battle invalid;
  a::Event  wrong = incoming;
  wrong.actor     = 42;
  assert(!invalid.append(wrong) && invalid.failed && invalid.events.empty());
  assert(!a::valid(a::reported_damage(-1, 0, 0, 0, 0, 0, 0, 0)));
  a::Damage totals;
  auto      wide = a::reported_damage(INT64_MAX, INT64_MAX, 0, 0, 0, 0, 0, 0);
  a::add(totals, wide, true);
  a::add(totals, wide, false);
  assert(totals.landed() > static_cast<long double>(INT64_MAX) * 3);
  assert(a::Damage{}.average() == 0 && a::Damage{}.critical_percent() == 0);

  a::Battle fresh;
  assert(fresh.events.empty() && fresh.local() == a::none && fresh.rounds == 0);
  std::cout << "PASS: actual Armada totals, raw/landed columns, six allies, native positions, filters, pins, distinct "
               "ships, repairs, bounds and reset\n";
}
