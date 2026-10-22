#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace galaxy_controls
{
// Stable round-robin maintenance survives hash-table rehash, live insertions and
// explicit releases. Handle ownership remains with the native adapter.
template <class Key, class Handle> class LabelTracking
{
  using Rows = std::list<std::pair<Key, Handle>>;

public:
  using iterator = typename Rows::iterator;
  iterator begin()
  { return rows_.begin(); }
  iterator end()
  { return rows_.end(); }
  iterator find(Key key)
  {
    const auto found = index_.find(key);
    return found == index_.end() ? end() : found->second;
  }
  bool emplace(Key key, Handle handle)
  {
    if (index_.contains(key))
      return false;
    rows_.emplace_front(key, handle);
    try {
      index_.emplace(key, rows_.begin());
    } catch (...) {
      rows_.pop_front();
      throw;
    }
    return true;
  }
  void erase(iterator it)
  {
    if (cursor_ == it)
      ++cursor_;
    index_.erase(it->first);
    rows_.erase(it);
  }
  template <class Alive, class Release> std::size_t Prune(std::size_t limit, Alive alive, Release release)
  {
    const auto count = std::min(limit, rows_.size());
    for (std::size_t visited = 0; visited < count; ++visited) {
      if (cursor_ == end())
        cursor_ = begin();
      const auto current = cursor_++;
      if (!alive(current->second)) {
        release(current->second);
        erase(current);
      }
    }
    return count;
  }
  std::size_t size() const
  { return rows_.size(); }
  LabelTracking()                                = default;
  LabelTracking(const LabelTracking&)            = delete;
  LabelTracking& operator=(const LabelTracking&) = delete;

private:
  Rows                              rows_;
  std::unordered_map<Key, iterator> index_;
  iterator                          cursor_ = rows_.end();
};
} // namespace galaxy_controls
