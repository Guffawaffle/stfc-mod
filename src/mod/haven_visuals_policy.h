#pragma once

// Haven-only discovery and restoration. Native adapters supply checked
// observations and local strong custody; persistent ownership is always weak.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace haven_visuals
{
inline constexpr std::size_t kRoadLimit = 256, kWaterLimit = 8;
enum class AccessState { Unavailable, Gone, Ready };

// Protect the whole native discovery/application episode, not just the owned
// mutation registry. A time throttle cannot suppress delayed synchronous reentry.
template <class F> bool RunExclusive(bool& busy, F&& action)
{
  if (busy)
    return false;
  busy = true;
  struct Guard {
    bool& busy;
    ~Guard()
    { busy = false; }
  } guard{busy};
  std::forward<F>(action)();
  return true;
}

// Both callers share one owner/classification pass. Looking up the path again
// also detects an active old owner replaced by another still-live object.
template <class Backend> class SnapshotCache
{
public:
  using Clock = std::chrono::steady_clock;
  using Weak  = typename Backend::Weak;

  void Refresh(Backend& backend, Clock::time_point now)
  {
    try {
      const auto current = backend.GetOwner(owner_);
      if (!current)
        Clear();
      if (now < next_)
        return;
      next_            = now + std::chrono::seconds(5);
      const auto found = backend.FindOwner();
      if (!found) {
        Clear();
        return;
      }
      if (found != current) {
        Clear();
        owner_ = backend.Retain(found);
        if (!owner_)
          return;
      }
      std::vector<Weak> roads, water;
      if (!backend.ScanOwner(found, roads, water) || roads.size() > kRoadLimit || water.size() > kWaterLimit) {
        roads_.clear();
        water_.clear();
        return;
      }
      roads_ = std::move(roads);
      water_ = std::move(water);
    } catch (...) {
      // Any failed observation/publication invalidates the entire discovery
      // snapshot. Restoration ownership is separate and must survive this.
      next_ = now + std::chrono::seconds(5);
      Clear();
      throw;
    }
  }
  const std::vector<Weak>& Roads() const
  { return roads_; }
  const std::vector<Weak>& Water() const
  { return water_; }
  void Clear()
  {
    owner_ = {};
    roads_.clear();
    water_.clear();
  }

private:
  Weak              owner_;
  std::vector<Weak> roads_, water_;
  Clock::time_point next_{};
};

class WaterRestoration
{
public:
  template <class Access> void Apply(Access& access, bool hidden)
  {
    const auto before = access.ReadEnabled();
    if (!before)
      return;
    if (hidden && !original_)
      original_ = before; // A failed setter can already have mutated the object.
    if (!original_)
      return;
    const bool target = hidden ? false : *before || *original_;
    if (*before != target)
      (void)access.WriteEnabled(target);
    const auto after = access.ReadEnabled();
    // Release never disables another owner's enable. A successful setter alone
    // says nothing about actual native state; only matching readback confirms.
    if (!hidden && after && *after == target)
      original_.reset();
  }
  bool Pending() const
  { return original_.has_value(); }

private:
  std::optional<bool> original_;
};

class RoadRestoration
{
public:
  template <class Access> void Apply(Access& access, std::size_t property, bool expanded)
  {
    const auto before = access.ReadDepth(property);
    if (!before || !std::isfinite(*before))
      return;
    float target;
    if (expanded) {
      target = std::min(*before, -1.0f);
      if (target == *before)
        return;
      if (!original_)
        original_ = before;
      applied_ = target; // Retain both values before a possibly mutating failure.
    } else {
      if (!original_)
        return;
      if (*before != *original_ && *before != applied_) {
        // A known different edit supersedes our intervention, not a restoration.
        original_.reset();
        return;
      }
      target = *original_;
    }
    if (*before != target)
      (void)access.WriteDepth(property, target);
    const auto after = access.ReadDepth(property);
    if (!expanded && after && std::isfinite(*after) && *after == target)
      original_.reset();
  }
  bool Pending() const
  { return original_.has_value(); }

private:
  std::optional<float> original_;
  float                applied_ = -1.0f;
};

// Access owns a local strong root and distinguishes unavailable acquisition from
// confirmed loss. Its reads/writes recheck that same object's native lifetime.
// Id is a checked signed observation; Token is the actual weak/rooted identity.
template <class Backend> class OwnedVisuals
{
public:
  using Weak   = typename Backend::Weak;
  using Access = decltype(std::declval<Backend&>().Acquire(std::declval<const Weak&>()));

  void Apply(Backend& backend, std::span<const Weak> roads, std::span<const Weak> water, bool expanded, bool hidden)
  {
    if (applying_)
      return;
    struct Guard {
      bool& active;
      ~Guard()
      { active = false; }
    } guard{applying_};
    applying_ = true;
    // Oversized observations do not establish a complete owned cohort. Do not
    // publish/apply a prefix; old interventions may only attempt restoration.
    if (roads.size() > kRoadLimit || water.size() > kWaterLimit) {
      roads    = {};
      water    = {};
      expanded = hidden = false;
    }
    Update(backend, roads_, roads, expanded, kRoadLimit);
    Update(backend, water_, water, hidden, kWaterLimit);
  }
  bool Pending() const
  { return !roads_.empty() || !water_.empty(); }

private:
  struct RoadState {
    std::array<RoadRestoration, 2> properties;
    void                           Apply(Access& access, bool active)
    {
      if (active) {
        // A new/reapplied material override requires the complete native pair.
        // Restoration remains independent so a failed property cannot discard
        // the other property's already confirmed progress.
        for (std::size_t i = 0; i < properties.size(); ++i) {
          const auto observed = access.ReadDepth(i);
          if (!observed || !std::isfinite(*observed))
            return;
        }
      }
      for (std::size_t i = 0; i < properties.size(); ++i)
        properties[i].Apply(access, i, active);
    }
    bool Pending() const
    { return properties[0].Pending() || properties[1].Pending(); }
  };
  template <class State> struct Entry {
    std::int32_t id;
    Weak         weak;
    State        state;
  };
  struct Current {
    std::int32_t id;
    Access       access;
    bool         handled = false;
  };
  template <class State>
  void Update(Backend& backend, std::vector<Entry<State>>& owned, std::span<const Weak> members, bool active,
              std::size_t limit)
  {
    std::vector<Current> current;
    if (active) {
      for (const auto& weak : members) {
        auto access = backend.Acquire(weak);
        if (access.State() != AccessState::Ready || !access.Token())
          continue;
        const auto id = access.Id();
        if (!id || std::ranges::any_of(current, [&](const auto& item) {
              return item.id == *id && item.access.Token() == access.Token();
            }))
          continue;
        current.push_back({*id, std::move(access)});
      }
    }
    for (auto it = owned.begin(); it != owned.end();) {
      auto access = backend.Acquire(it->weak);
      if (access.State() == AccessState::Gone) {
        it = owned.erase(it);
        continue;
      }
      const auto id = access.State() == AccessState::Ready ? access.Id() : std::nullopt;
      if (!id) {
        // Do not rebase an unresolved existing obligation through a fresh
        // acquisition of the same numeric identity in this pass.
        for (auto& item : current)
          if (item.id == it->id)
            item.handled = true;
        ++it;
        continue;
      }
      if (*id != it->id) {
        it = owned.erase(it); // The old identity cannot authorize a new object.
        continue;
      }
      auto found = std::ranges::find_if(
          current, [&](const auto& item) { return item.id == it->id && item.access.Token() == access.Token(); });
      const bool retained = found != current.end();
      if (retained) {
        found->handled = true;
        it->state.Apply(found->access, true);
      } else {
        it->state.Apply(access, false);
      }
      if (!it->state.Pending())
        it = owned.erase(it);
      else
        ++it;
    }
    for (auto& item : current) {
      if (item.handled || owned.size() == limit)
        continue;
      auto weak = backend.Retain(item.access);
      if (!weak)
        continue;
      owned.push_back({item.id, std::move(weak), {}});
      owned.back().state.Apply(item.access, true);
      if (!owned.back().state.Pending())
        owned.pop_back();
    }
  }

  std::vector<Entry<RoadState>>        roads_;
  std::vector<Entry<WaterRestoration>> water_;
  bool                                 applying_ = false;
};
} // namespace haven_visuals
