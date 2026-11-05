#pragma once

// Process-lifetime, game-thread-only subscribers to the host's single frame tick.
// Registration does not install a hook or establish that frames can be delivered.

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>
#include <vector>

class FrameCallbacks
{
public:
  using Callback = void (*)();
  using Reporter = void (*)(const char* name, std::exception_ptr error);

  explicit FrameCallbacks(Reporter reporter = nullptr)
      : reporter_(reporter)
  {
  }
  FrameCallbacks(const FrameCallbacks&)            = delete;
  FrameCallbacks& operator=(const FrameCallbacks&) = delete;
  FrameCallbacks(FrameCallbacks&&)                 = delete;
  FrameCallbacks& operator=(FrameCallbacks&&)      = delete;

  // Registered subscribers, independent of whether the native frame hook exists.
  std::size_t RegistrationCount() const noexcept
  { return entries_.size(); }

  // The first registration owns its diagnostic name. Registering the same
  // callback again is harmless, except during dispatch when every mutation
  // request is rejected. Allocation can occur here, never to snapshot a frame.
  bool Register(Callback callback, const char* name)
  {
    if (!callback || dispatching_)
      return false;
    if (std::any_of(entries_.begin(), entries_.end(),
                    [callback](const Entry& entry) { return entry.callback == callback; }))
      return true;
    entries_.push_back({callback, name && *name ? name : "Unnamed frame callback"});
    return true;
  }

  // A recursive dispatch is suppressed. One callback or reporter throwing must
  // not skip later subscribers or strand the registry in its dispatching state.
  void Dispatch() noexcept
  {
    if (dispatching_)
      return;
    struct Guard {
      bool& active;
      explicit Guard(bool& value)
          : active(value)
      { active = true; }
      ~Guard()
      { active = false; }
    } guard(dispatching_);

    // Registration is rejected while this range is borrowed, so no snapshot or
    // per-frame collection allocation is necessary.
    for (const auto& entry : entries_) {
      try {
        entry.callback();
      } catch (...) {
        if (reporter_) {
          try {
            reporter_(entry.name.c_str(), std::current_exception());
          } catch (...) {
            // Reporting cannot unwind into the host's frame or stop dispatch.
          }
        }
      }
    }
  }

private:
  struct Entry {
    Callback    callback;
    std::string name;
  };
  std::vector<Entry> entries_;
  Reporter           reporter_;
  bool               dispatching_ = false;
};
