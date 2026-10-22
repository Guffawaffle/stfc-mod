#define STFC_HAVEN_VISUALS_PORTABLE_TEST
#include "../mods/src/patches/haven_visuals_cache.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <unordered_set>

namespace
{
struct Backend {
  using Object = int;
  struct Weak {
    static inline int handles = 0;
    int               object  = 0;
    Weak()                    = default;
    explicit Weak(int value)
        : object(value)
    {
      if (object)
        ++handles;
    }
    Weak(const Weak &)            = delete;
    Weak &operator=(const Weak &) = delete;
    Weak(Weak &&other) noexcept
        : object(std::exchange(other.object, 0))
    {
    }
    Weak &operator=(Weak &&other) noexcept
    {
      if (object)
        --handles;
      object = std::exchange(other.object, 0);
      return *this;
    }
    ~Weak()
    {
      if (object)
        --handles;
    }
    explicit operator bool() const
    { return object != 0; }
  };
  static inline int                     owner = 0, finds = 0, scans = 0;
  static inline bool                    scan_ok = true;
  static inline std::unordered_set<int> live;
  static inline std::vector<int>        roads, water;
  static int                            Get(int weak)
  { return live.contains(weak) ? weak : 0; }
  static inline bool active      = true;
  static inline int  fail_retain = 0;
  static int         GetOwner(const Weak &weak)
  { return active ? Get(weak.object) : 0; }
  static Weak Retain(int object)
  { return Weak(object == fail_retain ? 0 : object); }
  static int FindOwner()
  {
    ++finds;
    return active ? Get(owner) : 0;
  }
  static bool ScanOwner(int, std::vector<Weak> &out_roads, std::vector<Weak> &out_water)
  {
    ++scans;
    for (int object : roads)
      if (auto weak = Retain(object); weak)
        out_roads.push_back(std::move(weak));
    for (int object : water)
      if (auto weak = Retain(object); weak)
        out_water.push_back(std::move(weak));
    return scan_ok;
  }
};
} // namespace

int main()
{
  using Cache = HavenVisuals::SnapshotCache<Backend>;
  using namespace std::chrono_literals;
  auto  now = Cache::Clock::time_point{} + 100s;
  Cache cache;
  // Missing owners never turn each camera update into a scene search.
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(now);
  assert(Backend::finds == 1 && Backend::scans == 0);
  Backend::owner = 1;
  Backend::live  = {1, 10, 20};
  Backend::roads = {10};
  Backend::water = {20};
  cache.Refresh(now + 5s);
  assert(Backend::finds == 2 && Backend::scans == 1);
  assert(cache.Roads().size() == 1 && cache.Roads()[0].object == 10);
  assert(cache.Water().size() == 1 && cache.Water()[0].object == 20);
  // Independent road/water callers reuse the same owner and classification.
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(now + 6s);
  assert(Backend::finds == 2 && Backend::scans == 1);
  // Streamed/replaced descendants refresh without destroying their owner.
  Backend::roads = {11};
  cache.Refresh(now + 10s);
  assert(Backend::finds == 2 && Backend::scans == 2);
  assert(cache.Roads().size() == 1 && cache.Roads()[0].object == 11);
  // A failed scan drops stale membership and backs off rather than spinning.
  Backend::scan_ok = false;
  cache.Refresh(now + 15s);
  cache.Refresh(now + 16s);
  assert(Backend::scans == 3 && cache.Roads().empty() && cache.Water().empty());
  // Destruction drops the old cohort. A new owner forces immediate discovery.
  Backend::live.erase(1);
  Backend::owner = 2;
  Backend::live.insert(2);
  Backend::scan_ok = true;
  Backend::roads   = {12};
  cache.Refresh(now + 17s);
  assert(Backend::finds == 3 && Backend::scans == 4);
  assert(cache.Roads().size() == 1 && cache.Roads()[0].object == 12);
  // An unexpectedly large scene cannot grow retained handles without a bound.
  Backend::roads.assign(1000, 30);
  Backend::water.assign(100, 40);
  cache.Refresh(now + 22s);
  assert(cache.Roads().size() == 256 && cache.Water().size() == 8);
  for (int i = 1; i < 100; ++i) {
    cache.Refresh(now + 22s + i * 5s);
    assert(Backend::Weak::handles == 265);
  }
  now += 500s;
  Backend::live.erase(2);
  Backend::owner = 0;
  cache.Refresh(now + 23s);
  assert(cache.Roads().empty() && cache.Water().empty());
  assert(Backend::Weak::handles == 0);
  // Failed weak-handle allocation neither publishes an unowned snapshot nor
  // retries global lookup per frame; it recovers at the next bounded attempt.
  Backend::owner = Backend::fail_retain = 3;
  Backend::live.insert(3);
  const auto scans_before = Backend::scans;
  cache.Refresh(now + 28s);
  const auto finds_after_failure = Backend::finds;
  for (int i = 0; i < 10000; ++i)
    cache.Refresh(now + 29s);
  assert(Backend::finds == finds_after_failure && Backend::scans == scans_before);
  assert(cache.Roads().empty() && cache.Water().empty());
  Backend::fail_retain = 0;
  cache.Refresh(now + 33s);
  assert(Backend::scans == scans_before + 1 && Backend::Weak::handles == 265);
  // An inactive but still-live old owner must not keep its cohort authoritative.
  Backend::active = false;
  cache.Refresh(now + 34s);
  assert(cache.Roads().empty() && cache.Water().empty() && Backend::Weak::handles == 0);
}
