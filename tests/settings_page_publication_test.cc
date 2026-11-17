#include "settings/native_category_publication.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
struct Handle {
  int                         object = 0;
  static inline std::set<int> collected;
  static inline bool          roots_fail = false;
  static Handle               Weak(int object)
  { return {object}; }
  static Handle Strong(int object)
  { return {roots_fail ? 0 : object}; }
  int Get() const
  { return collected.contains(object) ? 0 : object; }
  explicit operator bool() const
  { return Get() != 0; }
};
struct Host {
  std::vector<int>    children{99};
  int                 parent = 1, query = 3, removals = 0;
  bool                count_known = true, query_known = true, parent_known = true, remove_works = true;
  std::optional<bool> Owned(int row, int expected)
  { return query_known ? std::optional<bool>(row == 2 && query == expected) : std::nullopt; }
  std::optional<std::size_t> Occurrences(int expected_parent, int row)
  {
    Check(expected_parent == 1, "publication examines only its observed native parent");
    return count_known ? std::optional<std::size_t>(std::ranges::count(children, row)) : std::nullopt;
  }
  std::optional<int> Parent(int row)
  {
    Check(row == 2, "publication examines its exact staged root");
    return parent_known ? std::optional<int>(parent) : std::nullopt;
  }
  void Remove(int expected_parent, int row)
  {
    Check(expected_parent == 1 && row == 2, "cleanup cannot remove a foreign category");
    ++removals;
    if (remove_works) {
      if (auto found = std::ranges::find(children, row); found != children.end())
        children.erase(found);
      parent = 0;
    }
  }
};
using Publication = mod_settings::native::CategoryPublication<Handle>;
using State       = mod_settings::native::CategoryPublicationState;
} // namespace

int main()
{
  Host        host;
  Publication pending;
  Check(pending.Begin(1, 2, 3), "custody starts with the exact off-screen root and query before AddChild");
  host.children.push_back(2);
  pending.Retry(host);
  Check(pending.state() == State::AwaitingReturn && host.removals == 0,
        "native reentry before AddChild returns can neither publish nor remove");
  pending.Complete(false); // Append, then throw from the child's parent setter.
  host.count_known = false;
  pending.Retry(host);
  Check(pending.state() == State::Pending && host.removals == 0, "unknown post-throw list retains operation custody");
  host.count_known = true;
  host.query_known = false;
  pending.Retry(host);
  Check(pending.state() == State::Pending && host.removals == 0, "unknown query cannot authorize cleanup");
  host.query_known  = true;
  host.remove_works = false;
  pending.Retry(host);
  Check(pending.state() == State::Removing && host.removals == 1,
        "a successful-looking removal without absence remains pending");
  host.remove_works = true;
  pending.Retry(host);
  Check(pending.state() == State::Retired && host.children == std::vector<int>{99},
        "the exact root is retired only after observed removal; foreign siblings survive");

  Publication success;
  Host        complete;
  Check(success.Begin(1, 2, 3), "new publication starts independently");
  complete.children.push_back(2);
  success.Complete(true);
  complete.parent_known = false;
  success.Retry(complete);
  Check(success.state() == State::Pending, "AddChild success without a parent observation is not publication");
  complete.parent_known = true;
  success.Retry(complete);
  Check(success.state() == State::Published && complete.removals == 0,
        "exact single membership, parent and query establish publication");
  success.Revoke();
  success.Retry(complete);
  Check(success.state() == State::Retired && complete.children == std::vector<int>{99},
        "revocation cleans only the known published root");

  Publication foreign;
  Host        replaced;
  Check(foreign.Begin(1, 2, 3), "replacement observation begins under the original identity");
  replaced.children.push_back(2);
  foreign.Complete(false);
  replaced.query = 7;
  foreign.Retry(replaced);
  Check(foreign.state() == State::Retired && replaced.removals == 0 && replaced.children.size() == 2,
        "a replaced query does not authorize removing somebody else's current category");

  Publication duplicates;
  Host        repeated;
  Check(duplicates.Begin(1, 2, 3), "duplicate-root recovery retains known object identity");
  repeated.children.insert(repeated.children.end(), {2, 2});
  duplicates.Complete(true);
  duplicates.Retry(repeated);
  Check(duplicates.state() == State::Removing && repeated.removals == 1,
        "duplicate exact membership cannot publish and one removal is not complete");
  duplicates.Retry(repeated);
  Check(duplicates.state() == State::Retired && repeated.children == std::vector<int>{99},
        "duplicate exact roots drain without deleting foreign siblings");

  Publication moved;
  Host        moved_host;
  Check(moved.Begin(1, 2, 3), "moving-parent observation starts with known identities");
  moved_host.children.push_back(2);
  moved_host.parent = 8;
  moved.Complete(true);
  moved.Retry(moved_host);
  Check(moved.state() == State::Retired && moved_host.removals == 0,
        "another current parent prevents cleanup from clobbering the new relationship");

  Publication rooted;
  Host        rooted_host;
  Check(rooted.Begin(1, 2, 3), "temporary-root failure keeps weak publication custody");
  rooted_host.children.push_back(2);
  rooted.Complete(true);
  Handle::roots_fail = true;
  rooted.Retry(rooted_host);
  Check(rooted.state() == State::Pending, "failed strong rooting is not absent native state");
  Handle::roots_fail = false;
  Handle::collected.insert(2);
  rooted.Retry(rooted_host);
  Check(rooted.state() == State::Retired, "collected weak root ends custody without retaining the closed screen");
  std::cout << "PASS staged native category publication, exact identity and cleanup custody\n";
}
