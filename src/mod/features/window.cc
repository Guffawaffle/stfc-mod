// The game window on Windows: [graphics] free_resize, the -ccm profile in the title bar, [graphics] allow_cursor, and
// the monitor's DPI scale.

#include "features/features.h"

#if _WIN32
#include "config.h"
#include "file.h"

#include "game/app.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <unity/core.h>
#include <unity/input.h>

#include <chrono>
#include <string>

#include <Windows.h>

namespace
{
// The first visible, unowned top-level window of this process. Found once it exists, then cached.
HWND GameWindow()
{
  static HWND window = nullptr;
  if (window && IsWindow(window))
    return window;
  window = nullptr;

  const auto process = GetCurrentProcessId();
  for (auto hwnd = GetTopWindow(nullptr); hwnd; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) {
    DWORD owner_process = 0;
    GetWindowThreadProcessId(hwnd, &owner_process);
    if (owner_process == process && !GetWindow(hwnd, GW_OWNER) && IsWindowVisible(hwnd)) {
      window = hwnd;
      break;
    }
  }
  return window;
}

// Keep one reversible window-owned change. No system-wide WndProc detour is
// needed: the shared game-thread frame delivery already observes keyboard input.
struct BorderlessWindow {
  HWND            window = nullptr;
  LONG_PTR        style = 0, extended = 0;
  WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
};
BorderlessWindow borderless_window;

bool SetWindowStyle(HWND window, int field, LONG_PTR value)
{
  SetLastError(0);
  return SetWindowLongPtrW(window, field, value) != 0 || GetLastError() == 0;
}

bool RestoreBorderlessWindow()
{
  auto& saved = borderless_window;
  if (!saved.window)
    return true;
  if (!IsWindow(saved.window)) {
    saved = {};
    return true;
  }
  const bool style    = SetWindowStyle(saved.window, GWL_STYLE, saved.style);
  const bool extended = SetWindowStyle(saved.window, GWL_EXSTYLE, saved.extended);
  const bool placed   = SetWindowPlacement(saved.window, &saved.placement) != 0;
  const bool framed   = SetWindowPos(saved.window, nullptr, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED)
                        != 0;
  if (style && extended && placed && framed)
    saved = {};
  return style && extended && placed && framed;
}

[[maybe_unused]] void BorderlessWindowFrame()
{
  if (!Config::Get().graphics.borderless_fullscreen) {
    RestoreBorderlessWindow();
    return;
  }
  unity::Key::Poll();
  if (!unity::Key::Down(unity::KeyCode::F11) || unity::Key::IsModified() || unity::Key::IsInputFocused())
    return;
  auto window = GameWindow();
  if (!window || GetForegroundWindow() != window)
    return;
  if (borderless_window.window) {
    RestoreBorderlessWindow();
    return;
  }
  // A natively borderless window has no owned windowed placement to restore.
  // Leave it native instead of inventing a previous size or style.
  const auto style = GetWindowLongPtrW(window, GWL_STYLE);
  if (!(style & WS_OVERLAPPEDWINDOW))
    return;
  BorderlessWindow saved;
  saved.window   = window;
  saved.style    = style;
  saved.extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
  MONITORINFO monitor{sizeof(MONITORINFO)};
  if (!GetWindowPlacement(window, &saved.placement)
      || !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
    return;
  borderless_window   = saved;
  const bool styled   = SetWindowStyle(window, GWL_STYLE, style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW));
  const bool extended = SetWindowStyle(
      window, GWL_EXSTYLE,
      saved.extended
          & ~static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
  if (!styled || !extended
      || !SetWindowPos(window, nullptr, monitor.rcMonitor.left, monitor.rcMonitor.top,
                       monitor.rcMonitor.right - monitor.rcMonitor.left,
                       monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                       SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
    RestoreBorderlessWindow();
}

// Custom launches can reach Update before their window exists. Retry once a second and retain the complete
// desired title after a failed write, so a partial native effect cannot duplicate the prefix on the next attempt.
struct ProfileWindowTitle {
  bool                                  complete = false;
  std::chrono::steady_clock::time_point next_attempt{};
  std::wstring                          title;

  void Update(std::chrono::steady_clock::time_point now)
  {
    if (complete)
      return;
    if (!File::HasCustomNames()) {
      complete = true;
      return;
    }
    if (now < next_attempt)
      return;
    next_attempt = now + std::chrono::seconds(1);

    auto window = GameWindow();
    if (!window)
      return;
    if (title.empty()) {
      const auto length = GetWindowTextLengthW(window);
      if (length <= 0)
        return;
      std::wstring native_title(static_cast<size_t>(length) + 1, L'\0');
      const auto   written = GetWindowTextW(window, native_title.data(), length + 1);
      if (written <= 0)
        return;
      native_title.resize(written);

      // Named Profiles keep their admitted ID. Directory-like -ccm routes can have no display stem; use the
      // selected config filename then, without changing their file routing or reviving old extension mutation.
      const auto prefix = File::Profile().empty()
                              ? File::FromUtf8(File::Config()).filename().replace_extension().wstring()
                              : File::FromUtf8(File::Profile()).wstring();
      title             = L"[" + prefix + L"] " + native_title;
    }
    if (SetWindowTextW(window, title.c_str()))
      complete = true;
  }
};

// With free_resize the original is skipped: the game's per-frame aspect-ratio enforcement is what would snap the
// window back.
//
// TODO: the old hook also stretched a 1024x768 fullscreen fallback to the monitor, but only once a window handle
// captured by a SetWindowLongW detour was set, and that detour was never installed. Bring it back with a tested
// trigger if the fallback is still seen.
void AspectRatioConstraintHandler_Update_Hook(auto original, il2cpp::Object* _this)
{
  static ProfileWindowTitle profile_title;
  profile_title.Update(std::chrono::steady_clock::now());
  if (!Config::Get().graphics.free_resize)
    original(_this);
}

// allow_cursor = false: the plain arrow, never clipped to the window. SetCursor_Injected is static; the trailing
// MethodInfo is passed through unchanged. The UI scale hook applies the same cursor when canvases rescale, since the
// game sets its own cursor again then.
void Cursor_SetCursor_Injected_Hook(auto original, intptr_t texture, const unity::Vector2* hotspot, int32_t mode,
                                    const MethodInfo* method)
{
  if (Config::Get().graphics.allow_cursor)
    return original(texture, hotspot, mode, method);
  SetCursor(LoadCursor(nullptr, IDC_ARROW));
  ClipCursor(nullptr);
}

intptr_t AspectRatioConstraintHandler_WndProc_Hook(auto original, HWND window, uint32_t message, intptr_t w_param,
                                                   intptr_t l_param)
{
  // Skip the game's aspect-ratio clamp and hand the message straight to Unity. The message arrives in the form of
  // the window's current procedure (the game's, installed through its own P/Invoke), and the A and W calls each
  // convert from their own form; the wrong one garbles non-ASCII text such as chat input.
  if (Config::Get().graphics.free_resize) {
    const auto procedure = AspectRatioConstraintHandler::unityWndProc();
    // Failed typed reads return zero, as does a not-yet-populated field. Let the native handler own either case.
    if (!procedure)
      return original(window, message, w_param, l_param);
    const auto forward = IsWindowUnicode(window) ? CallWindowProcW : CallWindowProcA;
    return forward(reinterpret_cast<WNDPROC>(procedure), window, message, w_param, l_param);
  }
  return original(window, message, w_param, l_param);
}

bool InstallFreeResize()
{
  il2cpp::HookSet hooks("FreeResize");
  IL2CPP_STAGE(hooks, AspectRatioConstraintHandler::Class::Method("Update"), AspectRatioConstraintHandler_Update_Hook);
  IL2CPP_STAGE(hooks, AspectRatioConstraintHandler::Class::Method("WndProc"),
               AspectRatioConstraintHandler_WndProc_Hook);
  return hooks.Commit();
}

bool InstallCursor()
{
  il2cpp::HookSet hooks("Cursor");
  IL2CPP_STAGE(hooks, unity::Cursor::Method("SetCursor_Injected"), Cursor_SetCursor_Injected_Hook);
  return hooks.Commit();
}
} // namespace

// The display scale of the monitor holding the game window, cached until `refresh` or a move to another monitor: the
// display mode's physical width over the width Windows reports to this process, 1.5 when it scales the game to 150%.
float WindowDpiScale(bool refresh)
{
  static HMONITOR last_monitor = nullptr;
  static float    scale        = 1.0f;

  // The game's own window, not GetActiveWindow(): that is null while the game is in the background, which would
  // pick the primary monitor.
  const auto monitor = MonitorFromWindow(GameWindow(), MONITOR_DEFAULTTONEAREST);
  if (!refresh && monitor == last_monitor)
    return scale;

  MONITORINFOEX info{};
  info.cbSize = sizeof(info);
  DEVMODE mode{.dmSize = sizeof(DEVMODE)};
  if (GetMonitorInfo(monitor, &info) && EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
    const auto logical_width = info.rcMonitor.right - info.rcMonitor.left;
    if (logical_width > 0)
      scale = static_cast<float>(mode.dmPelsWidth) / static_cast<float>(logical_width);
  }
  last_monitor = monitor;
  return scale;
}

// PLAY contains an F11 wrapper, but never installs the SetWindowLong hook that would connect it. Do not revive
// that dormant experiment as a parity requirement. Keep its implementation for a separately authorized feature.
const Feature kFreeResize{"FreeResize", &Config::Patches::freeresizehooks, InstallFreeResize};
// TODO: allow_cursor is not a test patch; it sits in testpatches only because it always has.
const Feature kCursor{"Cursor", &Config::Patches::testpatches, InstallCursor};
#else
float WindowDpiScale(bool)
{ return 1.0f; }
#endif
