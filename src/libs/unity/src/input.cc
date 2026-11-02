// Owns the per-frame keyboard snapshot and text-field focus.

#include <unity/input.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace unity
{
namespace
{
  // On an idle frame anyKey is false and no key is asked about at all; otherwise each key is one internal call.
  // The optional members cache a read for the rest of the frame.
  struct Frame {
    int32_t             number   = -1;
    bool                any_held = false;
    bool                any_down = false;
    std::optional<bool> focused;
    std::optional<bool> modified;
    // Indexed [down][key].
    std::array<std::array<std::optional<bool>, std::to_underlying(KeyCode::Max)>, 2> keys{};

    bool Read(KeyCode key, bool down)
    {
      if (!(down ? any_down : any_held))
        return false;
      auto& value = keys[down][std::to_underlying(key)];
      if (!value)
        value = down ? Input::GetKeyDown(key) : Input::GetKey(key);
      return *value;
    }
  } frame;

  std::optional<bool> InputFocused(EventSystem* event_system)
  {
    if (!event_system)
      return false;
    const auto selection = event_system->TryCall<"get_currentSelectedGameObject", GameObject*>();
    if (!selection)
      return {};
    auto* selected = selection.value;
    if (!selected)
      return false;
    // EventSystem can retain a non-null managed wrapper after Unity destroys its native GameObject.
    // Only confirmed destruction skips the lookup; an unavailable liveness read preserves the existing path.
    const auto alive = TryAlive(selected);
    if (alive && !alive.value)
      return false;
    auto* type = TMP_InputField::Class::TypeObject();
    if (!type)
      return {};
    const auto field = selected->TryCall<"GetComponent", TMP_InputField*>(type);
    if (!field)
      return {};
    if (!field.value)
      return false;
    const auto focused = field.value->TryCall<"get_isFocused", bool>();
    return focused ? std::optional{focused.value} : std::nullopt;
  }
} // namespace

namespace Key
{
  void Poll()
  {
    // Without the frame count nothing is ever read: no key counts as pressed.
    const int32_t number = Time::frameCount();
    if (number < 0 || number == frame.number)
      return;
    frame = {.number = number, .any_held = Input::anyKey(), .any_down = Input::anyKeyDown()};
  }

  bool Pressed(KeyCode key)
  { return frame.Read(key, false); }

  bool Down(KeyCode key)
  { return frame.Read(key, true); }

  bool IsModified()
  {
    constexpr std::array kModifiers{KeyCode::LeftAlt,      KeyCode::LeftControl,  KeyCode::LeftShift,
                                    KeyCode::RightAlt,     KeyCode::RightControl, KeyCode::RightShift,
                                    KeyCode::AltGr,        KeyCode::LeftCommand,  KeyCode::LeftWindows,
                                    KeyCode::RightCommand, KeyCode::RightWindows};
    if (!frame.modified)
      frame.modified = frame.any_held && std::ranges::any_of(kModifiers, Pressed);
    return *frame.modified;
  }

  bool IsInputFocused()
  {
    if (!frame.focused) {
      const auto event_system = EventSystem::Class::TryCall<"get_current", EventSystem*>();
      // An unavailable observation cannot release typed keys to gameplay shortcuts.
      // Reserve this frame for native input and retry on the next frame.
      frame.focused = !event_system || InputFocused(event_system.value).value_or(true);
    }
    return *frame.focused;
  }

  void ClearInputFocus()
  {
    const auto event_system = EventSystem::Class::TryCall<"get_current", EventSystem*>();
    if (event_system && InputFocused(event_system.value).value_or(false))
      event_system.value->SetSelectedGameObject(nullptr);
    // Selection callbacks can focus another field. Re-read the actual focus on the next query this frame.
    frame.focused.reset();
  }
} // namespace Key
} // namespace unity
