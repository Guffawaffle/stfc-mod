#pragma once

// Preserved downstream 45d1deaf resolver primitive. Clear is the sole added
// operation, used by the foundation's independently owned session generation.

#include <chrono>
#include <cstddef>
#include <list>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace sync_resolver
{
inline constexpr std::size_t kEntryLimit = 4096;
inline constexpr std::size_t kFieldBytes = 256;

inline bool BoundedField(std::string_view value)
{ return value.size() <= kFieldBytes; }

// Call only from owned-payload/download workers, with the caller's cache mutex
// held. Value supplies expires_at. Prune once per batch/lookup operation, not
// once per insertion: work is O(capacity + batch size), with O(1) LRU eviction.
template <class Key, class Value> class Cache
{
public:
  using Time = std::chrono::steady_clock::time_point;
  explicit Cache(std::size_t capacity = kEntryLimit)
      : capacity_(capacity)
  {
  }
  Cache(const Cache&)            = delete;
  Cache& operator=(const Cache&) = delete;

  void Prune(Time now)
  {
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (it->second.value.expires_at <= now) {
        age_.erase(it->second.age);
        it = entries_.erase(it);
      } else {
        ++it;
      }
    }
  }

  Value* Find(const Key& key, Time now)
  {
    const auto it = entries_.find(key);
    if (it == entries_.end())
      return nullptr;
    if (it->second.value.expires_at <= now) {
      age_.erase(it->second.age);
      entries_.erase(it);
      return nullptr;
    }
    age_.splice(age_.begin(), age_, it->second.age);
    return &it->second.value;
  }

  void Put(Key key, Value value, Time now)
  {
    if (capacity_ == 0 || value.expires_at <= now)
      return;
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
      found->second.value = std::move(value);
      age_.splice(age_.begin(), age_, found->second.age);
      return;
    }
    if (entries_.size() >= capacity_) {
      entries_.erase(age_.back());
      age_.pop_back();
    }
    age_.push_front(key);
    try {
      entries_.emplace(std::move(key), Entry{std::move(value), age_.begin()});
    } catch (...) {
      age_.pop_front();
      throw;
    }
  }

  std::size_t Size() const
  { return entries_.size(); }

  void Clear()
  {
    entries_.clear();
    age_.clear();
  }

private:
  struct Entry {
    Value                             value;
    typename std::list<Key>::iterator age;
  };
  std::size_t                    capacity_;
  std::list<Key>                 age_;
  std::unordered_map<Key, Entry> entries_;
};
} // namespace sync_resolver
