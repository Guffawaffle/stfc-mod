#pragma once
#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>

// The same native event can pass through both toast enqueue hooks. Keep only scalar data.
class IncomingPlayerAttackTracker
{
public:
  using Clock = std::chrono::steady_clock;
  bool Accept(const std::string& uuid, long long fleet, const std::string& attacker, Clock::time_point now)
  {
    std::erase_if(seen_, [now](const auto& entry) { return entry.second <= now; });
    const auto key = uuid.empty() ? "fallback:" + std::to_string(fleet) + ":" + attacker
                                  : "event:" + uuid + ":" + std::to_string(fleet);
    if (seen_.contains(key))
      return false;
    if (seen_.size() >= 128) {
      const auto oldest = std::min_element(seen_.begin(), seen_.end(),
                                           [](const auto& a, const auto& b) { return a.second < b.second; });
      seen_.erase(oldest);
    }
    seen_.emplace(key, now + std::chrono::seconds(uuid.empty() ? 2 : 60));
    return true;
  }

private:
  std::unordered_map<std::string, Clock::time_point> seen_;
};
