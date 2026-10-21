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
    ally.allied      = true;
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
  assert(fresh.contributions(false).empty());

  // Contribution excludes the opposing side, follows raw/landed mode, and
  // keeps individual ship identities even with identical names or zero damage.
  a::Battle  ranking;
  const auto friendly = ranking.participant(local);
  auto       ally     = target;
  ally.ship           = 201;
  ally.allied         = true;
  const auto allied   = ranking.participant(ally);
  const auto enemy    = ranking.participant(target);
  const auto idle     = ranking.participant(second); // Local is friendly even without the allied flag.
  const auto attack   = [&](std::size_t actor, std::size_t target, long double landed, long double raw) {
    a::Event e;
    e.kind   = a::Kind::Attack;
    e.actor  = actor;
    e.target = target;
    e.round = e.sub_round = 1;
    e.damage.hull         = landed;
    e.damage.standard     = raw;
    assert(ranking.append(e));
  };
  attack(friendly, enemy, 75, 75);
  attack(allied, enemy, 25, 125);
  attack(enemy, friendly, 1000, 2000);
  auto ranked = ranking.contributions(false);
  assert(ranked.size() == 4);
  assert(ranked[0].participant == friendly && ranked[0].rank == 1 && ranked[0].share == 75);
  assert(ranked[1].participant == allied && ranked[1].rank == 2 && ranked[1].share == 25);
  assert(ranked[2].participant == idle && ranked[2].allied && ranked[2].share == 0);
  assert(ranked[3].participant == enemy && !ranked[3].allied && ranked[3].rank == 1 && ranked[3].share == 100);
  ranked = ranking.contributions(true);
  assert(ranked[0].participant == allied && ranked[0].share == 62.5L);
  assert(ranked[1].participant == friendly && ranked[1].share == 37.5L);
  assert(ranking.local() == friendly); // Query never changes selected/local identity.
  a::Battle zero;
  zero.participant(local);
  zero.participant(second);
  zero.participant(target);
  ranked = zero.contributions(false);
  assert(ranked[0].participant == 0 && ranked[1].participant == 1); // Stable ties.
  for (const auto& row : ranked)
    assert(row.share == 0 && std::isfinite(row.share));

  a::Battle  pattern;
  const auto shooter = pattern.participant(local), opponent = pattern.participant(target);
  assert(pattern.record_sub_rounds(1, 3) && pattern.record_sub_rounds(2, 2));
  assert(pattern.sub_round_count == 5 && pattern.rounds == 2);
  a::Event shot;
  shot.kind        = a::Kind::Attack;
  shot.actor       = shooter;
  shot.target      = opponent;
  shot.round       = 1;
  shot.sub_round   = 2;
  shot.damage.hull = shot.damage.standard = 10;
  assert(pattern.append(shot));
  shot.critical = true;
  assert(pattern.append(shot)); // Multiple shots remain individually inspectable.
  shot.actor     = opponent;
  shot.target    = shooter;
  shot.sub_round = 3;
  assert(pattern.append(shot)); // Other ship firing is still a gap for the selected ship.
  shot.actor     = shooter;
  shot.target    = opponent;
  shot.round     = 2;
  shot.sub_round = 1; // Sub-round numbering restarts with the new round.
  assert(pattern.append(shot));
  auto history = pattern.firing_history(shooter);
  assert(history.size() == 6);
  assert(history[0].round == 1 && history[0].sub_round == 1 && history[0].attack == a::none);
  assert(history[1].attack == 0 && history[2].attack == 1);
  assert(history[3].round == 1 && history[3].sub_round == 3 && history[3].attack == a::none);
  assert(history[4].round == 2 && history[4].sub_round == 1 && history[4].attack == 3);
  assert(history[5].round == 2 && history[5].sub_round == 2 && history[5].attack == a::none);
  const auto round_two = pattern.firing_history(shooter, 2);
  assert(round_two.size() == 2 && round_two[0].attack == 3 && round_two[0].sub_round == 1);
  assert(pattern.firing_history(a::none).empty() && pattern.firing_history(shooter, 3).empty());
  assert(pattern.participants[shooter].outgoing.shots == 3);
  assert(pattern.participants[shooter].outgoing.critical == 2); // Gaps do not change rates or averages.
  assert(pattern.participants[shooter].outgoing.average() == 10);
  assert(pattern.record_sub_rounds(3, 2));
  const auto no_shots = pattern.firing_history(shooter, 3);
  assert(no_shots.size() == 2 && no_shots[0].attack == a::none && no_shots[1].attack == a::none);
  assert(pattern.record_sub_rounds(3, 1) && pattern.sub_round_count == 6); // Count replacement, not double counting.
  assert(pattern.record_sub_rounds(4, 0) && pattern.firing_history(shooter, 4).empty());
  auto missing_metadata = pattern;
  missing_metadata.sub_rounds.clear();
  missing_metadata.sub_round_count = 0;
  assert(missing_metadata.firing_history(shooter).size() == 3); // No fabricated gaps without phase evidence.
  std::swap(missing_metadata.events[0], missing_metadata.events[3]);
  const auto reordered = missing_metadata.firing_history(shooter);
  assert(reordered[0].round == 1 && reordered.back().round == 2);
  a::Battle phase_limit;
  phase_limit.participant(local);
  assert(phase_limit.record_sub_rounds(1, static_cast<int>(a::max_events)));
  const auto full_quiet_round = phase_limit.firing_history(0);
  assert(full_quiet_round.size() == a::max_events && full_quiet_round.back().sub_round == a::max_events);
  assert(!phase_limit.record_sub_rounds(2, 1) && phase_limit.failed);
  std::cout << "PASS: actual Armada totals, raw/landed columns, six allies, native positions, filters, pins, distinct "
               "ships, repairs, bounds, per-side contribution/ranking, round-local firing gaps and reset\n";
}
