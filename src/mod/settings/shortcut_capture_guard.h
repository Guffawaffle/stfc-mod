#pragma once

#include "input_release.h"
#include <cstdint>

namespace mod_settings
{
// UI-thread ownership for a shortcut editor, independent of the binding matcher. Shortcut::Owns observes keys
// already used by matched modified chords; capture must block dispatch before any shortcut is matched.
// Keep this owner alive beyond a pooled editor. Acceptance, cancellation and page departure all Release its token.
class ShortcutCaptureGuard
{
public:
  using Token = std::uint64_t;

  Token Claim()
  {
    if (++serial_ == 0)
      ++serial_;
    owner_ = serial_;
    ready_ = false;
    release_.Hold();
    frame_blocked_ = true;
    return owner_;
  }
  bool Owns(Token token) const
  { return token != 0 && token == owner_; }
  bool Ready(Token token) const
  { return Owns(token) && ready_; }
  bool Release(Token token)
  {
    if (!Owns(token))
      return false; // A stale pooled-row cancel cannot release the replacement editor.
    owner_ = 0;
    ready_ = false;
    release_.Hold();
    frame_blocked_ = true;
    return true;
  }

  // Call once per input frame, before shortcut dispatch. `focused` is application focus; `held` covers every
  // physical capture key, including modifiers and popup Enter/Escape, rather than only configured bindings.
  // The editor reads raw input after this call. Do not skip native UI processing merely because shortcuts block.
  bool BeforeDispatch(bool focused, bool held)
  {
    frame_blocked_ = owner_ != 0 || release_.active();
    if (!frame_blocked_)
      return false;
    if (owner_ != 0) {
      if (!focused) {
        ready_ = false;
        release_.Hold();
      } else if (!ready_) {
        ready_ = release_.Tick(true, held);
      }
    } else {
      release_.Tick(focused, held);
    }
    // Keep the final input-free frame owned too. A later frame gives gameplay a fresh input boundary.
    return true;
  }
  bool BlocksShortcuts() const
  { return frame_blocked_ || owner_ != 0 || release_.active(); }

private:
  InputRelease release_;
  Token        serial_ = 0, owner_ = 0;
  bool         ready_ = false, frame_blocked_ = false;
};
} // namespace mod_settings
