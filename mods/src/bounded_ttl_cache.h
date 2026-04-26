#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

struct BoundedTtlDedupeResult {
  bool emitted = false;
  bool suppressed_by_window = false;
  bool evicted_oldest = false;
  size_t cache_size = 0;
};

template <typename Key,
          typename Clock = std::chrono::steady_clock,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class BoundedTtlDeduper {
public:
  using key_type = Key;
  using time_point = typename Clock::time_point;
  using duration = typename Clock::duration;

  explicit BoundedTtlDeduper(size_t max_entries)
    : max_entries_(max_entries)
  {
  }

  BoundedTtlDedupeResult should_emit(const Key& key, time_point now, duration ttl)
  {
    prune(now);

    BoundedTtlDedupeResult result;

    auto it = entries_.find(key);
    if (it != entries_.end() && !is_expired(it->second, now)) {
      result.suppressed_by_window = true;
      result.cache_size = entries_.size();
      return result;
    }

    if (ttl <= duration::zero()) {
      result.emitted = true;
      result.cache_size = entries_.size();
      return result;
    }

    entries_[key] = Entry{now, ttl, next_sequence_++};
    result.emitted = true;
    result.evicted_oldest = enforce_limit();
    result.cache_size = entries_.size();
    return result;
  }

  size_t prune(time_point now)
  {
    size_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (is_expired(it->second, now)) {
        it = entries_.erase(it);
        ++removed;
        continue;
      }

      ++it;
    }

    return removed;
  }

  size_t size() const
  {
    return entries_.size();
  }

  bool contains(const Key& key) const
  {
    return entries_.find(key) != entries_.end();
  }

  void clear()
  {
    entries_.clear();
  }

private:
  struct Entry {
    time_point seen_at;
    duration ttl;
    uint64_t sequence = 0;
  };

  size_t max_entries_;
  std::unordered_map<Key, Entry, Hash, KeyEqual> entries_;
  uint64_t next_sequence_ = 0;

  static bool is_expired(const Entry& entry, time_point now)
  {
    if (now <= entry.seen_at) {
      return false;
    }

    return now - entry.seen_at >= entry.ttl;
  }

  bool enforce_limit()
  {
    bool evicted = false;
    while (entries_.size() > max_entries_) {
      auto oldest = std::ranges::min_element(entries_, [](const auto& lhs, const auto& rhs) {
        if (lhs.second.seen_at == rhs.second.seen_at) {
          return lhs.second.sequence < rhs.second.sequence;
        }

        return lhs.second.seen_at < rhs.second.seen_at;
      });

      if (oldest == entries_.end()) {
        break;
      }

      entries_.erase(oldest);
      evicted = true;
    }

    return evicted;
  }
};
