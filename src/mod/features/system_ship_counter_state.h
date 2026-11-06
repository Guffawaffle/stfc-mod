#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace system_ship_counter
{
struct Scope {
  int64_t galaxy{}, system{};
  int32_t instance{};
  auto    operator<=>(const Scope&) const = default;
};

struct Observation {
  int64_t                id{};
  Scope                  address;
  int32_t                type{}, state{};
  bool                   destroyed{}, isActive{}, isLocal{};
  std::optional<int32_t> cloakstatus;
  std::optional<bool>    detected;
  std::optional<int32_t> removalReason;
};

struct Totals {
  size_t ships{}, own{}, cloaked{}, undetected_cloaked{}, cloak_unknown{};
  auto   operator<=>(const Totals&) const = default;
};

// Owns copied observations only. Native removal intent wins over the client's
// queue-retained cache until a new server receipt, never a render/state tick.
class Counter
{
public:
  void Reset()
  {
    scope_.reset();
    entries_.clear();
    removed_.clear();
    ready_ = false;
  }
  bool Focus(Scope scope)
  {
    if (scope_ == scope)
      return false;
    scope_ = scope;
    entries_.clear();
    ready_ = false;
    return true;
  }
  void Unavailable()
  { ready_ = false; }
  void Seed(const std::vector<Observation>& observations)
  {
    entries_.clear();
    for (const auto& observation : observations)
      Apply(observation);
    ready_ = scope_.has_value();
  }
  void Receipt(int64_t id)
  { removed_.erase(id); }
  void Remove(int64_t id)
  {
    if (id <= 0)
      return;
    entries_.erase(id);
    removed_.insert(id);
  }
  void Apply(const Observation& observation)
  {
    if (observation.id <= 0)
      return;
    if (!scope_ || removed_.contains(observation.id) || !Eligible(observation, *scope_)) {
      entries_.erase(observation.id);
      return;
    }
    entries_.insert_or_assign(observation.id, observation);
  }
  std::optional<Totals> Count() const
  {
    if (!ready_ || !scope_)
      return std::nullopt;
    Totals result;
    for (const auto& [id, observation] : entries_) {
      ++result.ships;
      result.own += observation.isLocal;
      if (!observation.cloakstatus)
        ++result.cloak_unknown;
      else if (*observation.cloakstatus == 1) {
        ++result.cloaked;
        result.undetected_cloaked += observation.detected && !*observation.detected;
      }
    }
    return result;
  }
  const std::optional<Scope>& CurrentScope() const
  { return scope_; }

private:
  static bool Eligible(const Observation& observation, const Scope& scope)
  {
    // Client274: Player1, Docked5. Warp/recall can still be in-system while
    // charging/approaching; the native full address establishes their scope.
    return observation.type == 1 && observation.address == scope && observation.isActive && !observation.destroyed
           && observation.state != 5 && (!observation.removalReason || *observation.removalReason == 0);
  }
  std::optional<Scope>                     scope_;
  std::unordered_map<int64_t, Observation> entries_;
  std::unordered_set<int64_t>              removed_;
  bool                                     ready_ = false;
};
} // namespace system_ship_counter
