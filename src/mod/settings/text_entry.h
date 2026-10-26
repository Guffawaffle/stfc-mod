#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace mod_settings
{
// The owning setting captures its observed revision in commit. Validation has
// no side effects; a rejected/stale commit keeps the editor open for correction.
struct TextEntryRequest {
  std::string                                  title, value, help;
  std::function<std::string(std::string_view)> validate;
  std::function<bool(std::string_view)>        commit;
  std::function<void()>                        cancel;
  bool                                         read_only   = false;
  std::string                                  apply_label = "Apply";
  // A read-only recording popup samples raw input; Return is a candidate, never implicit Apply.
  std::function<std::optional<std::string>()> record_frame;
  std::function<void()>                       record_again;
};
#if defined(STFC_SETTINGS_TEXT_ENTRY_FIXTURE) || (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
bool OpenTextEntry(TextEntryRequest request);
void CloseTextEntry();
bool TextEntryActive();
#else
inline bool OpenTextEntry(TextEntryRequest)
{ return false; }
inline void CloseTextEntry() {}
inline bool TextEntryActive()
{ return false; }
#endif
} // namespace mod_settings
