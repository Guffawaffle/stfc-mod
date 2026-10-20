#pragma once

#include <cmath>
#include <cstdint>

namespace battle_summary
{
// Actual hull/shield damage, not pre-mitigation damage. Wide accumulators keep
// battle totals from overflowing an individual event's signed 64-bit amount.
struct Fire {
  std::uint64_t shots = 0, critical = 0;
  long double   hull = 0, shield = 0;
  long double   landed() const
  { return hull + shield; }
  long double average() const
  { return shots ? landed() / shots : 0; }
  long double critical_percent() const
  { return shots ? 100.0L * critical / shots : 0; }
};

struct Summary {
  Fire          outgoing, incoming;
  int           rounds        = 0;
  long double   repairs       = 0;
  std::uint64_t repair_events = 0, rejected = 0;
  bool          local_identity_seen = false;

  bool attack(bool local_attacker, bool local_target, bool critical, std::int64_t hull, std::int64_t shield)
  {
    local_identity_seen |= local_attacker || local_target;
    if (!local_attacker && !local_target)
      return true;
    if (hull < 0 || shield < 0) {
      ++rejected;
      return false;
    }
    auto add = [&](Fire& fire) {
      ++fire.shots;
      fire.critical += critical;
      fire.hull += hull;
      fire.shield += shield;
    };
    if (local_attacker)
      add(outgoing);
    if (local_target)
      add(incoming);
    return true;
  }

  bool repair(bool local_ship, float amount)
  {
    local_identity_seen |= local_ship;
    if (!local_ship)
      return true;
    if (!std::isfinite(amount) || amount < 0) {
      ++rejected;
      return false;
    }
    repairs += amount;
    ++repair_events;
    return true;
  }

  long double per_round() const
  { return rounds > 0 ? outgoing.landed() / rounds : 0; }
};
} // namespace battle_summary
