#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace battle_analysis
{
constexpr std::size_t none       = std::numeric_limits<std::size_t>::max();
constexpr std::size_t max_events = 200000, max_participants = 128;

enum class Kind { Attack, Charging, Officer, Technology, Repair, Taunt, HullMelt, Other };

struct Damage {
  long double   hull = 0, shield = 0, standard = 0, isolytic = 0;
  long double   mitigated_standard = 0, mitigated_isolytic = 0, barrier = 0, critical_mitigation = 0;
  std::uint64_t shots = 0, critical = 0;
  long double   landed() const
  { return hull + shield; }
  long double raw() const
  { return standard + isolytic; }
  long double average(bool pre_mitigation = false) const
  { return shots ? (pre_mitigation ? raw() : landed()) / shots : 0; }
  long double critical_percent() const
  { return shots ? 100.L * critical / shots : 0; }
};

struct Event {
  Kind         kind  = Kind::Other;
  int          round = 0, sub_round = 0;
  std::size_t  actor = none, target = none;
  std::int64_t weapon = 0, ability = 0, owner = 0;
  bool         critical = false, destroyed = false, shield_depleted = false;
  Damage       damage;
  long double  value = 0;
  std::string  description, source;
};

struct Participant {
  std::int64_t     ship = 0, hull_id = 0;
  std::string      name, ship_name;
  bool             local = false, allied = false;
  int              destroyed_round = 0;
  long double      repairs         = 0;
  std::uint64_t    repair_events   = 0;
  Damage           outgoing, incoming;
  std::vector<int> active_rounds;
};

struct Contribution {
  std::size_t participant = none;
  bool        allied      = false;
  std::size_t rank        = 0;
  long double share       = 0;
};

inline bool valid(const Damage& d)
{
  for (auto x : {d.hull, d.shield, d.standard, d.isolytic, d.mitigated_standard, d.mitigated_isolytic, d.barrier,
                 d.critical_mitigation})
    if (!std::isfinite(x) || x < 0)
      return false;
  return true;
}

inline void add(Damage& total, const Damage& event, bool critical)
{
  total.hull += event.hull;
  total.shield += event.shield;
  total.standard += event.standard;
  total.isolytic += event.isolytic;
  total.mitigated_standard += event.mitigated_standard;
  total.mitigated_isolytic += event.mitigated_isolytic;
  total.barrier += event.barrier;
  total.critical_mitigation += event.critical_mitigation;
  ++total.shots;
  total.critical += critical;
}

// Mirrors the client's reported damage columns, using wide intermediate sums.
// The isolytic value in an attack event is post-mitigation damage. Mitigation
// categories overlap and must not be summed into a second mitigation percentage.
inline Damage reported_damage(std::int64_t hull, std::int64_t shield, std::int64_t post_isolytic,
                              std::int64_t standard_mitigation, std::int64_t isolytic_mitigation, std::int64_t barrier,
                              std::int64_t standard_critical, std::int64_t isolytic_critical)
{
  Damage d;
  if (hull < 0 || shield < 0 || post_isolytic < 0 || standard_mitigation < 0 || isolytic_mitigation < 0 || barrier < 0
      || standard_critical < 0 || isolytic_critical < 0) {
    d.standard = -1;
    return d;
  }
  d.hull                = hull;
  d.shield              = shield;
  d.standard            = std::max(0.L, d.landed() - post_isolytic + standard_mitigation + barrier + standard_critical);
  d.isolytic            = static_cast<long double>(post_isolytic) + isolytic_mitigation;
  d.mitigated_standard  = standard_mitigation;
  d.mitigated_isolytic  = isolytic_mitigation;
  d.barrier             = barrier;
  d.critical_mitigation = static_cast<long double>(standard_critical) + isolytic_critical;
  return d;
}

struct Battle {
  std::vector<Participant> participants;
  std::vector<Event>       events;
  int                      rounds = 0;
  bool                     failed = false;

  std::size_t participant(Participant p)
  {
    // Native ship tokens identify individual combatants, even when one player
    // owns multiple ships or multiple NPCs have the same hull/localized name.
    for (std::size_t i = 0; i < participants.size(); ++i)
      if (participants[i].ship == p.ship)
        return i;
    if (participants.size() >= max_participants) {
      failed = true;
      return none;
    }
    participants.push_back(std::move(p));
    return participants.size() - 1;
  }

  bool append(Event e)
  {
    if (events.size() >= max_events || e.round < 1 || e.sub_round < 1
        || (e.actor != none && e.actor >= participants.size()) || (e.target != none && e.target >= participants.size())
        || !std::isfinite(e.value) || (e.kind == Kind::Repair && e.value < 0)) {
      failed = true;
      return false;
    }
    if (e.kind == Kind::Attack) {
      if (e.actor == none || e.target == none || !valid(e.damage)) {
        failed = true;
        return false;
      }
      auto& a = participants[e.actor];
      auto& t = participants[e.target];
      add(a.outgoing, e.damage, e.critical);
      add(t.incoming, e.damage, e.critical);
      if (std::find(a.active_rounds.begin(), a.active_rounds.end(), e.round) == a.active_rounds.end())
        a.active_rounds.push_back(e.round);
      if (e.destroyed)
        t.destroyed_round = e.round;
    } else if (e.kind == Kind::Repair && e.actor != none) {
      participants[e.actor].repairs += e.value;
      ++participants[e.actor].repair_events;
    }
    rounds = std::max(rounds, e.round);
    events.push_back(std::move(e));
    return true;
  }

  std::vector<std::size_t> attacks(std::size_t actor, int round = 0) const
  {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < events.size(); ++i)
      if (events[i].kind == Kind::Attack && events[i].actor == actor && (!round || events[i].round == round))
        indices.push_back(i);
    return indices;
  }

  std::vector<std::size_t> timeline(std::size_t participant, int round, bool all) const
  {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < events.size(); ++i) {
      const auto& e = events[i];
      if ((!round || e.round == round) && (all || e.actor == participant || e.target == participant))
        indices.push_back(i);
    }
    return indices;
  }

  std::size_t local() const
  {
    for (std::size_t i = 0; i < participants.size(); ++i)
      if (participants[i].local)
        return i;
    return none;
  }

  // Rank each side independently: enemy damage is never part of allied share.
  // This query uses only recorded totals and does not depend on UI selection.
  std::vector<Contribution> contributions(bool raw) const
  {
    std::vector<Contribution> rows;
    long double               totals[2]{};
    const auto amount = [raw](const Participant& p) { return raw ? p.outgoing.raw() : p.outgoing.landed(); };
    for (std::size_t i = 0; i < participants.size(); ++i) {
      const bool allied = participants[i].local || participants[i].allied;
      rows.push_back({i, allied});
      totals[allied] += amount(participants[i]);
    }
    std::stable_sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
      if (a.allied != b.allied)
        return a.allied > b.allied;
      return amount(participants[a.participant]) > amount(participants[b.participant]);
    });
    std::size_t ranks[2]{};
    for (auto& row : rows) {
      row.rank  = ++ranks[row.allied];
      row.share = totals[row.allied] > 0 ? 100 * amount(participants[row.participant]) / totals[row.allied] : 0;
    }
    return rows;
  }

  std::size_t peak_attack(std::size_t actor, bool raw) const
  {
    std::size_t best    = none;
    long double maximum = -1;
    for (std::size_t i = 0; i < events.size(); ++i) {
      const auto& e     = events[i];
      const auto  value = raw ? e.damage.raw() : e.damage.landed();
      if (e.kind == Kind::Attack && e.actor == actor && value > maximum) {
        best    = i;
        maximum = value;
      }
    }
    return best;
  }
};
} // namespace battle_analysis
