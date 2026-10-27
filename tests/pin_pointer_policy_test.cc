#include "pin_pointer_policy.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace ship_pin_pointer;
using namespace std::chrono_literals;
constexpr Identity      first{1, 1, 101}, rebound{1, 2, 202}, other{2, 3, 101};
const Clock::time_point epoch{};
void                    Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
// Fake input identities advance independently of the hold/pair clock.
struct ObservedPolicy : Policy {
  uint64_t input_epoch = 0;
  bool     Press(Identity identity, std::uintptr_t scroll, int pointer, Point position, Clock::time_point now)
  { return Policy::Press(identity, scroll, pointer, position, now, ++input_epoch); }
  Click Release(Identity identity, bool fresh, bool inside, Clock::time_point now)
  { return Policy::Release(identity, fresh, inside, now, ++input_epoch); }
};
void Clicks()
{
  ObservedPolicy policy;
  Check(policy.Press(first, 9, -1, {5, 5}, epoch), "mouse badge press not admitted");
  Check(policy.Release(first, true, true, epoch + 20ms) == Click::Consume,
        "single badge click must consume native selection without pinning");
  Check(policy.Press(first, 9, -1, {5, 5}, epoch + 50ms), "second press not admitted");
  Check(policy.Release(first, true, true, epoch + 70ms) == Click::Toggle, "same live badge pair must toggle once");
  Check(policy.Press(first, 9, -1, {5, 5}, epoch + 80ms)
            && policy.Release(first, true, true, epoch + 90ms) == Click::Consume,
        "a third click must not reuse the consumed pair");
  policy.Press(other, 9, -1, {5, 5}, epoch + 100ms);
  Check(policy.Release(other, true, true, epoch + 110ms) == Click::Consume,
        "another tile holding the same ship must not inherit a pair");
  policy.InvalidateTile(other.tile);
  policy.Press(other, 9, -1, {5, 5}, epoch + 130ms);
  Check(policy.Release(other, true, true, epoch + 140ms) == Click::Consume, "rebind invalidation retained a pair");
  policy.Press(rebound, 9, -1, {5, 5}, epoch + 150ms);
  Check(policy.Release(rebound, true, true, epoch + 160ms) == Click::Consume,
        "pooled address/new generation must start a new badge pair");
  policy.Press(rebound, 9, -1, {5, 5}, epoch + 170ms);
  Check(policy.Release(rebound, false, true, epoch + 180ms) == Click::Native,
        "keyboard/touch activation must not become a mouse pin action");
  policy.Press(rebound, 9, -1, {5, 5}, epoch + 190ms);
  Check(policy.Release(rebound, true, false, epoch + 200ms) == Click::Native,
        "release outside badge must preserve native routing");
  policy.Press(rebound, 9, -1, {5, 5}, epoch + 210ms);
  policy.Release(rebound, true, true, epoch + 220ms);
  policy.Tick(true, false, false, epoch + 621ms);
  policy.Press(rebound, 9, -1, {5, 5}, epoch + 630ms);
  Check(policy.Release(rebound, true, true, epoch + 640ms) == Click::Consume,
        "expired badge click must not complete a double click");
}
void ScrollingAndDrag()
{
  ObservedPolicy policy;
  policy.Press(first, 9, -1, {0, 0}, epoch);
  Check(policy.Begin(9, -1, Point{20, 0}, true, epoch + 124ms) == Route::Native,
        "movement before the hold threshold must remain native scrolling");
  Check(policy.Drag(9, -1, Point{30, 0}, true) == Route::Native, "native scroll was captured later in the gesture");
  Check(policy.Release(first, true, true, epoch + 160ms) == Click::Consume,
        "scroll completion must suppress its stale badge click");

  policy.Press(first, 9, -1, {0, 0}, epoch + 200ms);
  Check(policy.Begin(8, -1, Point{9, 0}, true, epoch + 325ms) == Route::Native
            && policy.Begin(9, 0, Point{9, 0}, true, epoch + 325ms) == Route::Native,
        "unrelated ScrollRect or pointer must stay native");
  Check(policy.Begin(9, -1, Point{7, 0}, true, epoch + 325ms) == Route::Consume && !policy.Dragging(),
        "held movement below eight pixels should remain pending");
  Check(policy.Drag(9, -1, Point{8, 0}, true) == Route::Consume && policy.Dragging(),
        "eight pixels did not start drag");
  const auto end = policy.End(9, -1, true, true);
  Check(end.route == Route::Consume && end.commit && !policy.Source(), "qualified drag completion not returned once");
  Check(policy.Release(first, true, true, epoch + 350ms) == Click::Consume,
        "completed drag must not also click or assign the ship");
  Check(policy.End(9, -1, true, true).route == Route::Native, "completed drag retained capture");
}
void Cancellation()
{
  ObservedPolicy policy;
  policy.Press(first, 9, -1, {0, 0}, epoch);
  policy.Begin(9, -1, Point{8, 0}, true, epoch + 125ms);
  Check(policy.Drag(9, -1, Point{9, 0}, false) == Route::Consume && !policy.Source(),
        "lost source must cancel an already captured drag");
  policy.Press(first, 9, -1, {0, 0}, epoch + 200ms);
  policy.Begin(9, -1, Point{8, 0}, true, epoch + 325ms);
  Check(!policy.End(9, -1, true, false).commit, "unknown drop coordinates must not request an action");
  policy.Press(first, 9, -1, {0, 0}, epoch + 400ms);
  policy.Begin(9, -1, Point{8, 0}, true, epoch + 525ms);
  policy.Tick(true, true, false, epoch + 550ms);
  policy.Tick(true, true, false, epoch + 799ms);
  Check(policy.Dragging(), "end-drag grace period was not preserved");
  policy.Tick(true, true, false, epoch + 800ms);
  Check(!policy.Source(), "lost end-drag callback was not released automatically");
  policy.Press(first, 9, -1, {0, 0}, epoch + 900ms);
  policy.Tick(false, true, true, epoch + 901ms);
  Check(!policy.Source() && policy.Release(first, true, true, epoch + 902ms) != Click::Toggle,
        "focus loss retained a publishable mouse action");
  Check(!policy.Press(first, 9, 0, {0, 0}, epoch), "touch pointer was treated as mouse");
  Check(!policy.Press({}, 9, -1, {0, 0}, epoch), "unknown ship identity was admitted");
  Check(!policy.Press(first, 0, -1, {0, 0}, epoch), "missing scroll owner was admitted");
  Check(!policy.Press(first, 9, -1, {std::numeric_limits<float>::quiet_NaN(), 0}, epoch),
        "unavailable pointer position was admitted");
}
void InputEpochs()
{
  Policy policy;
  Check(policy.Press(first, 9, -1, {0, 0}, epoch, 10), "qualified input epoch refused");
  Check(!policy.Press(first, 9, -1, {0, 0}, epoch, 10), "duplicate initialization manufactured another press");
  Check(policy.Release(first, true, true, epoch + 1ms, 11) == Click::Consume, "initial release not consumed");
  Check(!policy.Press(first, 9, -1, {0, 0}, epoch + 1ms, 11), "release frame manufactured another press");
  Check(policy.Release(first, true, true, epoch + 1ms, 11) == Click::Consume,
        "reentrant release changed pins or reached native selection");
  Check(policy.Press(first, 9, -1, {0, 0}, epoch + 20ms, 12)
            && policy.Release(first, true, true, epoch + 21ms, 13) == Click::Toggle,
        "independent input epochs no longer support normal double click");
  Check(policy.Release(first, true, true, epoch + 21ms, 13) == Click::Consume,
        "reentrant completion published a second toggle");
  Check(!policy.Press(first, 9, -1, {0, 0}, epoch + 22ms, 0), "unavailable frame identity was admitted");
}
void SameShipRebind()
{
  ObservedPolicy     policy;
  constexpr Identity same_ship_new_episode{first.tile, first.generation + 1, first.ship};
  policy.Press(first, 9, -1, {0, 0}, epoch);
  policy.Begin(9, -1, Point{8, 0}, true, epoch + 125ms);
  policy.InvalidateTile(first.tile); // Native SetWidgetData, including unchanged ship/context.
  Check(!policy.Source() && !policy.End(9, -1, true, true).commit,
        "a native rebind must revoke an in-flight drag even for the same ship");
  Check(policy.Release(same_ship_new_episode, true, true, epoch + 140ms) != Click::Toggle,
        "rebound same-ship release must not turn a stale drag into a pin toggle");
  policy.Press(same_ship_new_episode, 9, -1, {0, 0}, epoch + 200ms);
  Check(policy.Release(same_ship_new_episode, true, true, epoch + 210ms) == Click::Consume,
        "fresh same-ship binding must start a new badge pair");
}
} // namespace
int main()
{
  try {
    Clicks();
    ScrollingAndDrag();
    Cancellation();
    InputEpochs();
    SameShipRebind();
    std::cout
        << "Pin pointer policy passed: fresh badge pairs, native scroll, held drag, source and focus cancellation\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
