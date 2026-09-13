#pragma once
#include <functional>
#include <string>

namespace mod_settings
{
// A command with a current presentation, not a persisted boolean. The native
// adapter owns each bound view; these definitions live with the page catalog.
struct ActionSetting {
  struct Presentation {
    std::string label, button, value;
    bool        enabled = true;
  };
  std::string                   identity, label;
  std::function<Presentation()> read;
  std::function<void()>         invoke;
  std::function<void()>         hidden;
  const std::string&            id() const
  { return identity; }
};
} // namespace mod_settings
