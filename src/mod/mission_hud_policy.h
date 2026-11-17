#pragma once

// Mission HUD restoration and refresh decisions, independent of Unity objects.
// The native owner resets Restoration when its weak button identity changes.
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace mission_hud
{
class Restoration
{
public:
  std::optional<bool> Prepare(std::optional<bool> forced, std::optional<bool> observed)
  {
    if (!observed)
      return {};
    // Capture before attempting a write. A setter can mutate and then fail, so
    // even an unconfirmed override owes restoration to this native baseline.
    if (forced && !baseline_)
      baseline_ = observed;
    return forced ? forced : baseline_;
  }
  void Confirm(std::optional<bool> forced, std::optional<bool> observed)
  {
    if (!forced && baseline_ && observed && *observed == *baseline_)
      baseline_.reset();
  }
  bool Pending() const
  { return baseline_.has_value(); }
  void Reset()
  { baseline_.reset(); }

private:
  std::optional<bool> baseline_;
};

// Explicit readback owns confirmation; neither setter success nor failure says
// whether the native object actually changed. Observations include identity and
// liveness checks supplied by the native adapter.
template <class Button> void ApplyVisibility(Restoration& state, std::optional<bool> forced, Button& button)
{
  const auto observed = button.Observe();
  const auto desired  = state.Prepare(forced, observed);
  if (!desired)
    return;
  if (*observed != *desired)
    (void)button.Write(*desired);
  state.Confirm(forced, button.Observe());
}

class ReentryGuard
{
public:
  explicit ReentryGuard(bool& active)
      : active_(active)
      , entered_(!active)
  {
    if (entered_)
      active_ = true;
  }
  ~ReentryGuard()
  {
    if (entered_)
      active_ = false;
  }
  explicit operator bool() const
  { return entered_; }
  ReentryGuard(const ReentryGuard&) = delete;

private:
  bool& active_;
  bool  entered_;
};

// Native callbacks can request another HUD application while an owner is being
// applied or refreshed. Keep only weak identities, and drain one owned batch so
// work requested by that batch waits for a subsequent native event/frame.
template <class Handle> class DeferredOwners
{
public:
  using Object = decltype(std::declval<const Handle&>().Get());

  void Remember(Object owner)
  {
    if (!owner || std::ranges::any_of(pending_, [&](const auto& weak) { return weak.Get() == owner; }))
      return;
    if (auto weak = Handle::Weak(owner))
      pending_.push_back(std::move(weak));
  }
  bool Draining() const
  { return draining_; }
  template <class Apply> void Drain(bool blocked, Apply&& apply)
  {
    if (blocked || draining_ || pending_.empty())
      return;
    ReentryGuard draining(draining_);
    auto         batch = std::move(pending_);
    pending_.clear();
    for (const auto& weak : batch)
      if (auto* owner = weak.Get())
        apply(owner);
  }

private:
  std::vector<Handle> pending_;
  bool                draining_ = false;
};

// The owner holds a strong snapshot, but Unity lifetime, enabled state and weak
// identities must still be observed after each native callback.
template <class Controller> void RefreshController(Controller& controller)
{
  if (!controller.Ready() || !controller.Achievements() || !controller.Ready() || !controller.Outposts()
      || !controller.Ready())
    return;
  controller.Apply();
}
} // namespace mission_hud
