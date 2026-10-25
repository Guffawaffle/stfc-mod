#include "mod_state_writer.h"
#include "pending_pin_order.h"
#include <condition_variable>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace mod_state;
using namespace std::chrono_literals;
void Check(bool value)
{
  if (!value)
    throw std::runtime_error("state writer fixture failed");
}
int main()
{
  std::mutex              mutex;
  std::condition_variable wake;
  bool                    first_started = false, release = false;
  std::vector<StateEdit>  committed;
  const auto              owner = std::this_thread::get_id();
  StateWriter             writer(
      [&](const StateEdit& edit) {
        Check(std::this_thread::get_id() != owner);
        std::unique_lock lock(mutex);
        if (!first_started) {
          first_started = true;
          wake.notify_all();
          wake.wait(lock, [&] { return release; });
        }
        if (!edit.ids.empty() && edit.ids.front() == -1)
          return false;
        committed.push_back(edit);
        return true;
      },
      0ms);
  struct Unblock {
    std::mutex&              mutex;
    std::condition_variable& wake;
    bool&                    release;
    ~Unblock()
    {
      {
        std::lock_guard lock(mutex);
        release = true;
      }
      wake.notify_all();
    }
  } unblock{mutex, wake, release};
  pinned_ship_state::PendingPinOrder intent;
  StateEdit                          reserved{Group::ShipPins, {}, {}};
  reserved.ids.reserve(512);
  reserved.ids.push_back(1);
  reserved.legacy_names.reserve(32);
  reserved.legacy_names.emplace_back("unresolved");
  reserved.legacy_names.front().reserve(4096);
  const auto reserved_charge = sizeof(StateEdit) + reserved.ids.capacity() * sizeof(int64_t)
                               + reserved.legacy_names.capacity() * sizeof(std::string)
                               + reserved.legacy_names.front().capacity() + 1;
  Check(StateWriter::Charge(reserved) == reserved_charge);
  const auto first = writer.Submit(std::move(reserved));
  intent.Queue({{1}, {"unresolved"}}, first);
  {
    std::unique_lock lock(mutex);
    Check(wake.wait_for(lock, 3s, [&] { return first_started; }));
  }
  Check(writer.RetainedBytes() == reserved_charge); // Includes unused capacity while in flight.
  auto desired = *intent.Desired();
  desired.ids.push_back(2); // Second input composes against pending, not applied.
  auto latest = writer.Submit({Group::ShipPins, desired.ids, desired.legacy_names});
  intent.Queue(std::move(desired), latest);
  for (int index = 0; index < 1000; ++index) {
    auto next         = *intent.Desired();
    next.legacy_names = {"latest-" + std::to_string(index)};
    latest            = writer.Submit({Group::ShipPins, next.ids, next.legacy_names});
    Check(latest != 0 && writer.RetainedBytes() <= StateWriter::MaximumRetainedBytes);
    intent.Queue(std::move(next), latest);
  }
  const auto officer = writer.Submit({Group::OfficerOrder, {9, 8}, {}});
  const auto opacity = writer.Submit({Group::ConsoleOpacity, {}, {}, 91});
  Check(opacity);
  Check(officer && writer.Status(Group::ShipPins).revision == latest);
  Check(intent.Observe({first, SaveOutcome::Saved}) == decltype(intent)::Completion::Waiting);
  Check(intent.Observe(writer.Status(Group::ShipPins)) == decltype(intent)::Completion::Waiting);
  Check(!writer.Submit({Group::ShipPins, std::vector<int64_t>(StateWriter::MaximumEntries + 1), {}}));
  Check(!writer.Submit({Group::ShipPins, {}, {std::string(StateWriter::MaximumEditBytes, 'x')}}));
  const auto retained_before_rejections = writer.RetainedBytes();
  StateEdit  large_ids{Group::ShipPins, {}, {}};
  large_ids.ids.reserve(StateWriter::MaximumEditBytes / sizeof(int64_t) + 1);
  large_ids.ids.push_back(1);
  Check(!StateWriter::Charge(large_ids) && !writer.Submit(std::move(large_ids)));
  StateEdit large_slots{Group::ShipPins, {}, {}};
  large_slots.legacy_names.reserve(StateWriter::MaximumEditBytes / sizeof(std::string) + 1);
  large_slots.legacy_names.emplace_back("x");
  Check(!StateWriter::Charge(large_slots) && !writer.Submit(std::move(large_slots)));
  StateEdit large_string{Group::ShipPins, {}, {"x"}};
  large_string.legacy_names.front().reserve(StateWriter::MaximumEditBytes);
  Check(!StateWriter::Charge(large_string) && !writer.Submit(std::move(large_string)));
  Check(writer.RetainedBytes() == retained_before_rejections);
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  wake.notify_all();
  const auto until = std::chrono::steady_clock::now() + 3s;
  while ((writer.Status(Group::ShipPins).outcome == SaveOutcome::Pending
          || writer.Status(Group::OfficerOrder).outcome == SaveOutcome::Pending
          || writer.Status(Group::ConsoleOpacity).outcome == SaveOutcome::Pending)
         && std::chrono::steady_clock::now() < until)
    std::this_thread::yield();
  Check(intent.Observe(writer.Status(Group::ShipPins)) == decltype(intent)::Completion::Saved);
  auto applied = intent.Take();
  Check(applied.ids == std::vector<int64_t>({1, 2})
        && applied.legacy_names == std::vector<std::string>({"latest-999"}));
  Check(!intent.Desired());
  {
    std::lock_guard lock(mutex);
    Check(committed.size() == 4);
    Check(committed[0].ids == std::vector<int64_t>({1}));
    Check(committed[1].ids == applied.ids && committed[1].legacy_names == applied.legacy_names);
    Check(committed[2].group == Group::OfficerOrder);
    Check(committed[3].group == Group::ConsoleOpacity && committed[3].opacity_percent == 91);
  }
  const auto failed = writer.Submit({Group::ShipPins, {-1}, {}});
  intent.Queue({{-1}, {}}, failed);
  const auto failed_until = std::chrono::steady_clock::now() + 3s;
  while (writer.Status(Group::ShipPins).outcome == SaveOutcome::Pending
         && std::chrono::steady_clock::now() < failed_until)
    std::this_thread::yield();
  Check(intent.Observe(writer.Status(Group::ShipPins)) == decltype(intent)::Completion::Failed);
  intent.Take(); // Native applied remains {1,2} on failure.
  Check(applied.ids == std::vector<int64_t>({1, 2}));
  std::cout << "State writer: bounded active/latest pending, group ordering, exact durable receipts, composition and "
               "failure passed\n";
}
