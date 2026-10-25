#pragma once

// Value-only mining observations shared by the HUD and notification consumers.
// A managed address is only a cache identity token, never retained for dereference.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

struct FleetPlayerData;

namespace fleet_opc
{
struct Cargo {
  bool   known           = false;
  bool   opc             = false;
  double current         = 0;
  double protected_limit = 0;
};

class SampleCache
{
public:
  void Invalidate(int32_t slot)
  {
    if (slot >= 0 && static_cast<size_t>(slot) < entries_.size())
      entries_[slot].valid = false;
  }
  template <class Reader>
  Cargo Read(int32_t slot, int64_t fleet, uintptr_t object, int32_t state, int64_t now, Reader&& reader)
  {
    if (slot < 0 || static_cast<size_t>(slot) >= entries_.size())
      return {};
    auto& entry = entries_[slot];
    if (entry.valid && entry.fleet == fleet && entry.object == object && entry.state == state && now >= entry.sampled
        && now - entry.sampled < 1000)
      return entry.cargo;
    const auto observed = reader();
    entry               = {true, fleet, object, state, now, observed};
    return observed;
  }

private:
  struct Entry {
    bool      valid   = false;
    int64_t   fleet   = 0;
    uintptr_t object  = 0;
    int32_t   state   = 0;
    int64_t   sampled = 0;
    Cargo     cargo;
  };
  std::array<Entry, 10> entries_{};
};

struct Status {
  bool    mining = false;
  Cargo   cargo;
  bool    rate_known      = false;
  bool    node_known      = false;
  bool    safe_on_node    = false;
  double  rate_per_second = 0;
  double  node_remaining  = 0;
  int64_t eta_seconds     = -1;
};

inline Status Evaluate(bool mining, Cargo cargo, bool rate_observed, double rate, bool node_observed,
                       double amount_mined, double node_amount)
{
  Status result;
  result.mining = mining;
  result.cargo  = cargo;
  if (!mining)
    return result;
  result.rate_known      = rate_observed && std::isfinite(rate) && rate > 0;
  result.rate_per_second = rate;
  result.node_known = node_observed && std::isfinite(amount_mined) && std::isfinite(node_amount) && amount_mined >= 0
                      && node_amount >= amount_mined;
  if (result.node_known)
    result.node_remaining = node_amount - amount_mined;
  if (!cargo.known)
    return result;
  if (cargo.opc) {
    result.eta_seconds = 0;
    return result;
  }
  if (!result.rate_known)
    return result;
  const auto seconds = std::ceil(std::max(0.0, cargo.protected_limit - cargo.current) / rate);
  // 2^63 cannot be converted to int64_t; avoid a rounded double max boundary.
  if (std::isfinite(seconds) && seconds >= 0 && seconds < 0x1p63)
    result.eta_seconds = static_cast<int64_t>(seconds);
  const auto node_seconds = result.node_known ? std::ceil(result.node_remaining / rate) : -1;
  if (std::isfinite(node_seconds) && node_seconds >= 0 && node_seconds < 0x1p63)
    result.safe_on_node = result.node_remaining <= cargo.protected_limit - cargo.current;
  return result;
}

inline std::string Duration(int64_t seconds)
{
  if (seconds < 0)
    return {};
  if (seconds < 60)
    return "<1m";
  const auto minutes = seconds / 60 + (seconds % 60 != 0);
  if (minutes < 60)
    return std::to_string(minutes) + "m";
  const auto hours = minutes / 60;
  if (hours < 24)
    return std::to_string(hours) + "h" + (minutes % 60 ? " " + std::to_string(minutes % 60) + "m" : "");
  return std::to_string(hours / 24) + "d" + (hours % 24 ? " " + std::to_string(hours % 24) + "h" : "");
}

inline std::string Display(const Status& status)
{
  if (!status.mining || !status.cargo.known)
    return {};
  if (status.cargo.opc)
    return "OPC";
  if (!status.rate_known || status.eta_seconds < 0)
    return {};
  return status.safe_on_node ? "SAFE" : Duration(status.eta_seconds);
}

inline std::string CardDisplay(const Status& status)
{
  if (!status.mining || !status.cargo.known)
    return {};
  return "OPC: "
         + (status.cargo.opc          ? std::string("Now")
            : status.eta_seconds >= 0 ? Duration(status.eta_seconds)
                                      : std::string("Unknown"));
}

Cargo  Read(FleetPlayerData* fleet, int32_t slot, int64_t fleet_id, int32_t state);
void   Invalidate(int32_t slot);
Status Observe(FleetPlayerData* fleet, int32_t slot, int64_t fleet_id, int32_t state);
} // namespace fleet_opc
