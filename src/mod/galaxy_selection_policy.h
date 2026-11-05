#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
namespace galaxy_selection
{
inline constexpr size_t MaximumProxies         = 64;
inline constexpr size_t MaximumClickCandidates = 16;
// A failed disable may have mutated before throwing. Only the observed result discharges cleanup custody.
template <class Objects, class Host> bool Deactivate(const Objects& objects, Host& host)
{
  bool complete = true;
  for (auto object : objects) {
    if (!object)
      continue;
    const auto alive = host.Alive(object);
    if (!alive) {
      complete = false;
      continue;
    }
    if (!*alive)
      continue;
    auto active = host.Active(object);
    if (active && !*active)
      continue;
    host.Disable(object);
    active = host.Active(object);
    if (!active || *active)
      complete = false;
  }
  return complete;
}
struct TapWatch {
  const void* manager;
  float       x, y;
  bool        dispatched = false;
  bool        blocked    = false; // Unconfirmed proxy cleanup cannot authorize a fallback dispatch.
  bool        Matches(const void* other, float px, float py) const
  { return manager == other && x == px && y == py; }
};
// POIs are immutable identities. Recent clicks and known UI owners survive pool pressure; incomplete owner
// observations can never authorize retirement. This bounds owned proxies, not the game's source node count.
inline bool CanRetire(uint64_t generation, uint64_t last_click, std::chrono::steady_clock::duration age,
                      bool owners_known, bool pinned)
{
  return owners_known && !pinned && generation >= last_click && generation - last_click > 2
         && age >= std::chrono::seconds(2);
}
} // namespace galaxy_selection
