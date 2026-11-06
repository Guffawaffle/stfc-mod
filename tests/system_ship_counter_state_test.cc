#include "features/system_ship_counter_state.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace system_ship_counter;
unsigned    checks = 0;
const Scope here{11, 101, 0}, elsewhere{11, 202, 0};

void Check(bool condition, const char* message)
{
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}

Observation Player(int64_t id, Scope address = here, bool own = false)
{
  Observation value;
  value.id       = id;
  value.address  = address;
  value.type     = 1;
  value.state    = 0;
  value.isActive = true;
  value.isLocal  = own;
  return value;
}

void Expect(Counter& counter, Totals expected, const char* message)
{
  const auto count = counter.Count();
  Check(count.has_value(), message);
  Check(*count == expected, message);
}

void UnknownIsDifferentFromEmpty()
{
  Counter counter;
  Check(!counter.Count(), "a new service has no observed count");
  counter.Seed({Player(1)});
  Check(!counter.Count(), "a seed without a viewed scope cannot invent a count");
  Check(counter.Focus(here), "first viewed scope establishes a new boundary");
  Check(counter.CurrentScope() == here, "full viewed scope is retained");
  Check(!counter.Count(), "focus alone does not establish an empty system");
  counter.Seed({});
  Expect(counter, {}, "a successfully observed empty collection is known zero");
  Check(!counter.Focus(here), "repeated focus on the same address does not restart acquisition");
  Expect(counter, {}, "same-scope focus preserves known zero");
  counter.Unavailable();
  Check(!counter.Count(), "failed acquisition hides even a previously known zero");
  counter.Apply(Player(1));
  Check(!counter.Count(), "a partial update cannot claim acquisition is complete");
  counter.Seed({Player(1)});
  Expect(counter, {1, 0, 0, 0, 1}, "a complete replacement seed restores availability");
  Check(counter.Focus(elsewhere), "a new viewed address invalidates the old cohort");
  Check(!counter.Count(), "the old system's result is not shown under the new address");
}

void IdentityAndNativeEligibility()
{
  Counter counter;
  counter.Focus(here);
  auto                     own   = Player(10, here, true);
  auto                     other = Player(20);
  std::vector<Observation> rows{own, other, own, other};
  for (int type : {0, 2, 3, 4, 5, 6}) {
    auto npc = Player(100 + type);
    npc.type = type;
    rows.push_back(npc);
  }
  rows.push_back(Player(0));
  rows.push_back(Player(-1));
  rows.push_back(Player(201, {12, 101, 0})); // Same system number, different galaxy.
  rows.push_back(Player(202, {11, 102, 0}));
  rows.push_back(Player(203, {11, 101, 1})); // Same galaxy/system, different instance.
  auto destroyed      = Player(204);
  destroyed.destroyed = true;
  rows.push_back(destroyed);
  auto inactive     = Player(205);
  inactive.isActive = false;
  rows.push_back(inactive);
  auto docked  = Player(305);
  docked.state = 5;
  rows.push_back(docked);
  for (int reason : {1, 2, 3, 4, 5}) {
    auto removed          = Player(400 + reason);
    removed.removalReason = reason;
    rows.push_back(removed);
  }
  other.removalReason = 0;
  rows.push_back(other);
  counter.Seed(rows);
  Expect(counter, {2, 1, 0, 0, 2}, "only unique, active, present player ships qualify");

  other.isLocal = true;
  counter.Apply(other);
  Expect(counter, {2, 2, 0, 0, 2}, "a new observation replaces one fleet rather than adding a duplicate");
  other.address = elsewhere;
  counter.Apply(other);
  Expect(counter, {1, 1, 0, 0, 1}, "an observed exit removes the old matching entry");
  other.address  = here;
  other.isActive = false;
  counter.Apply(other);
  Expect(counter, {1, 1, 0, 0, 1}, "inactive receipts cannot establish presence");
  other.isActive = true;
  counter.Apply(other);
  Expect(counter, {2, 2, 0, 0, 2}, "a valid current observation can restore an eligible fleet");

  rows.clear();
  for (int state : {0, 1, 2, 3, 4, 6}) {
    auto present  = Player(500 + state);
    present.state = state;
    rows.push_back(present);
  }
  counter.Seed(rows);
  Expect(counter, {6, 0, 0, 0, 6}, "warp charging and recall still count when the native full address is here");
}

void CourseTickAddressReplay()
{
  Counter counter;
  auto    course = Player(30);
  course.state   = 2;
  counter.Focus(here);
  counter.Seed({course});
  Expect(counter, {1, 0, 0, 0, 1}, "warp state alone does not erase an observed in-system ship");

  // The native course tick changes the address to galaxy transit independently
  // of a new DeploymentService server receipt. Apply must consume that change.
  course.address.system = -1;
  counter.Apply(course);
  Expect(counter, {}, "a course tick entering galaxy transit removes the old in-system entry without a receipt");
  course.address = here;
  course.state   = 4;
  counter.Apply(course);
  Expect(counter, {1, 0, 0, 0, 1}, "a nonremoved recall ship can be observed at the current native system address");
  course.address.instance = 1;
  counter.Apply(course);
  Expect(counter, {}, "a course tick in another instance cannot remain under the current system's count");

  course.address = here;
  counter.Apply(course);
  counter.Remove(course.id);
  course.state = 2;
  counter.Apply(course);
  counter.Apply(course);
  Expect(counter, {}, "repeated warp/course ticks cannot resurrect a retained object after removal intent");
  course.state = 4;
  counter.Apply(course);
  Expect(counter, {}, "a recall state-change tick also cannot clear a removal tombstone");
  counter.Focus(elsewhere);
  counter.Seed({course});
  counter.Focus(here);
  counter.Seed({course});
  Expect(counter, {}, "a retained course object remains removed after a focus roundtrip and bootstrap");

  counter.Receipt(course.id);
  counter.Apply(course);
  Expect(counter, {1, 0, 0, 0, 1}, "only a fresh server receipt allows the retained identity to re-enter");
  course.address.system = -1;
  counter.Apply(course);
  Expect(counter, {}, "the next course tick can leave again without needing another server receipt");
}

void RetainedQueueGhostReplay()
{
  Counter    counter;
  const auto own   = Player(10, here, true);
  const auto ghost = Player(20);
  counter.Focus(here);
  counter.Seed({own, ghost});
  Expect(counter, {2, 1, 0, 0, 2}, "initial native cache contains two known ships");

  // The native queue can retain this same object after authoritative removal.
  // A state/render callback, a cache reread and a later bootstrap are not new
  // server receipts and must never undo that removal intent.
  counter.Remove(ghost.id);
  Expect(counter, {1, 1, 0, 0, 1}, "removal intent removes the ship before native cache eviction");
  counter.Apply(ghost);
  counter.Apply(ghost);
  Expect(counter, {1, 1, 0, 0, 1}, "late retained-object observations do not resurrect a queue ghost");
  counter.Focus(elsewhere);
  counter.Seed({own, ghost});
  Expect(counter, {}, "retained contacts do not leak into a different viewed system");
  counter.Focus(here);
  counter.Seed({own, ghost});
  Expect(counter, {1, 1, 0, 0, 1}, "returning and reseeding the original system preserves the removal tombstone");

  // This models the private update hook: receipt before original, then original
  // rejects the fresh record and enters private removal before the post-read.
  counter.Receipt(ghost.id);
  counter.Remove(ghost.id);
  counter.Apply(ghost);
  Expect(counter, {1, 1, 0, 0, 1}, "nested native removal wins over the enclosing receipt and post-read");
  counter.Unavailable();
  Check(!counter.Count(), "unavailability does not publish the retained cohort");
  counter.Seed({own, ghost});
  Expect(counter, {1, 1, 0, 0, 1}, "recovery reseed also preserves nested removal intent");

  counter.Receipt(ghost.id);
  Expect(counter, {1, 1, 0, 0, 1}, "a receipt alone does not recreate a ship without a current observation");
  counter.Apply(ghost);
  Expect(counter, {2, 1, 0, 0, 2}, "a fresh eligible receipt permits genuine re-entry");
  counter.Apply(ghost);
  Expect(counter, {2, 1, 0, 0, 2}, "repeated post-read remains idempotent");
  counter.Remove(0);
  counter.Remove(-1);
  Expect(counter, {2, 1, 0, 0, 2}, "invalid removal identities do not erase a valid ship");

  counter.Remove(ghost.id);
  counter.Reset();
  Check(!counter.CurrentScope() && !counter.Count(), "service reset clears the old address and observation readiness");
  counter.Focus(here);
  counter.Seed({ghost});
  Expect(counter, {1, 0, 0, 0, 1}, "a new service can observe an identity previously removed in the old service");
}

void CloakedContactsAndUnknownMetadata()
{
  Counter counter;
  auto    detected_cloak       = Player(1, here, true);
  detected_cloak.cloakstatus   = 1;
  detected_cloak.detected      = true;
  auto undetected_cloak        = Player(2);
  undetected_cloak.cloakstatus = 1;
  undetected_cloak.detected    = false;
  auto unknown                 = Player(3);
  auto not_cloaked             = Player(4);
  not_cloaked.cloakstatus      = 0;
  not_cloaked.detected         = false;
  counter.Focus(here);
  counter.Seed({detected_cloak, undetected_cloak, unknown, not_cloaked});
  Expect(counter, {4, 1, 2, 1, 1}, "known cloaked contacts remain ships; missing cloak metadata remains unknown");

  unknown.cloakstatus = 1;
  counter.Apply(unknown);
  Expect(counter, {4, 1, 3, 1, 0}, "a known cloak with unknown detection is not fabricated as undetected");
  unknown.detected = false;
  counter.Apply(unknown);
  Expect(counter, {4, 1, 3, 2, 0}, "a later independent detection observation refines only that subtotal");
  undetected_cloak.cloakstatus = 0;
  counter.Apply(undetected_cloak);
  Expect(counter, {4, 1, 2, 1, 0}, "an observed cloak change does not change total ship presence");
  detected_cloak.cloakstatus.reset();
  detected_cloak.detected.reset();
  counter.Apply(detected_cloak);
  Expect(counter, {4, 1, 1, 1, 1}, "losing optional metadata never converts the fleet to an invented known zero");
}
} // namespace

int main()
{
  try {
    UnknownIsDifferentFromEmpty();
    IdentityAndNativeEligibility();
    CourseTickAddressReplay();
    RetainedQueueGhostReplay();
    CloakedContactsAndUnknownMetadata();
    std::cout << "System ship counter: " << checks << " replay checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "System ship counter fixture failed: " << error.what() << '\n';
    return 1;
  }
}
