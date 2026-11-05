#include "mission_hud_policy.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct NativeFailure {
};
void Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
struct Button {
  bool                  active = false, readable = true, mutate = true, success = true, throw_after_write = false;
  bool                  lose_readback = false;
  int                   writes        = 0;
  std::function<void()> during_write;
  std::optional<bool>   Observe()
  { return readable ? std::optional{active} : std::nullopt; }
  bool Write(bool value)
  {
    ++writes;
    if (mutate)
      active = value;
    if (during_write)
      during_write();
    if (lose_readback)
      readable = false;
    if (throw_after_write)
      throw NativeFailure{};
    return success;
  }
};
void RestorationTransitions()
{
  for (bool native : {false, true}) {
    for (bool forced : {false, true}) {
      mission_hud::Restoration state;
      Button                   button{.active = native};
      mission_hud::ApplyVisibility(state, forced, button);
      Check(button.active == forced && state.Pending(), "explicit visibility lost its native baseline");
      mission_hud::ApplyVisibility(state, !forced, button);
      Check(button.active == !forced && state.Pending(), "Always/Never transition rebased over an override");
      mission_hud::ApplyVisibility(state, {}, button);
      Check(button.active == native && !state.Pending(), "Auto did not restore the original observed value");
      const auto writes = button.writes;
      mission_hud::ApplyVisibility(state, {}, button);
      Check(button.writes == writes, "Auto with no override wrote a guessed native value");
    }
  }
}
void UnavailableAndFailedWrites()
{
  mission_hud::Restoration state;
  Button                   button{.readable = false};
  mission_hud::ApplyVisibility(state, true, button);
  Check(!state.Pending() && button.writes == 0, "unavailable state became an observed false baseline");
  button.readable     = true;
  button.success      = false;
  button.during_write = [&] { Check(state.Pending(), "mutation happened before restoration was retained"); };
  mission_hud::ApplyVisibility(state, true, button);
  Check(button.active && state.Pending(), "a failed but mutating setter lost its restoration obligation");
  button.during_write = {};
  button.mutate       = false;
  button.success      = true;
  mission_hud::ApplyVisibility(state, {}, button);
  Check(button.active && state.Pending(), "setter success alone acknowledged restoration");
  button.mutate        = true;
  button.success       = false;
  button.lose_readback = true;
  mission_hud::ApplyVisibility(state, {}, button);
  Check(!button.active && state.Pending(), "missing readback acknowledged restoration");
  const auto writes = button.writes;
  mission_hud::ApplyVisibility(state, {}, button);
  Check(button.writes == writes && state.Pending(), "unknown observation authorized another native mutation");
  button.readable      = true;
  button.lose_readback = false;
  mission_hud::ApplyVisibility(state, {}, button);
  Check(!state.Pending(), "matching readback could not confirm an already restored button");

  button.active            = false;
  button.throw_after_write = true;
  try {
    mission_hud::ApplyVisibility(state, true, button);
    Check(false, "throwing setter did not exercise the owned failure boundary");
  } catch (const NativeFailure&) {
  }
  Check(button.active && state.Pending(), "a throw after mutation discarded the baseline");
  button.throw_after_write = false;
  mission_hud::ApplyVisibility(state, {}, button);
  Check(!button.active && !state.Pending(), "throwing override could not later restore safely");

  mission_hud::ApplyVisibility(state, true, button);
  state.Reset(); // Native adapter observed replacement weak button identity.
  Button replacement{.active = true};
  mission_hud::ApplyVisibility(state, {}, replacement);
  Check(replacement.active && replacement.writes == 0, "old button baseline was applied to its replacement");
  mission_hud::ApplyVisibility(state, false, replacement);
  mission_hud::ApplyVisibility(state, {}, replacement);
  Check(replacement.active && !state.Pending(), "replacement did not capture its own native baseline");
}

struct Controller {
  bool alive = true, enabled = true;
  int  button = 1;
};

template <class Owner> struct WeakOwner {
  std::weak_ptr<Owner> weak;
  static WeakOwner     Weak(Owner* owner)
  { return {owner->weak_from_this()}; }
  Owner* Get() const
  { return weak.lock().get(); }
  explicit operator bool() const
  { return !weak.expired(); }
};
struct DeferredOwner : std::enable_shared_from_this<DeferredOwner> {
};
void DeferredBatches()
{
  mission_hud::DeferredOwners<WeakOwner<DeferredOwner>> deferred;
  auto                                                  owner  = std::make_shared<DeferredOwner>();
  int                                                   calls  = 0;
  auto                                                  repeat = [&](DeferredOwner* current) {
    ++calls;
    Check(deferred.Draining(), "batch callback did not retain drain ownership");
    deferred.Remember(current);
    deferred.Drain(false, [&](auto*) { Check(false, "nested drain recursively applied a queued owner"); });
  };
  deferred.Remember(owner.get());
  deferred.Remember(owner.get());
  deferred.Drain(true, repeat);
  Check(calls == 0, "blocked drain applied during an active refresh/application");
  deferred.Drain(false, repeat);
  Check(calls == 1 && !deferred.Draining(), "duplicate owner or reentry expanded one owned batch");
  deferred.Drain(false, repeat);
  Check(calls == 2, "work requested by a batch did not wait for its next drain");
  std::weak_ptr<DeferredOwner> retired = owner;
  owner.reset();
  Check(retired.expired(), "deferred queue retained a strong owner");
  deferred.Drain(false, repeat);
  Check(calls == 2, "collected weak owner reached native application");

  owner = std::make_shared<DeferredOwner>();
  deferred.Remember(owner.get());
  try {
    deferred.Drain(false, [&](auto* current) {
      deferred.Remember(current);
      throw NativeFailure{};
    });
    Check(false, "deferred failure fixture did not throw");
  } catch (const NativeFailure&) {
  }
  Check(!deferred.Draining(), "throwing batch left drain ownership permanently active");
  deferred.Drain(false, [&](auto*) { ++calls; });
  Check(calls == 3, "throwing batch discarded newly deferred work");
}

// Models the native identity adapter around the shared policy/batch algorithms.
// Unity liveness, GcHandle resolution and actual fields still require a native
// qualification; this does not claim to run the native HUD registry.
struct ApplyOwner : std::enable_shared_from_this<ApplyOwner> {
  mission_hud::DeferredOwners<WeakOwner<ApplyOwner>>& deferred;
  bool&                                               applying;
  std::shared_ptr<Button>                             current = std::make_shared<Button>();
  std::weak_ptr<Button>                               tracked;
  mission_hud::Restoration                            restoration;
  std::optional<bool>                                 forced       = true;
  int                                                 applications = 0;

  ApplyOwner(mission_hud::DeferredOwners<WeakOwner<ApplyOwner>>& queue, bool& active)
      : deferred(queue)
      , applying(active)
  {
  }
  struct BoundButton {
    ApplyOwner&             owner;
    std::shared_ptr<Button> held;
    std::optional<bool>     Observe()
    {
      if (owner.current != held || owner.tracked.lock() != held) {
        owner.deferred.Remember(&owner);
        return {};
      }
      return held->Observe();
    }
    bool Write(bool value)
    { return Observe() && held->Write(value); }
  };
  void Apply()
  {
    {
      mission_hud::ReentryGuard guard(applying);
      if (!guard) {
        deferred.Remember(this);
        return;
      }
      auto owner = shared_from_this();
      auto held  = current;
      ++applications;
      if (tracked.lock() != held) {
        tracked = held;
        restoration.Reset();
      }
      BoundButton button{*this, std::move(held)};
      mission_hud::ApplyVisibility(restoration, forced, button);
    }
    deferred.Drain(applying, [](auto* owner) { owner->Apply(); });
  }
};
void NestedReplacement()
{
  mission_hud::DeferredOwners<WeakOwner<ApplyOwner>> deferred;
  bool                                               applying    = false;
  auto                                               owner       = std::make_shared<ApplyOwner>(deferred, applying);
  auto                                               old_button  = owner->current;
  auto                                               replacement = std::make_shared<Button>();
  replacement->active      = true; // Its baseline differs from the retired button.
  old_button->during_write = [&] {
    Check(owner->restoration.Pending(), "replacement callback ran before the old restoration was marked");
    owner->current = replacement;
    owner->Apply();
    Check(owner->applications == 1, "nested Apply mutated the registry during the outer application");
  };
  owner->Apply();
  Check(owner->applications == 2 && owner->tracked.lock() == replacement && !applying,
        "nested replacement did not rebind through one deferred application");
  Check(old_button->active && replacement->active && replacement->writes == 0 && owner->restoration.Pending(),
        "replacement inherited a write or acknowledgement from its retired button");
  owner->forced.reset();
  owner->Apply();
  Check(replacement->active && replacement->writes == 0 && !owner->restoration.Pending(),
        "Auto restored the retired button's baseline onto its replacement");
}

// This fixture drives the production refresh sequence. Native liveness/weak
// identity observations are supplied here; it is not a Unity hook fixture.
struct RefreshSource {
  std::shared_ptr<Controller> held;
  std::weak_ptr<Controller>   registered;
  int                         expected_button = 1;
  bool&                       refreshing;
  std::vector<std::string>    calls;
  std::function<void()>       on_achievements, on_outposts;
  bool                        achievements_ok = true, outposts_ok = true;
  bool                        Ready()
  {
    calls.push_back("ready");
    return held && held->alive && held->enabled && registered.lock() == held && held->button == expected_button;
  }
  bool Achievements()
  {
    calls.push_back("achievements");
    if (on_achievements)
      on_achievements();
    return achievements_ok;
  }
  bool Outposts()
  {
    calls.push_back("outposts");
    if (on_outposts)
      on_outposts();
    return outposts_ok;
  }
  void Apply()
  { calls.push_back("apply"); }
  void Run()
  {
    mission_hud::ReentryGuard guard(refreshing);
    if (guard)
      mission_hud::RefreshController(*this);
  }
};
void RefreshLifecycle()
{
  bool          refreshing = false;
  auto          owner      = std::make_shared<Controller>();
  RefreshSource normal{owner, owner, 1, refreshing};
  normal.on_achievements = [&] { normal.Run(); };
  normal.Run();
  Check(!refreshing
            && normal.calls == std::vector<std::string>{"ready", "achievements", "ready", "outposts", "ready", "apply"},
        "reentrant refresh changed native setup order or applied twice");
  for (int scenario = 0; scenario < 5; ++scenario) {
    owner = std::make_shared<Controller>();
    RefreshSource               changing{owner, owner, 1, refreshing};
    std::shared_ptr<Controller> replacement;
    changing.on_achievements = [&] {
      if (scenario == 0)
        owner->alive = false;
      if (scenario == 1)
        owner->enabled = false;
      if (scenario == 2) {
        replacement         = std::make_shared<Controller>();
        changing.registered = replacement;
      }
      if (scenario == 3)
        owner->button = 2;
      if (scenario == 4)
        changing.achievements_ok = false;
    };
    changing.Run();
    Check(changing.calls
              == (scenario == 4 ? std::vector<std::string>{"ready", "achievements"}
                                : std::vector<std::string>{"ready", "achievements", "ready"}),
          "lost/disabled/replaced controller or failed native refresh reached later setup/application");
  }
  owner = std::make_shared<Controller>();
  RefreshSource disabled_late{owner, owner, 1, refreshing};
  disabled_late.on_outposts = [&] { owner->alive = false; };
  disabled_late.Run();
  Check(disabled_late.calls == std::vector<std::string>{"ready", "achievements", "ready", "outposts", "ready"},
        "native lifetime lost in the second callback reached final application");
  owner                             = std::make_shared<Controller>();
  std::weak_ptr<Controller> retired = owner;
  owner.reset();
  RefreshSource collected{retired.lock(), retired, 1, refreshing};
  collected.Run();
  Check(collected.calls == std::vector<std::string>{"ready"}, "collected weak owner was refreshed");
  owner = std::make_shared<Controller>();
  RefreshSource snapshot{owner, owner, 1, refreshing};
  owner.reset();
  snapshot.Run();
  Check(snapshot.calls.back() == "apply" && !snapshot.registered.expired(),
        "strong refresh snapshot did not retain its managed owner");
}
void ReentryFailure()
{
  bool active = false;
  try {
    mission_hud::ReentryGuard outer(active);
    Check(static_cast<bool>(outer) && active, "outer operation did not own its guard");
    {
      mission_hud::ReentryGuard nested(active);
      Check(!nested && active, "nested operation stole the active guard");
    }
    Check(active, "nested guard released its caller's operation");
    throw NativeFailure{};
  } catch (const NativeFailure&) {
  }
  Check(!active, "throwing refresh left reentry disabled permanently");
}
} // namespace
int main()
{
  try {
    RestorationTransitions();
    UnavailableAndFailedWrites();
    DeferredBatches();
    NestedReplacement();
    RefreshLifecycle();
    ReentryFailure();
    std::cout << "HUD visibility policy and owned refresh sequence passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
