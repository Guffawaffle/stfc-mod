#pragma once

// Pin-specific mouse routing, independent of Unity pointers/geometry. The native
// adapter supplies a live weak-root identity and checked badge hit observations.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

namespace ship_pin_pointer
{
using Clock                                = std::chrono::steady_clock;
inline constexpr auto  HoldTime            = std::chrono::milliseconds(125);
inline constexpr auto  DoubleClickWindow   = std::chrono::milliseconds(400);
inline constexpr auto  LostEndGrace        = std::chrono::milliseconds(250);
inline constexpr float DragDistanceSquared = 64.0f;
struct Point {
  float x, y;
};
struct Identity {
  std::uintptr_t tile                                         = 0;
  std::uint64_t  generation                                   = 0;
  std::int64_t   ship                                         = 0;
  friend bool    operator==(const Identity&, const Identity&) = default;
  explicit       operator bool() const noexcept
  { return tile && generation && ship > 0; }
};
enum class Route { Native, Consume };
enum class Click { Native, Consume, Toggle };

class Policy
{
public:
  void Reset() noexcept
  {
    const auto press = last_press_epoch_, release = last_release_epoch_;
    *this               = Policy{};
    last_press_epoch_   = press;
    last_release_epoch_ = release;
  }
  void ForgetClick() noexcept
  { last_click_.reset(); }
  void InvalidateTile(std::uintptr_t tile) noexcept
  {
    if (last_click_ && last_click_->identity.tile == tile)
      last_click_.reset();
    if (pressed_badge_.tile == tile)
      pressed_badge_ = {};
    if (source_.tile == tile)
      Cancel();
  }
  bool Press(Identity identity, std::uintptr_t scroll, int pointer, Point position, Clock::time_point now,
             uint64_t input_epoch) noexcept
  {
    // Down/Up are frame-wide. Reentrant dispatch is not another physical press.
    if (!input_epoch || input_epoch <= last_press_epoch_ || input_epoch <= last_release_epoch_)
      return false;
    last_press_epoch_ = input_epoch;
    ClearDrag();
    suppress_ship_ = 0;
    pressed_badge_ = {};
    if (!identity || !scroll || pointer != -1 || !Finite(position)) {
      last_click_.reset();
      return false;
    }
    if (last_click_ && last_click_->identity != identity)
      last_click_.reset();
    source_ = pressed_badge_ = identity;
    scroll_                  = scroll;
    pointer_                 = pointer;
    origin_                  = position;
    pressed_                 = now;
    return true;
  }
  bool Matches(std::uintptr_t scroll, int pointer) const noexcept
  { return source_ && scroll_ == scroll && pointer_ == pointer; }
  Identity Source() const noexcept
  { return source_; }
  bool Dragging() const noexcept
  { return dragging_; }
  bool Active() const noexcept
  { return source_ || last_click_.has_value(); }
  Identity LastClickIdentity() const noexcept
  { return last_click_ ? last_click_->identity : Identity{}; }
  bool Armed(Clock::time_point now) const noexcept
  { return source_ && now >= pressed_ && now - pressed_ >= HoldTime; }

  Route Begin(std::uintptr_t scroll, int pointer, std::optional<Point> position, bool source_live,
              Clock::time_point now) noexcept
  {
    if (!Matches(scroll, pointer))
      return Route::Native;
    last_click_.reset();
    if (!source_live || !position || !Finite(*position) || !Armed(now)) {
      Cancel();
      return Route::Native;
    }
    captured_      = true;
    suppress_ship_ = source_.ship;
    pressed_badge_ = {};
    dragging_      = Distance(*position) >= DragDistanceSquared;
    return Route::Consume;
  }
  Route Drag(std::uintptr_t scroll, int pointer, std::optional<Point> position, bool source_live) noexcept
  {
    if (!Matches(scroll, pointer) || !captured_)
      return Route::Native;
    if (!source_live || !position || !Finite(*position)) {
      Cancel();
      return Route::Consume;
    }
    dragging_ |= Distance(*position) >= DragDistanceSquared;
    return Route::Consume;
  }
  // The adapter resolves the source before End and performs an action only if
  // commit is true and the independently checked drop geometry permits it.
  struct EndResult {
    Route route;
    bool  commit;
  };
  EndResult End(std::uintptr_t scroll, int pointer, bool source_live, bool position_known) noexcept
  {
    if (!Matches(scroll, pointer) || !captured_)
      return {Route::Native, false};
    const bool commit = dragging_ && source_live && position_known;
    Cancel();
    return {Route::Consume, commit};
  }
  void Cancel() noexcept
  {
    if (source_)
      suppress_ship_ = source_.ship;
    pressed_badge_ = {};
    last_click_.reset();
    ClearDrag();
  }
  Click Release(Identity identity, bool fresh_mouse_release, bool inside_badge, Clock::time_point now,
                uint64_t input_epoch) noexcept
  {
    if (!identity || !fresh_mouse_release || !input_epoch || input_epoch < last_press_epoch_) {
      last_click_.reset();
      return Click::Native;
    }
    if (input_epoch <= last_release_epoch_)
      return Click::Consume;
    if (suppress_ship_ == identity.ship || dragging_) {
      suppress_ship_ = 0;
      pressed_badge_ = {};
      last_click_.reset();
      ClearDrag();
      last_release_epoch_ = input_epoch;
      return Click::Consume;
    }
    if (pressed_badge_ != identity || !inside_badge) {
      pressed_badge_ = {};
      last_click_.reset();
      return Click::Native;
    }
    pressed_badge_ = {};
    ClearDrag();
    const bool pair = last_click_ && last_click_->identity == identity && now >= last_click_->at
                      && now - last_click_->at <= DoubleClickWindow;
    last_click_.reset();
    last_release_epoch_ = input_epoch;
    if (pair)
      return Click::Toggle;
    last_click_ = ClickState{identity, now};
    return Click::Consume;
  }
  void Tick(bool focused, bool source_live, bool held, Clock::time_point now) noexcept
  {
    if (!focused) {
      Cancel();
      return;
    }
    if (last_click_ && (now < last_click_->at || now - last_click_->at > DoubleClickWindow))
      last_click_.reset();
    if (!source_)
      return;
    if (!source_live) {
      Cancel();
      return;
    }
    if (!dragging_ && !held) {
      // Preserve only the press identity until a same-frame mouse release click.
      ClearDrag();
    } else if (dragging_) {
      if (held)
        released_.reset();
      else if (!released_)
        released_ = now;
      else if (now >= *released_ && now - *released_ >= LostEndGrace)
        Cancel();
    }
  }

private:
  static bool Finite(Point point) noexcept
  { return std::isfinite(point.x) && std::isfinite(point.y); }
  float Distance(Point point) const noexcept
  {
    const float x = point.x - origin_.x, y = point.y - origin_.y;
    return x * x + y * y;
  }
  void ClearDrag() noexcept
  {
    source_   = {};
    scroll_   = 0;
    pointer_  = 0;
    captured_ = dragging_ = false;
    released_.reset();
  }
  struct ClickState {
    Identity          identity;
    Clock::time_point at;
  };
  Identity                         source_{}, pressed_badge_{};
  std::uintptr_t                   scroll_  = 0;
  int                              pointer_ = 0;
  Point                            origin_{};
  Clock::time_point                pressed_{};
  std::optional<Clock::time_point> released_;
  std::optional<ClickState>        last_click_;
  std::int64_t                     suppress_ship_    = 0;
  uint64_t                         last_press_epoch_ = 0, last_release_epoch_ = 0;
  bool                             captured_ = false, dragging_ = false;
};
} // namespace ship_pin_pointer
