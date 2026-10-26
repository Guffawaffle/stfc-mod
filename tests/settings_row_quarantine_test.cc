#include "settings/native_row_quarantine.h"
#include <cassert>
#include <functional>
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
  { return object != 0; }
};
struct Host {
  std::set<int>         children{10, 20}; // 10 is ours; 20 belongs to the game/another feature.
  bool                  inspect = true, owned = true, fail_remove = true, throw_after_remove = false;
  unsigned              removals = 0;
  std::function<void()> reentry;
  std::optional<bool>   Contains(int, int row)
  { return inspect ? std::optional(children.contains(row)) : std::nullopt; }
  std::optional<bool> Owned(int row, int getter, int setter)
  { return row == 10 && getter == 30 && setter == 40 ? std::optional(owned) : std::nullopt; }
  void Remove(int, int row)
  {
    ++removals;
    if (reentry)
      reentry();
    if (!fail_remove)
      children.erase(row);
    // Native TryCall contains the exception; its returned success can be false either side of mutation.
    (void)throw_after_remove;
  }
};
int main()
{
  mod_settings::native::RowQuarantine<Handle> quarantine;
  Host                                        host;
  assert(quarantine.Add(1, 10, 30, 40) && quarantine.Add(1, 10, 30, 40) && quarantine.Size() == 1);
  quarantine.Retry(host); // Append-then-throw followed by removal failing before mutation.
  assert(quarantine.Contains(10) && host.children == std::set<int>({10, 20}) && host.removals == 1);
  host.inspect = false;
  quarantine.Retry(host);
  assert(quarantine.Size() == 1 && host.removals == 1); // Unknown membership is not absence.
  host.inspect      = true;
  Handle::fail_root = true;
  quarantine.Retry(host);
  assert(quarantine.Size() == 1 && host.removals == 1);
  Handle::fail_root       = false;
  host.fail_remove        = false;
  host.throw_after_remove = true;
  quarantine.Retry(host);
  assert(quarantine.Size() == 0 && host.children == std::set<int>({20}) && host.removals == 2);
  // Positive callback-owner replacement revokes deletion authority, without deleting the native row.
  host.children.insert(10);
  host.owned = false;
  quarantine.Add(1, 10, 30, 40);
  quarantine.Retry(host);
  assert(quarantine.Size() == 0 && host.children.contains(10) && host.removals == 2);
  host.owned = true;
  quarantine.Add(1, 10, 30, 40);
  host.reentry = [&] { quarantine.Add(2, 11, 31, 41); };
  quarantine.Retry(host);
  assert(quarantine.Size() == 1 && quarantine.Contains(11) && host.children.contains(20));
  Handle::collected.insert(11);
  quarantine.Retry(host);
  assert(quarantine.Size() == 0);
  std::cout
      << "Settings insertion quarantine: mutation failures, exact custody, unknowns, replacement and reentry passed\n";
}
