#pragma once

#include <atomic>

namespace keyboard_layout
{
// Shared with the notification callback; all other mapping state stays on the game thread.
class RefreshState
{
public:
  struct Tick {
    bool new_frame;
    bool invalidated;
    bool CheckKeyboard() const
    { return new_frame || invalidated; }
  };

  void Invalidate() noexcept
  { dirty_.store(true); }

  Tick Begin(int frame)
  {
    const Tick tick{frame != last_frame_, dirty_.exchange(false)};
    last_frame_ = frame;
    return tick;
  }

private:
  std::atomic<bool> dirty_{true};
  int               last_frame_ = -1;
};
} // namespace keyboard_layout
