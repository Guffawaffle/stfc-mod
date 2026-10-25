#include "keyboard_layout.h"
#include "keyboard_layout_refresh.h"
#include "keyboard_layout_windows.h"

#include <spdlog/spdlog.h>
#include <unity/keyboard_layout.h>

#include <array>
#include <string>

namespace keyboard_layout
{
namespace
{
  bool                             enabled = false, initialized = false, failed = false;
  RefreshState                     refresh;
  BindingState                     bindings;
  std::array<bool, LayoutKeyCount> requested{}, required_shift{};
  std::string                      layout_name, status = "physical", reason = "configured_physical";
  unsigned                         generation = 0;

  void Publish(std::string next, std::string why)
  {
    if (status == next && reason == why)
      return;
    status = std::move(next);
    reason = std::move(why);
    spdlog::info("[KeyboardLayout] status={} layout='{}' generation={} reason={}", status, layout_name, ++generation,
                 reason);
  }
  void Unavailable(const char* why)
  {
    bindings.Clear();
    required_shift = {};
    layout_name.clear();
    Publish("unavailable", why);
  }
  void Invalidate() noexcept
  { refresh.Invalidate(); }
  void Initialize()
  {
    initialized             = true;
    const auto subscription = unity::keyboard_layout::Start(&Invalidate);
    if (subscription != unity::keyboard_layout::ObserverResult::Started || !unity::Time::FrameCountAvailable()) {
      failed = true;
      unity::keyboard_layout::Stop();
      Unavailable(subscription == unity::keyboard_layout::ObserverResult::Unsupported
                      ? "platform_unsupported"
                      : "notification_or_frame_api_unavailable");
    }
  }
  void Update()
  {
    if (!initialized)
      Initialize();
    if (failed || !refresh.Consume())
      return;
    std::string observed;
    if (!unity::keyboard_layout::Observe(observed)) {
      Unavailable("keyboard_or_layout_absent");
      return;
    }
    layout_name = std::move(observed);
    LayoutKeys                       keys{};
    std::array<bool, LayoutKeyCount> shift{};
    bool                             complete = true;
    for (std::size_t index = 0; index < requested.size(); ++index) {
      if (!requested[index])
        continue;
#if _WIN32
      const auto chord = ResolveWindowsChord(static_cast<char>(index), layout_name);
      keys[index]      = chord.key;
      shift[index]     = chord.shift;
#endif
      if (keys[index] == KeyCode::None) {
        complete = false;
        spdlog::warn("[KeyboardLayout] unresolved character '{}' ({}) on layout '{}'; binding disabled",
                     static_cast<char>(index), index, layout_name);
      }
    }
    const auto frame = unity::Time::frameCount();
    if (frame < 0) {
      Unavailable("frame_unavailable");
      return;
    }
    bindings.Replace(keys, unity::Key::Pressed, frame);
    required_shift = shift;
    Publish(complete ? "resolved" : "partial", complete ? "layout_lookup" : "unresolved_keys_disabled");
  }
} // namespace

void Configure(std::string_view mode)
{
  const bool next = mode == "layout";
  if (enabled == next)
    return;
  enabled = next;
  bindings.Clear();
  required_shift = {};
  refresh.Invalidate();
  if (!enabled)
    Publish("physical", "configured_physical");
  else if (!failed)
    Publish("pending", "awaiting_game_input");
  else
    Publish("unavailable", "layout_support_failed_until_restart");
}
void RegisterShortcut(KeyCode key) noexcept
{
  if (enabled && IsLayoutKey(key) && !requested[static_cast<int>(key)]) {
    requested[static_cast<int>(key)] = true;
    refresh.Invalidate();
  }
}
ResolvedChord ResolveChord(KeyCode configured)
{
  if (!enabled || !IsLayoutKey(configured))
    return {configured, false};
  Update();
  return {bindings.Resolve(configured, unity::Time::frameCount, unity::Key::Pressed),
          required_shift[static_cast<int>(configured)]};
}
ResolvedChord DescribeChord(KeyCode configured)
{
  if (!enabled || !IsLayoutKey(configured))
    return {configured, false};
  Update();
#if _WIN32
  return ResolveWindowsChord(static_cast<char>(configured), layout_name);
#else
  return {};
#endif
}
KeyCode CaptureIdentity(KeyCode physical, bool shift)
{
  if (!enabled || !IsLayoutKey(physical))
    return physical;
  Update();
  for (bool inferred : {false, true}) {
    if (inferred && !shift)
      break;
    auto found = KeyCode::None;
    for (int index = 0; index < static_cast<int>(LayoutKeyCount); ++index) {
      const auto key = static_cast<KeyCode>(index);
      if (!IsLayoutKey(key))
        continue;
      const auto chord = DescribeChord(key);
      if (chord.key == physical && chord.shift == inferred) {
        if (found != KeyCode::None)
          return KeyCode::None;
        found = key;
      }
    }
    if (found != KeyCode::None)
      return found;
  }
  return KeyCode::None;
}
} // namespace keyboard_layout
