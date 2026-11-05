#include "settings/native_row_insertion.h"
#include "settings/native_row_quarantine.h"
#include <cassert>
#include <iostream>
#include <set>

struct Handle {
  int                         object = 0;
  static inline std::set<int> collected;
  static inline bool          fail_root = false;
  static Handle               Weak(int object)
  { return {object}; }
  static Handle Strong(int object)
  { return {fail_root ? 0 : object}; }
  int Get() const
  { return collected.contains(object) ? 0 : object; }
  explicit operator bool() const
  { return Get() != 0; }
};
struct Host {
  std::vector<int> children{20};
  bool             count_known = true, items_known = true, ownership_known = true, configure = true;
  int              configured = 0;
  mod_settings::native::RowQuarantine<Handle> quarantine;
  std::optional<std::vector<Handle>>          Children(int)
  {
    if (!count_known || !items_known)
      return std::nullopt;
    std::vector<Handle> result;
    for (auto child : children)
      result.push_back(Handle::Strong(child));
    return result;
  }
  std::optional<bool> Owned(int row, int get, int set)
  {
    if (!ownership_known)
      return std::nullopt;
    return row == 10 && get == 30 && set == 40;
  }
  bool Configure(int row, int query)
  {
    configured += configure && row == 10 && query == 50;
    return configure;
  }
  bool Quarantine(int parent, int row, int get, int set)
  { return quarantine.Add(parent, row, get, set); }
  std::optional<bool> Contains(int, int row)
  { return std::ranges::find(children, row) != children.end(); }
  void Remove(int, int row)
  { std::erase(children, row); }
};
int main()
{
  using Insertion = mod_settings::native::RowInsertion<Handle>;
  Insertion  pending;
  Host       host;
  const auto operation = pending.Begin(1, 30, 40, 50, *host.Children(1));
  assert(operation && pending.Pending(1));
  host.children.push_back(10); // Native append happens before the failure.
  pending.Retry(host);         // Native reentry before it has returned cannot publish or remove the row.
  assert(pending.Size() == 1 && host.configured == 0 && host.quarantine.Size() == 0);
  pending.Complete(operation, false); // Contained native exception after mutation.
  host.count_known = false;
  pending.Retry(host);
  assert(pending.Size() == 1 && host.quarantine.Size() == 0);
  host.count_known = true;
  host.items_known = false;
  pending.Retry(host);
  assert(pending.Size() == 1);
  host.items_known     = true;
  host.ownership_known = false;
  pending.Retry(host);
  assert(pending.Size() == 1);
  host.ownership_known = true;
  pending.Retry(host);
  assert(pending.Size() == 0 && host.quarantine.Contains(10) && host.configured == 0);
  host.quarantine.Retry(host);
  assert(host.children == std::vector<int>{20} && host.quarantine.Size() == 0);

  // Successful insertion is configured only after a complete, exact scan; a foreign new row is never ours.
  auto second = pending.Begin(1, 30, 40, 50, *host.Children(1));
  host.children.insert(host.children.end(), {10, 21});
  pending.Complete(second, true);
  pending.Retry(host);
  assert(pending.Size() == 0 && host.configured == 1 && host.children.size() == 3);
  std::erase(host.children, 10);

  // No report-root retention: collected parents retire recovery. A collected unattached query cannot authorize
  // configuration, so a surviving owned row moves to exact cleanup instead.
  auto third = pending.Begin(1, 30, 40, 50, *host.Children(1));
  host.children.push_back(10);
  pending.Complete(third, true);
  Handle::collected.insert(50);
  pending.Retry(host);
  assert(host.configured == 1 && pending.Size() == 0 && host.quarantine.Contains(10));
  host.quarantine.Retry(host);
  Handle::collected.clear();
  auto fourth = pending.Begin(1, 30, 40, 50, *host.Children(1));
  pending.Complete(fourth, false);
  Handle::collected.insert(1);
  pending.Retry(host);
  assert(pending.Size() == 0);
  Handle::collected.clear();
  std::cout << "Native row insertion: pre-call custody, unknown scans, partial mutation, exact linkage and GC passed\n";
}
