#include "keyboard_layout_diagnostics.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Check(bool condition, const char* name)
{
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}
} // namespace

int main()
{
  keyboard_layout::DiagnosticState state;
  const auto                       initial = state.Snapshot();
  Check(initial.layout.empty() && initial.status == "physical" && initial.reason == "configured_physical"
            && initial.generation == 0,
        "startup physical diagnostics exist before any native query");
  Check(!state.ChangedSince(0), "startup generation can be acknowledged");
  Check(!state.Publish("", "physical", "configured_physical"), "unchanged configuration stays quiet");

  uint64_t saved_generation = initial.generation;
  Check(state.Publish("", "pending", "awaiting_game_input"), "layout mode publishes pending");
  Check(state.Snapshot().generation == 1 && state.ChangedSince(saved_generation), "pending marks vars stale");
  Check(state.Publish("US", "resolved", "layout_lookup"), "first successful lookup publishes");
  const auto american = state.Snapshot();
  Check(american.generation == 2 && american.layout == "US", "resolved layout is retained");

  // A failed writer leaves its cursor alone. Repeated inspection cannot consume the pending update.
  Check(state.ChangedSince(saved_generation) && state.ChangedSince(saved_generation), "read does not acknowledge");
  saved_generation = american.generation;
  Check(!state.ChangedSince(saved_generation), "successful write acknowledges only its captured generation");
  Check(!state.Publish("US", "resolved", "layout_lookup"), "quiet lookup does not dirty vars again");

  Check(state.Publish("German", "resolved", "layout_lookup"), "resolved to resolved layout switch publishes");
  Check(state.Snapshot().generation == 3 && state.ChangedSince(saved_generation), "layout change advances generation");
  Check(american.layout == "US" && american.generation == 2, "writer snapshot remains stable after later change");
  Check(!state.ChangedSince(state.Snapshot().generation), "latest generation can be acknowledged");

  Check(state.Publish("German", "partial", "unresolved_keys_disabled"), "partial mapping is observable");
  Check(state.Publish("French", "partial", "unresolved_keys_disabled"), "partial layout switch also publishes");
  Check(state.Snapshot().generation == 5, "each changed layout or status advances once");
  Check(state.Publish("", "unavailable", "keyboard_or_layout_absent"), "lost layout clears diagnostic identity");
  Check(state.Snapshot().layout.empty() && state.Snapshot().status == "unavailable", "unavailable snapshot retained");
  Check(!state.Publish("", "unavailable", "keyboard_or_layout_absent"), "repeated unavailable state stays quiet");
  Check(state.Publish("", "unavailable", "frame_unavailable"), "changed unavailable reason publishes");
  Check(state.Snapshot().generation == 7, "reason-only change advances generation");

  // Intermediate transitions may outpace a writer; the latest complete snapshot remains available.
  Check(state.Publish("", "physical", "configured_physical"), "physical mode transition publishes");
  Check(state.Publish("", "pending", "awaiting_game_input"), "reenabling layout mode publishes pending");
  Check(state.Publish("US", "resolved", "layout_lookup"), "recovered layout publishes resolved");
  Check(state.Snapshot().generation == 10 && state.ChangedSince(saved_generation),
        "latest snapshot survives skipped writes");
  saved_generation = state.Snapshot().generation;
  Check(!state.ChangedSince(saved_generation), "latest successful write clears stale observation");
  Check(state.Publish("", "physical", "configured_physical"), "disable publishes final physical state");
  Check(state.Snapshot().layout.empty() && state.Snapshot().reason == "configured_physical",
        "physical diagnostics cannot retain stale layout identity");

  std::cout << "Keyboard layout diagnostic publication and writer cursor checks passed\n";
}
