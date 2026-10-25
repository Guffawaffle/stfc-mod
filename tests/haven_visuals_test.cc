#include "haven_visuals_policy.h"

#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
void Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
struct NativeFailure {
};
struct Object : std::enable_shared_from_this<Object> {
  std::int32_t         id    = 0;
  bool                 alive = true, active = true, alive_readable = true, id_readable = true;
  bool                 root_failure = false, weak_failure = false;
  bool                 enabled = true, enabled_readable = true;
  std::array<float, 2> depth{0.0f, 0.0f};
  std::array<bool, 2>  depth_readable{true, true};
  // Index zero controls enabled; indices one/two control the depth properties.
  std::array<bool, 3>              mutate{true, true, true}, success{true, true, true};
  std::array<bool, 3>              lose_readback{}, throw_after{};
  std::array<int, 3>               writes{};
  std::function<void(std::size_t)> during_write;
};
struct Weak {
  static inline int     handles = 0;
  std::weak_ptr<Object> weak;
  bool                  allocated = false;
  Weak()                          = default;
  explicit Weak(const std::shared_ptr<Object>& object)
      : weak(object)
      , allocated(true)
  { ++handles; }
  ~Weak()
  {
    if (allocated)
      --handles;
  }
  Weak(const Weak&)            = delete;
  Weak& operator=(const Weak&) = delete;
  Weak(Weak&& other) noexcept
      : weak(std::move(other.weak))
      , allocated(std::exchange(other.allocated, false))
  {
  }
  Weak& operator=(Weak&& other) noexcept
  {
    if (allocated)
      --handles;
    weak      = std::move(other.weak);
    allocated = std::exchange(other.allocated, false);
    return *this;
  }
  explicit operator bool() const
  { return allocated; }
};
using haven_visuals::AccessState;
struct Access {
  std::shared_ptr<Object> root;
  AccessState             acquired = AccessState::Gone;
  Access()                         = default;
  Access(std::shared_ptr<Object> object, AccessState state)
      : root(std::move(object))
      , acquired(state)
  {
  }
  Access(const Access&)            = delete;
  Access& operator=(const Access&) = delete;
  Access(Access&&)                 = default;
  Access&     operator=(Access&&)  = default;
  AccessState State() const
  {
    if (acquired != AccessState::Ready)
      return acquired;
    if (!root->alive_readable)
      return AccessState::Unavailable;
    return root->alive ? AccessState::Ready : AccessState::Gone;
  }
  const void* Token() const
  { return root.get(); }
  std::optional<std::int32_t> Id()
  { return State() == AccessState::Ready && root->id_readable ? std::optional{root->id} : std::nullopt; }
  std::optional<bool> ReadEnabled()
  { return State() == AccessState::Ready && root->enabled_readable ? std::optional{root->enabled} : std::nullopt; }
  std::optional<float> ReadDepth(std::size_t property)
  {
    return State() == AccessState::Ready && root->depth_readable[property] ? std::optional{root->depth[property]}
                                                                           : std::nullopt;
  }
  void AfterWrite(std::size_t index)
  {
    if (root->during_write)
      root->during_write(index);
    if (root->lose_readback[index]) {
      if (!index)
        root->enabled_readable = false;
      else
        root->depth_readable[index - 1] = false;
    }
    if (root->throw_after[index])
      throw NativeFailure{};
  }
  bool WriteEnabled(bool value)
  {
    if (State() != AccessState::Ready)
      return false;
    ++root->writes[0];
    if (root->mutate[0])
      root->enabled = value;
    AfterWrite(0);
    return root->success[0];
  }
  bool WriteDepth(std::size_t property, float value)
  {
    if (State() != AccessState::Ready)
      return false;
    const auto index = property + 1;
    ++root->writes[index];
    if (root->mutate[index])
      root->depth[property] = value;
    AfterWrite(index);
    return root->success[index];
  }
};
// These observations model the adapter. The actual cache, ownership registry,
// identity matching, reentry guard and restoration algorithms are production.
struct Backend {
  using Weak   = ::Weak;
  using Object = ::Object*;
  std::shared_ptr<::Object>            owner;
  std::vector<std::weak_ptr<::Object>> roads, water;
  int                                  finds = 0, scans = 0;
  bool                                 scan_ok = true, scan_throws = false;
  bool                                 get_owner_throws = false, find_throws = false, retain_throws = false;
  Object                               scanned_owner = nullptr;
  Access                               Acquire(const Weak& weak)
  {
    auto object = weak.weak.lock();
    if (!object)
      return {};
    if (object->root_failure)
      return {{}, AccessState::Unavailable};
    return {std::move(object), AccessState::Ready};
  }
  Weak Retain(Object object)
  {
    if (object && retain_throws)
      throw NativeFailure{};
    return object && !object->weak_failure ? Weak(object->shared_from_this()) : Weak{};
  }
  Weak Retain(const Access& access)
  { return Retain(access.root.get()); }
  Object GetOwner(const Weak& weak)
  {
    if (weak && get_owner_throws)
      throw NativeFailure{};
    auto access = Acquire(weak);
    return access.State() == AccessState::Ready && access.root->active ? access.root.get() : nullptr;
  }
  Object FindOwner()
  {
    ++finds;
    if (find_throws)
      throw NativeFailure{};
    return owner && owner->alive && owner->active ? owner.get() : nullptr;
  }
  bool ScanOwner(Object owner_object, std::vector<Weak>& out_roads, std::vector<Weak>& out_water)
  {
    ++scans;
    scanned_owner = owner_object;
    for (const auto& weak : roads)
      if (auto object = weak.lock())
        out_roads.push_back(Retain(object.get()));
    for (const auto& weak : water)
      if (auto object = weak.lock())
        out_water.push_back(Retain(object.get()));
    if (scan_throws)
      throw NativeFailure{};
    return scan_ok;
  }
};
std::shared_ptr<Object> Make(std::int32_t id)
{
  auto object = std::make_shared<Object>();
  object->id  = id;
  return object;
}
std::vector<Weak> Members(Backend& backend, const std::shared_ptr<Object>& object)
{
  std::vector<Weak> result;
  result.push_back(backend.Retain(object.get()));
  return result;
}

void Discovery()
{
  using Cache = haven_visuals::SnapshotCache<Backend>;
  using namespace std::chrono_literals;
  const auto now = Cache::Clock::time_point{} + 100s;
  Backend    backend;
  Cache      cache;
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(backend, now);
  Check(backend.finds == 1 && backend.scans == 0, "no-owner lookup ignored backoff");
  auto first = Make(1), road = Make(-10), water = Make(0);
  backend.owner = first;
  backend.roads = {road};
  backend.water = {water};
  cache.Refresh(backend, now + 5s);
  Check(backend.finds == 2 && backend.scans == 1 && cache.Roads().size() == 1 && cache.Water().size() == 1,
        "road and water did not share owner discovery");
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(backend, now + 6s);
  Check(backend.finds == 2 && backend.scans == 1, "shared callers rescanned before five seconds");
  auto streamed = Make(11);
  backend.roads = {streamed};
  cache.Refresh(backend, now + 10s);
  Check(cache.Roads()[0].weak.lock() == streamed, "same-owner streamed descendants were not refreshed");
  auto replacement = Make(2);
  backend.owner    = replacement;
  cache.Refresh(backend, now + 15s);
  Check(first->alive && first->active && cache.Water()[0].weak.lock() == water && backend.scans == 3
            && backend.scanned_owner == replacement.get(),
        "alive owner replacement was not rescanned");
  replacement->active = false;
  cache.Refresh(backend, now + 16s);
  Check(cache.Roads().empty() && cache.Water().empty(), "inactive owner retained authoritative membership");
  replacement->active       = true;
  replacement->weak_failure = true;
  cache.Refresh(backend, now + 20s);
  const auto finds = backend.finds, scans = backend.scans;
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(backend, now + 21s);
  Check(backend.finds == finds && backend.scans == scans && cache.Water().empty(),
        "failed weak owner custody published membership or retried per call");
  replacement->weak_failure = false;
  backend.roads.assign(1000, road);
  backend.water.assign(100, water);
  cache.Refresh(backend, now + 25s);
  Check(cache.Roads().empty() && cache.Water().empty() && Weak::handles == 1,
        "oversized snapshot published a supposedly complete prefix");
  backend.roads.assign(256, road);
  backend.water.assign(8, water);
  cache.Refresh(backend, now + 30s);
  Check(cache.Roads().size() == 256 && cache.Water().size() == 8 && Weak::handles == 265,
        "complete admitted snapshot lost its bounded weak custody");
  backend.scan_ok = false;
  cache.Refresh(backend, now + 35s);
  cache.Refresh(backend, now + 36s);
  Check(cache.Roads().empty() && cache.Water().empty() && Weak::handles == 1, "failed scan retained stale membership");
  backend.scan_ok = true;
  cache.Refresh(backend, now + 40s);
  backend.scan_throws = true;
  try {
    cache.Refresh(backend, now + 45s);
    Check(false, "throwing scan fixture was not reached");
  } catch (const NativeFailure&) {
  }
  const auto scans_after_throw = backend.scans;
  cache.Refresh(backend, now + 46s);
  Check(cache.Roads().empty() && cache.Water().empty() && backend.scans == scans_after_throw && Weak::handles == 0,
        "throwing scan retained stale membership or bypassed backoff");
  backend.scan_throws = false;
  cache.Refresh(backend, now + 50s);
  Check(cache.Roads().size() == 256 && cache.Water().size() == 8,
        "throwing scan could not recover at its next attempt");
  cache.Clear();
  Check(Weak::handles == 0, "cache retirement leaked weak custody");
}

void DiscoveryExceptions()
{
  using Cache = haven_visuals::SnapshotCache<Backend>;
  using namespace std::chrono_literals;
  const auto now = Cache::Clock::time_point{} + 100s;
  for (int failure = 0; failure < 3; ++failure) {
    Backend                              backend;
    Cache                                cache;
    haven_visuals::OwnedVisuals<Backend> owned;
    auto                                 owner = Make(1), road = Make(-10), water = Make(0);
    backend.owner = owner;
    backend.roads = {road};
    backend.water = {water};
    cache.Refresh(backend, now);
    owned.Apply(backend, cache.Roads(), cache.Water(), true, true);
    Check(owned.Pending() && !water->enabled && road->depth[0] == -1.0f,
          "exception fixture did not establish old interventions");
    road->depth_readable    = {false, false};
    water->enabled_readable = false;
    auto when               = now + 5s;
    if (failure == 0) {
      backend.get_owner_throws = true;
      when                     = now + 1s;
    }
    if (failure == 1)
      backend.find_throws = true;
    if (failure == 2) {
      backend.owner         = Make(2);
      backend.retain_throws = true;
    }
    try {
      cache.Refresh(backend, when);
      Check(false, "discovery exception fixture did not reach its fault");
    } catch (const NativeFailure&) {
    }
    Check(cache.Roads().empty() && cache.Water().empty() && Weak::handles == 2 && owned.Pending(),
          "discovery exception retained old membership or erased separate restoration ownership");
    backend.get_owner_throws = backend.find_throws = backend.retain_throws = false;
    const auto finds = backend.finds, scans = backend.scans;
    const auto road_writes = road->writes, water_writes = water->writes;
    cache.Refresh(backend, when + 1s);
    owned.Apply(backend, cache.Roads(), cache.Water(), true, true);
    Check(cache.Roads().empty() && cache.Water().empty() && backend.finds == finds && backend.scans == scans
              && road->writes == road_writes && water->writes == water_writes && owned.Pending(),
          "within-backoff pass reused a stale cohort or discarded unknown restoration");
    road->depth_readable    = {true, true};
    water->enabled_readable = true;
    owned.Apply(backend, {}, {}, false, false);
    Check(!owned.Pending() && water->enabled && road->depth == std::array{0.0f, 0.0f} && Weak::handles == 0,
          "discovery failure prevented independently retained restoration from draining");
    cache.Refresh(backend, when + 5s);
    Check(cache.Roads().size() == 1 && cache.Water().size() == 1, "discovery exception did not recover after backoff");
  }
}

void OperationGuard()
{
  using Cache = haven_visuals::SnapshotCache<Backend>;
  using namespace std::chrono_literals;
  Backend                              backend;
  Cache                                cache;
  haven_visuals::OwnedVisuals<Backend> owned;
  auto                                 owner = Make(1), road = Make(-10), water = Make(0);
  backend.owner           = owner;
  backend.roads           = {road};
  backend.water           = {water};
  const auto started      = Cache::Clock::time_point{} + 100s;
  auto       clock        = started;
  bool       busy         = false;
  int        nested_calls = 0;
  water->during_write     = [&](auto) {
    clock += 6s; // Both the one-second apply and five-second discovery deadlines passed.
    Check(clock > started + 1s, "nested operation did not exercise an expired throttle");
    const auto finds = backend.finds, scans = backend.scans;
    const auto road_writes = road->writes, water_writes = water->writes;
    const bool ran = haven_visuals::RunExclusive(busy, [&] {
      ++nested_calls;
      cache.Refresh(backend, clock);
      owned.Apply(backend, cache.Roads(), cache.Water(), true, true);
    });
    Check(!ran && nested_calls == 0 && backend.finds == finds && backend.scans == scans && road->writes == road_writes
              && water->writes == water_writes && owned.Pending(),
          "delayed nested episode reached discovery or disturbed the outer intervention");
  };
  Check(haven_visuals::RunExclusive(busy,
                                    [&] {
                                      cache.Refresh(backend, clock);
                                      owned.Apply(backend, cache.Roads(), cache.Water(), true, true);
                                    }),
        "outer operation did not execute");
  Check(!busy && backend.finds == 1 && backend.scans == 1 && !water->enabled && owned.Pending(),
        "outer operation lost custody or intervention progress after reentry");
  water->during_write = {};
  haven_visuals::RunExclusive(busy, [&] { owned.Apply(backend, {}, {}, false, false); });
  Check(!busy && !owned.Pending() && water->enabled && road->depth == std::array{0.0f, 0.0f},
        "rejected nested episode prevented outer restoration from draining");
  water->throw_after[0] = true;
  try {
    haven_visuals::RunExclusive(busy, [&] {
      cache.Refresh(backend, clock);
      owned.Apply(backend, cache.Roads(), cache.Water(), true, true);
    });
    Check(false, "throwing guarded operation fixture was not reached");
  } catch (const NativeFailure&) {
  }
  Check(!busy && owned.Pending(), "throwing operation stranded busy state or discarded restoration");
  water->throw_after[0] = false;
  Check(haven_visuals::RunExclusive(busy, [&] { owned.Apply(backend, {}, {}, false, false); }),
        "operation guard did not recover after a throw");
  Check(!busy && !owned.Pending() && water->enabled && road->depth == std::array{0.0f, 0.0f},
        "post-failure operation could not confirm retained restorations");
}

void Water()
{
  for (bool original : {false, true}) {
    Backend                              backend;
    haven_visuals::OwnedVisuals<Backend> owned;
    auto                                 object = Make(original ? -7 : 0);
    object->enabled                             = original;
    auto members                                = Members(backend, object);
    owned.Apply(backend, {}, members, false, true);
    Check(!object->enabled && owned.Pending(), "water override lost a checked false or signed identity");
    owned.Apply(backend, {}, {}, false, false);
    Check(object->enabled == original && !owned.Pending(), "water release did not restore its original enable");
    owned.Apply(backend, {}, members, false, true);
    object->enabled   = true;
    const auto writes = object->writes[0];
    owned.Apply(backend, {}, {}, false, false);
    Check(object->enabled && object->writes[0] == writes && !owned.Pending(),
          "release disabled another owner's enable");
  }
  Backend                              backend;
  haven_visuals::OwnedVisuals<Backend> owned;
  auto                                 object  = Make(12);
  auto                                 members = Members(backend, object);
  object->enabled_readable                     = false;
  owned.Apply(backend, {}, members, false, true);
  Check(!owned.Pending() && object->writes[0] == 0, "unavailable enabled state became a false baseline");
  object->enabled_readable = true;
  object->success[0]       = false;
  object->lose_readback[0] = true;
  object->during_write     = [&](auto) { owned.Apply(backend, {}, {}, false, false); };
  owned.Apply(backend, {}, members, false, true);
  Check(!object->enabled && owned.Pending(), "mutating failed override or nested Apply lost restoration");
  object->during_write     = {};
  object->lose_readback[0] = false;
  object->enabled_readable = true;
  object->mutate[0]        = false;
  object->success[0]       = true;
  owned.Apply(backend, {}, {}, false, false);
  Check(!object->enabled && owned.Pending(), "setter success alone acknowledged water restoration");
  object->mutate[0]        = true;
  object->success[0]       = false;
  object->lose_readback[0] = true;
  owned.Apply(backend, {}, {}, false, false);
  Check(object->enabled && owned.Pending(), "missing readback acknowledged water restoration");
  object->enabled_readable = true;
  object->lose_readback[0] = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(!owned.Pending(), "matching later water readback did not retire restoration");

  haven_visuals::WaterRestoration state;
  auto                            access = backend.Acquire(members[0]);
  object->during_write   = [&](auto) { Check(state.Pending(), "water mutation preceded baseline retention"); };
  object->throw_after[0] = true;
  try {
    state.Apply(access, true);
    Check(false, "throw fixture was not reached");
  } catch (const NativeFailure&) {
  }
  Check(state.Pending() && !object->enabled, "throw after water mutation discarded the obligation");
  object->during_write   = {};
  object->throw_after[0] = false;
  state.Apply(access, false);
  Check(object->enabled && !state.Pending(), "throwing water override could not later restore");
}

void Roads()
{
  Backend                              backend;
  haven_visuals::OwnedVisuals<Backend> owned;
  auto                                 object = Make(-20);
  object->depth                               = {0.0f, -2.0f};
  auto members                                = Members(backend, object);
  owned.Apply(backend, members, {}, true, false);
  Check(object->depth == std::array{-1.0f, -2.0f} && owned.Pending(), "road bias weakened native depth or lost zero");
  owned.Apply(backend, members, {}, true, false);
  Check(object->writes[1] == 1 && object->writes[2] == 0, "road bias compounded or wrote an already stronger value");
  owned.Apply(backend, {}, {}, false, false);
  Check(object->depth == std::array{0.0f, -2.0f} && !owned.Pending(), "road dropout did not restore actual offsets");
  owned.Apply(backend, members, {}, true, false);
  object->depth[0]  = -0.25f;
  const auto writes = object->writes[1];
  owned.Apply(backend, {}, {}, false, false);
  Check(object->depth[0] == -0.25f && object->writes[1] == writes && !owned.Pending(),
        "road restoration overwrote a competing known edit");

  object->depth = {0.0f, 0.5f};
  owned.Apply(backend, members, {}, true, false);
  object->mutate[1] = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(object->depth == std::array{-1.0f, 0.5f} && owned.Pending(),
        "partial road restore cleared another property obligation");
  const auto restored_writes = object->writes[2];
  object->mutate[1]          = true;
  object->lose_readback[1]   = true;
  owned.Apply(backend, {}, {}, false, false);
  Check(object->depth[0] == 0.0f && owned.Pending(), "road setter status replaced matching readback");
  object->depth_readable[0] = true;
  object->lose_readback[1]  = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(!owned.Pending() && object->writes[2] == restored_writes, "partial restore repeated an acknowledged property");

  object->depth = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
  owned.Apply(backend, members, {}, true, false);
  Check(!owned.Pending(), "nonfinite road observations became baselines");
  object->depth                    = {0.0f, 0.0f};
  object->depth_readable[0]        = false;
  const auto known_property_writes = object->writes[2];
  owned.Apply(backend, members, {}, true, false);
  Check(!owned.Pending() && object->writes[2] == known_property_writes,
        "an incomplete road pair authorized a new partial override");
  object->depth_readable[0] = true;
  object->depth             = {0.0f, -3.0f};
  object->throw_after[1]    = true;
  try {
    owned.Apply(backend, members, {}, true, false);
    Check(false, "throwing road fixture was not reached");
  } catch (const NativeFailure&) {
  }
  Check(owned.Pending() && object->depth[0] == -1.0f, "throw after road mutation discarded its baseline");
  object->throw_after[1] = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(object->depth[0] == 0.0f && !owned.Pending(), "throwing owned pass kept reentry latched or lost restoration");
}

void IdentitiesAndCustody()
{
  Backend                              backend;
  haven_visuals::OwnedVisuals<Backend> owned;
  auto                                 old = Make(0), replacement = Make(0);
  replacement->enabled = false;
  auto previous = Members(backend, old), current = Members(backend, replacement);
  owned.Apply(backend, {}, previous, false, true);
  old->root_failure = true;
  owned.Apply(backend, {}, current, false, true);
  Check(owned.Pending() && replacement->writes[0] == 0, "unavailable old identity was silently rebased");
  old->root_failure = false;
  owned.Apply(backend, {}, current, false, true);
  Check(old->enabled && !replacement->enabled, "same numeric ID transferred old baseline to replacement");
  owned.Apply(backend, {}, {}, false, false);
  Check(!replacement->enabled && !owned.Pending(), "replacement did not retain its own false baseline");

  auto source = Make(-31), rebound = Make(-31);
  auto source_members = Members(backend, source), rebound_members = Members(backend, rebound);
  source->during_write = [&](auto) { owned.Apply(backend, {}, rebound_members, false, true); };
  owned.Apply(backend, {}, source_members, false, true);
  Check(!source->enabled && rebound->enabled && rebound->writes[0] == 0,
        "nested replacement mutated owned registry during its caller's pass");
  source->during_write = {};
  owned.Apply(backend, {}, rebound_members, false, true);
  Check(source->enabled && !rebound->enabled, "later replacement did not restore the retired actual object");
  owned.Apply(backend, {}, {}, false, false);
  Check(rebound->enabled && !owned.Pending(), "nested replacement inherited the retired object's restoration");

  replacement->enabled      = true;
  replacement->weak_failure = true;
  owned.Apply(backend, {}, current, false, true);
  Check(replacement->enabled && !owned.Pending(), "failed weak custody authorized a native mutation");
  replacement->weak_failure = false;
  replacement->id_readable  = false;
  owned.Apply(backend, {}, current, false, true);
  Check(replacement->enabled && !owned.Pending(), "unknown numeric identity became usable zero");
  replacement->id_readable = true;
  owned.Apply(backend, {}, current, false, true);
  replacement->alive_readable = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(owned.Pending(), "unavailable lifetime retired an unconfirmed restoration");
  replacement->alive_readable = true;
  replacement->alive          = false;
  owned.Apply(backend, {}, {}, false, false);
  Check(!owned.Pending(), "confirmed native loss retained stale obligations");

  auto retired = Make(-99);
  auto weak    = Members(backend, retired);
  owned.Apply(backend, {}, weak, false, true);
  std::weak_ptr<Object> observed = retired;
  retired.reset();
  Check(observed.expired(), "owned restoration retained a strong native object indefinitely");
  owned.Apply(backend, {}, {}, false, false);
  Check(!owned.Pending(), "collected weak object reached later native restoration");
}

void Limits()
{
  Backend                              backend;
  haven_visuals::OwnedVisuals<Backend> owned;
  std::vector<std::shared_ptr<Object>> objects;
  std::vector<Weak>                    roads, water;
  auto                                 previous  = Make(900);
  auto                                 old_water = Members(backend, previous);
  owned.Apply(backend, {}, old_water, false, true);
  previous->enabled_readable = false;
  for (std::size_t i = 0; i < haven_visuals::kRoadLimit + 1; ++i) {
    auto object = Make(static_cast<std::int32_t>(i));
    roads.push_back(backend.Retain(object.get()));
    objects.push_back(std::move(object));
  }
  for (std::size_t i = 0; i < haven_visuals::kWaterLimit + 1; ++i) {
    auto object = Make(-static_cast<std::int32_t>(i));
    water.push_back(backend.Retain(object.get()));
    objects.push_back(std::move(object));
  }
  owned.Apply(backend, roads, water, true, true);
  for (const auto& object : objects)
    Check(object->writes == std::array{0, 0, 0}, "oversized membership authorized a prefix of native mutations");
  Check(owned.Pending() && !previous->enabled, "oversized membership discarded an unconfirmed old obligation");
  previous->enabled_readable = true;
  owned.Apply(backend, {}, {}, false, false);
  Check(previous->enabled && !owned.Pending(), "unavailable membership prevented later old restoration");
  roads.pop_back();
  water.pop_back();
  owned.Apply(backend, roads, water, true, true);
  Check(owned.Pending() && objects[0]->writes[1] == 1 && objects[haven_visuals::kRoadLimit + 1]->writes[0] == 1,
        "complete admitted membership did not apply");
  owned.Apply(backend, {}, {}, false, false);
  Check(!owned.Pending(), "bounded admitted restoration failed to drain");
}
} // namespace

int main()
{
  try {
    Discovery();
    DiscoveryExceptions();
    OperationGuard();
    Water();
    Roads();
    IdentitiesAndCustody();
    Limits();
    Check(Weak::handles == 0, "fixture or owned algorithms leaked weak handles");
    std::cout << "Haven shared discovery, weak ownership and restoration fixtures passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
