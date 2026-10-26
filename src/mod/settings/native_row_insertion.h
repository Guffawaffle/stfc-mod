#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace mod_settings::native
{
// Custody begins BEFORE the mutating native call, not after successful discovery. A failed scan cannot make
// an append-then-throw disappear. All persistent identities are weak: a closed report is not kept alive by recovery.
template <class Handle> class RowInsertion
{
public:
  using Object = decltype(std::declval<const Handle&>().Get());
  uint64_t Begin(Object parent, Object getter, Object setter, Object query, std::vector<Handle> before)
  {
    auto record    = std::make_shared<Record>();
    record->parent = Handle::Weak(parent);
    record->get    = Handle::Weak(getter);
    record->set    = Handle::Weak(setter);
    record->query  = Handle::Weak(query);
    for (const auto& old : before)
      record->before.push_back(Handle::Weak(old.Get()));
    if (!record->parent || !record->get || !record->set || !record->query)
      return 0;
    const auto id = ++serial_;
    records_.emplace(id, std::move(record));
    return id;
  }
  void Complete(uint64_t id, bool succeeded)
  {
    if (auto it = records_.find(id); it != records_.end()) {
      it->second->returned  = true;
      it->second->succeeded = succeeded;
    }
  }
  size_t Size() const
  { return records_.size(); }
  bool Pending(Object parent) const
  {
    return std::ranges::any_of(records_, [&](const auto& pair) { return pair.second->parent.Get() == parent; });
  }
  // Host.Children is a fully rooted snapshot or unknown. Host.Owned is exact callback linkage or unknown.
  // Quarantine transfer succeeds before we release operation custody; foreign rows are never removed.
  template <class Host> void Retry(Host& host)
  {
    std::vector<uint64_t> keys;
    for (const auto& [id, record] : records_)
      keys.push_back(id);
    for (auto id : keys) {
      auto it = records_.find(id);
      if (it == records_.end())
        continue;
      auto record = it->second;
      if (!record->returned)
        continue; // Reentrant frame/bind during native insertion.
      auto parent = record->parent.Get();
      if (!parent) {
        records_.erase(id);
        continue;
      }
      auto root = Handle::Strong(parent);
      if (!root)
        continue;
      auto get = Handle::Strong(record->get.Get()), set = Handle::Strong(record->set.Get());
      auto query = Handle::Strong(record->query.Get());
      if (!record->get.Get() || !record->set.Get()) {
        records_.erase(id);
        continue;
      }
      if (!get || !set)
        continue;
      auto children = host.Children(parent);
      if (!children)
        continue;
      bool                unknown = false;
      std::vector<Object> ours;
      for (const auto& child : *children) {
        const auto row = child.Get();
        if (!row || std::ranges::any_of(record->before, [&](const auto& old) { return old.Get() == row; }))
          continue;
        auto owned = host.Owned(row, get.Get(), set.Get());
        if (!owned)
          unknown = true;
        else if (*owned)
          ours.push_back(row);
      }
      it = records_.find(id);
      if (it == records_.end() || it->second != record)
        continue;
      if (unknown)
        continue;
      if (record->succeeded && query && ours.size() == 1 && host.Configure(ours.front(), query.Get())) {
        records_.erase(id);
        continue;
      }
      bool transferred = true;
      for (auto row : ours)
        transferred = host.Quarantine(parent, row, get.Get(), set.Get()) && transferred;
      if (transferred)
        records_.erase(id);
    }
  }

private:
  struct Record {
    Handle              parent, get, set, query;
    std::vector<Handle> before;
    bool                returned = false, succeeded = false;
  };
  std::map<uint64_t, std::shared_ptr<Record>> records_;
  uint64_t                                    serial_ = 0;
};
} // namespace mod_settings::native
