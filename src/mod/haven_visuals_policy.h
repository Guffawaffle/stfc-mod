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

// Native lifecycle/material events request a new shared classification pass.
// Successful snapshots have no timer; only failed observations use a retry delay.
template <class Backend> class SnapshotCache
{
public:
  using Clock = std::chrono::steady_clock;
  using Weak  = typename Backend::Weak;

  std::uint64_t Epoch() const
  { return requested_; }
  void Invalidate(bool ready = false)
  {
    if (++requested_ == 0)
      ++requested_; // Zero is reserved for an unattempted/unpublished generation.
    ready_ = ready;
    ++readiness_revision_;
    // A native callback can invalidate while a consumer holds spans into these
    // vectors. Withdraw authority now, but retire storage only in a safe consumer.
  }
  void SetReady(std::uint64_t epoch, bool ready)
  {
    if (epoch != requested_ || ready == ready_)
      return;
    ready_ = ready;
    ++readiness_revision_;
    published_ = attempted_ = 0;
  }
  bool IsReady() const
  { return ready_; }
  bool IsPublished() const
  { return ready_ && published_ == requested_; }
  const Weak& Owner() const
  { return owner_; }

  bool Refresh(Backend& backend, Clock::time_point now)
  {
    if (!ready_)
      return false;
    if (IsPublished())
      return true;
    if (attempted_ == requested_ && attempted_revision_ == readiness_revision_ && now < retry_)
      return false;
    const auto epoch = requested_, revision = readiness_revision_;
    attempted_          = epoch;
    attempted_revision_ = revision;
    retry_              = now + std::chrono::seconds(5);
    try {
      const auto current = backend.GetOwner(owner_);
      if (!current)
        Retire();
      const auto found = backend.FindOwner();
      if (!found) {
        Retire();
        return false;
      }
      if (found != current) {
        Retire();
        owner_ = backend.Retain(found);
        if (!owner_)
          return false;
      }
      std::vector<Weak> roads, water;
      if (!backend.ScanOwner(found, roads, water) || roads.size() > kRoadLimit || water.size() > kWaterLimit) {
        roads_.clear();
        water_.clear();
        return false;
      }
      if (epoch != requested_ || revision != readiness_revision_ || !ready_) {
        Retire();
        return false; // A nested native event superseded this complete scan.
      }
      roads_     = std::move(roads);
      water_     = std::move(water);
      published_ = epoch;
      return true;
    } catch (...) {
      // Any failed observation/publication invalidates the entire discovery
      // snapshot. Restoration ownership is separate and must survive this.
      Retire();
      throw;
    }
  }
  std::span<const Weak> Roads() const
  { return IsPublished() ? std::span<const Weak>{roads_} : std::span<const Weak>{}; }
  std::span<const Weak> Water() const
  { return IsPublished() ? std::span<const Weak>{water_} : std::span<const Weak>{}; }
  void Clear()
  {
    Invalidate();
    Retire();
  }

private:
  void Retire()
  {
    owner_ = {};
    roads_.clear();
    water_.clear();
    published_ = 0;
  }
  Weak              owner_;
  std::vector<Weak> roads_, water_;
  Clock::time_point retry_{};
  std::uint64_t     requested_ = 1, attempted_ = 0, published_ = 0;
  std::uint64_t     readiness_revision_ = 0, attempted_revision_ = 0;
  bool              ready_ = false;
};

class WaterRestoration
{
public:
  template <class Access> bool Apply(Access& access, bool hidden)
  {
    if (!hidden && !original_)
      return true;
    const auto before = access.ReadEnabled();
    if (!before)
      return false;
    if (hidden && !original_)
      original_ = before; // A failed setter can already have mutated the object.
    if (!original_)
      return true;
    const bool target = hidden ? false : *before || *original_;
    if (*before != target)
      (void)access.WriteEnabled(target);
    const auto after = access.ReadEnabled();
    // Release never disables another owner's enable. A successful setter alone
    // says nothing about actual native state; only matching readback confirms.
    const bool verified = after && *after == target;
    if (!hidden && verified)
      original_.reset();
    return verified;
  }
  bool Pending() const
  { return original_.has_value(); }

private:
  std::optional<bool> original_;
};

class RoadRestoration
{
public:
  template <class Access> bool Apply(Access& access, std::size_t property, bool expanded)
  {
    if (!expanded && !original_)
      return true;
    const auto before = access.ReadDepth(property);
    if (!before || !std::isfinite(*before))
      return false;
    float target;
    if (expanded) {
      target = std::min(*before, -1.0f);
      if (target == *before)
        return true;
      if (!original_)
        original_ = before;
      applied_ = target; // Retain both values before a possibly mutating failure.
    } else {
      if (!original_)
        return true;
      if (*before != *original_ && *before != applied_) {
        // A known different edit supersedes our intervention, not a restoration.
        original_.reset();
        return true;
      }
      target = *original_;
    }
    if (*before != target)
      (void)access.WriteDepth(property, target);
    const auto after    = access.ReadDepth(property);
    const bool verified = after && std::isfinite(*after) && *after == target;
    if (!expanded && verified)
      original_.reset();
    return verified;
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

  bool Apply(Backend& backend, std::span<const Weak> roads, std::span<const Weak> water, bool expanded, bool hidden)
  {
    if (applying_)
      return false;
    struct Guard {
      bool& active;
      ~Guard()
      { active = false; }
    } guard{applying_};
    applying_ = true;
    // Oversized observations do not establish a complete owned cohort. Do not
    // publish/apply a prefix; old interventions may only attempt restoration.
    const bool admitted = roads.size() <= kRoadLimit && water.size() <= kWaterLimit;
    if (!admitted) {
      roads    = {};
      water    = {};
      expanded = hidden = false;
    }
    const bool roads_verified = Update(backend, roads_, roads, expanded, kRoadLimit);
    const bool water_verified = Update(backend, water_, water, hidden, kWaterLimit);
    return admitted && roads_verified && water_verified;
  }
  bool Pending() const
  { return !roads_.empty() || !water_.empty(); }

private:
  struct RoadState {
    std::array<RoadRestoration, 2> properties;
    bool                           Apply(Access& access, bool active)
    {
      if (active) {
        // A new/reapplied material override requires the complete native pair.
        // Restoration remains independent so a failed property cannot discard
        // the other property's already confirmed progress.
        for (std::size_t i = 0; i < properties.size(); ++i) {
          const auto observed = access.ReadDepth(i);
          if (!observed || !std::isfinite(*observed))
            return false;
        }
      }
      bool verified = true;
      for (std::size_t i = 0; i < properties.size(); ++i)
        verified = properties[i].Apply(access, i, active) && verified;
      return verified;
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
  bool Update(Backend& backend, std::vector<Entry<State>>& owned, std::span<const Weak> members, bool active,
              std::size_t limit)
  {
    bool                 verified = true;
    std::vector<Current> current;
    if (active) {
      for (const auto& weak : members) {
        auto access = backend.Acquire(weak);
        if (access.State() == AccessState::Gone)
          continue;
        if (access.State() != AccessState::Ready || !access.Token()) {
          verified = false;
          continue;
        }
        const auto id = access.Id();
        if (!id) {
          verified = false;
          continue;
        }
        if (std::ranges::any_of(
                current, [&](const auto& item) { return item.id == *id && item.access.Token() == access.Token(); }))
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
        verified = false;
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
        verified       = it->state.Apply(found->access, true) && verified;
      } else {
        verified = it->state.Apply(access, false) && verified;
      }
      if (!it->state.Pending())
        it = owned.erase(it);
      else
        ++it;
    }
    for (auto& item : current) {
      if (item.handled)
        continue;
      if (owned.size() == limit) {
        verified = false;
        continue;
      }
      auto weak = backend.Retain(item.access);
      if (!weak) {
        verified = false;
        continue;
      }
      owned.push_back({item.id, std::move(weak), {}});
      verified = owned.back().state.Apply(item.access, true) && verified;
      if (!owned.back().state.Pending())
        owned.pop_back();
    }
    return verified;
  }

  std::vector<Entry<RoadState>>        roads_;
  std::vector<Entry<WaterRestoration>> water_;
  bool                                 applying_ = false;
};
} // namespace haven_visuals
