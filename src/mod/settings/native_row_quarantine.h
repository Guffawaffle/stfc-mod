#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace mod_settings::native
{
// Failed native insertion/removal is not proof of no mutation. Retain only weak identities until authoritative
// membership proves removal, ownership is positively revoked, or the source is collected. This is Settings-local
// cleanup custody, not a second registry of settings or a renderer.
template <class Handle> class RowQuarantine
{
public:
  using Object = decltype(std::declval<const Handle&>().Get());
  bool Add(Object parent, Object row, Object getter, Object setter)
  {
    for (const auto& [id, record] : records_)
      if (record->parent.Get() == parent && record->row.Get() == row && record->getter.Get() == getter
          && record->setter.Get() == setter)
        return true;
    auto record = std::make_shared<Record>(
        Record{Handle::Weak(parent), Handle::Weak(row), Handle::Weak(getter), Handle::Weak(setter)});
    if (!record->parent || !record->row || !record->getter || !record->setter)
      return false;
    records_.emplace(++serial_, std::move(record));
    return true;
  }
  bool Contains(Object row) const
  {
    for (const auto& [id, record] : records_)
      if (record->row.Get() == row)
        return true;
    return false;
  }
  size_t Size() const
  { return records_.size(); }
  void Clear()
  { records_.clear(); }

  // Host Contains/Owned return optional<bool>: unknown must remain pending. Remove may fail before OR after
  // mutation. Never erase from a successful call alone, and never delete a row whose callbacks changed owners.
  template <class Host> void Retry(Host& host)
  {
    std::vector<uint64_t> keys;
    for (const auto& [id, record] : records_)
      keys.push_back(id);
    for (const auto id : keys) {
      auto found = records_.find(id);
      if (found == records_.end())
        continue;
      auto       record = found->second; // Native calls can reenter Add/Clear; no iterator crosses them.
      const auto parent = record->parent.Get(), row = record->row.Get();
      const auto getter = record->getter.Get(), setter = record->setter.Get();
      if (!parent || !row || !getter || !setter) {
        records_.erase(id);
        continue;
      }
      auto parent_root = Handle::Strong(parent), row_root = Handle::Strong(row);
      auto get_root = Handle::Strong(getter), set_root = Handle::Strong(setter);
      if (!parent_root || !row_root || !get_root || !set_root)
        continue;
      const auto member = host.Contains(parent, row);
      if (!member)
        continue;
      if (!*member) {
        records_.erase(id);
        continue;
      }
      const auto owned = host.Owned(row, getter, setter);
      if (!owned)
        continue;
      if (!*owned) {
        records_.erase(id);
        continue;
      }
      // A reentrant Clear or replacement must not authorize the old operation to mutate a new context.
      found = records_.find(id);
      if (found == records_.end() || found->second != record)
        continue;
      host.Remove(parent, row);
      const auto remaining = host.Contains(parent, row);
      if (remaining && !*remaining)
        records_.erase(id);
    }
  }

private:
  struct Record {
    Handle parent, row, getter, setter;
  };
  std::map<uint64_t, std::shared_ptr<Record>> records_;
  uint64_t                                    serial_ = 0;
};
} // namespace mod_settings::native
