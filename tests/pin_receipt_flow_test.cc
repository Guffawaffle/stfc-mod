// Proposal fixture: existing desired-order receipts, without native objects,
// a writer thread, user state files or a second persistence implementation.
#include "pending_pin_order.h"
#include "pinned_ship_order.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

int main()
{
  try {
    using Pending    = pinned_ship_state::PendingPinOrder;
    using Outcome    = mod_state::SaveOutcome;
    using Completion = Pending::Completion;
    Pending                    pending;
    const std::vector<int64_t> applied{101};

    auto first = applied;
    Check(pinned_ship_order::Toggle(first, 202), "first valid ship was not toggled");
    pending.Queue({first, {"UNKNOWN HULL"}}, 41);
    Check(pending.Observe({40, Outcome::Saved}) == Completion::Waiting,
          "an older save completed the latest desired edit");
    Check(pending.Observe({41, Outcome::Pending}) == Completion::Waiting, "admission was mistaken for durable success");

    auto second = pending.Desired()->ids;
    Check(pinned_ship_order::Toggle(second, 101), "rapid edit did not compose against desired pins");
    pending.Queue({second, pending.Desired()->legacy_names}, 42);
    Check(second == std::vector<int64_t>{202}, "rapid edit composed against the applied order");
    Check(pending.Observe({41, Outcome::Saved}) == Completion::Waiting, "superseded native order could be published");
    Check(pending.Observe({42, Outcome::Failed}) == Completion::Failed, "write failure was accepted as a saved order");
    const auto rejected = pending.Take();
    Check(rejected.ids == second && rejected.legacy_names == std::vector<std::string>{"UNKNOWN HULL"}
              && !pending.Desired(),
          "failed desired-state retirement lost its original data");
    Check(applied == std::vector<int64_t>{101}, "a failed receipt mutated the separately applied state");

    pending.Queue({second, {"UNKNOWN HULL"}}, 43);
    Check(pending.Observe({43, Outcome::Saved}) == Completion::Saved,
          "the exact latest saved receipt did not become publishable");
    const auto saved = pending.Take();
    Check(saved.ids == std::vector<int64_t>{202} && !pending.Desired(), "saved order was not transferred once");

    pending.Queue({{303}, {}}, 44);
    Check(pending.Observe({45, Outcome::Saved}) == Completion::Failed,
          "a different future group receipt was accepted as this edit");
    (void)pending.Take();
    Check(!pinned_ship_order::Toggle(second, 0) && !pinned_ship_order::Toggle(second, -1)
              && second == std::vector<int64_t>{202},
          "unavailable/nonpositive ship identities changed the desired order");
    std::cout << "Pin receipt flow passed: desired composition, durable acknowledgement, stale and failed rejection\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
