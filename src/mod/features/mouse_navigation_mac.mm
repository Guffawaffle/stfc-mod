// [control] macos_navigation_mouse: app-local horizontal swipes feed existing MOUSE3/MOUSE4 shortcuts.

#include "config.h"
#include "features/features.h"
#include "features/mouse_navigation.h"
#include "frame.h"
#include "shortcuts.h"

#include <spdlog/spdlog.h>
#include <unity/application.h>
#include <unity/input.h>

#include <atomic>
#include <mutex>

#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

namespace
{
struct NavigationMouseNative {
  std::mutex mutex;
  MouseNavigation::Stage stage;
  std::atomic_bool ready{false}, active{false};
  id monitor = nil, resigned = nil, activated = nil;
};

NavigationMouseNative& NavigationMouseState()
{
  // Process lifetime: never send AppKit messages from native static destructors.
  static auto& state = *new NavigationMouseNative;
  return state;
}

int32_t navigation_mouse_frame = -1; // Game thread only.

void RetireMacNavigationEvent() noexcept
{
  unity::Key::RetireNavigationMouse(); // Atomic revision only, with no Unity/managed calls on a callback thread.
  auto& state = NavigationMouseState();
  try {
    std::scoped_lock lock(state.mutex);
    state.stage.Retire();
  } catch (...) {
    state.ready.store(false, std::memory_order_release);
  }
}

NSEvent* NavigationMouseEvent(NSEvent* event)
{
  auto& state = NavigationMouseState();
  try {
    const bool active = NSApp.active;
    state.active.store(active, std::memory_order_release);
    if (!active) {
      RetireMacNavigationEvent();
      return event;
    }
    using MouseNavigation::Phase;
    const auto native_phase = event.phase;
    const double x = event.deltaX, y = event.deltaY;
    const auto phase = (!std::isfinite(x) || !std::isfinite(y) || (native_phase & NSEventPhaseCancelled)) ? Phase::kCancelled
                     : (native_phase & NSEventPhaseEnded) ? Phase::kEnded
                     : (native_phase & NSEventPhaseBegan) ? Phase::kBegan
                     : (native_phase & NSEventPhaseChanged) ? Phase::kChanged : Phase::kDiscrete;
    const auto button = MouseNavigation::Button(x, y);
    std::scoped_lock lock(state.mutex);
    return state.stage.Observe(button, phase) ? nil : event;
  } catch (...) {
    state.ready.store(false, std::memory_order_release);
    RetireMacNavigationEvent();
    return event;
  }
}

void InstallMacNavigationMonitor()
{
  auto& state = NavigationMouseState();
  try {
    state.monitor = [[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskSwipe
        handler:^NSEvent*(NSEvent* event) { return NavigationMouseEvent(event); }] retain];
    if (!state.monitor) {
      spdlog::warn("[MacNavigationMouse] could not install the app-local swipe monitor");
      return;
    }
    state.resigned = [[[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidResignActiveNotification object:NSApp queue:nil
        usingBlock:^(NSNotification*) {
          NavigationMouseState().active.store(false, std::memory_order_release);
          RetireMacNavigationEvent();
        }] retain];
    state.activated = [[[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidBecomeActiveNotification object:NSApp queue:nil
        usingBlock:^(NSNotification*) {
          NavigationMouseState().active.store(true, std::memory_order_release);
        }] retain];
    if (!state.resigned || !state.activated) {
      RetireMacNavigationEvent();
      spdlog::warn("[MacNavigationMouse] could not observe app focus changes; swipe input stays unavailable");
      return;
    }
    state.active.store(NSApp.active, std::memory_order_release);
    state.ready.store(true, std::memory_order_release);
  } catch (...) {
    state.ready.store(false, std::memory_order_release);
    RetireMacNavigationEvent();
    spdlog::warn("[MacNavigationMouse] app-local swipe monitor setup failed");
  }
}

bool InstallMacNavigationMouse()
{
  // Installation and event delivery use AppKit's main queue, independently of Unity/bootstrap's thread.
  if ([NSThread isMainThread])
    InstallMacNavigationMonitor();
  else
    dispatch_async(dispatch_get_main_queue(), ^{ InstallMacNavigationMonitor(); });
  return true; // Frame/menu availability waits for the monitor, including asynchronous failure.
}

void ResetMacNavigationMouse()
{
  unity::Key::ClearNavigationMouse();
  auto& state = NavigationMouseState();
  std::scoped_lock lock(state.mutex);
  state.stage.Retire();
  navigation_mouse_frame = -1;
}

void MacNavigationMouseFrame()
{
  auto& state = NavigationMouseState();
  const auto& config = Config::Get(); // Only the game thread touches Config, Unity or shortcut ownership.
  const auto frame = unity::Time::frameCount();
  const bool eligible = frame >= 0 && state.ready.load(std::memory_order_acquire)
                        && state.active.load(std::memory_order_acquire) && unity::Application::isFocused()
                        && config.control.macos_navigation_mouse && config.control.hotkeys_enabled
                        && !config.control.use_scopely_hotkeys && !ModSessionSuspended()
                        && !Shortcut::CaptureBlocksShortcuts();
  if (!eligible) {
    ResetMacNavigationMouse();
    return;
  }
  const auto revision = unity::Key::NavigationMouseRevision();
  std::array<bool, 2> buttons{};
  {
    std::scoped_lock lock(state.mutex);
    state.stage.Publish(true, revision);
    if (navigation_mouse_frame == frame)
      return; // Repeated native Update cannot consume/republish events in the same Unity frame.
    buttons = state.stage.Take();
  }
  navigation_mouse_frame = frame;
  // Poll already started this frame. The captured revision prevents concurrent native focus loss from turning
  // previously retired events into a fresh input overlay.
  unity::Key::PublishNavigationMouse(buttons, revision);
}
} // namespace

bool MacNavigationMouseAvailable() noexcept
{ return NavigationMouseState().ready.load(std::memory_order_acquire); }

const Feature kMacNavigationMouse{"MacNavigationMouse", &Config::Patches::hotkeyhooks, InstallMacNavigationMouse,
                                  MacNavigationMouseFrame, InstallRequirement::kOptional, ResetMacNavigationMouse};
