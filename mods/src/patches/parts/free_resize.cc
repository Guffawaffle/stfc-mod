/**
 * @file free_resize.cc
 * @brief Windows-only: enables free window resizing and borderless fullscreen (F11).
 *
 * The game ships as a fixed-size or pop-up window on Windows. This patch:
 *   - Intercepts SetWindowLongW to replace the window style with WS_OVERLAPPEDWINDOW,
 *     giving the user standard resize handles, minimize, and maximize buttons.
 *   - Optionally installs a WndProc sub-class that toggles borderless fullscreen on F11.
 *   - Hooks AspectRatioConstraintHandler to bypass Unity's aspect-ratio enforcement
 *     and fix resolution detection when going fullscreen.
 *   - Sets a custom window title from the user's config file.
 */
#if _WIN32
#include <Windows.h>

#include "config.h"
#include "errormsg.h"
#include "file.h"

#include <spud/detour.h>

#include "prime/AspectRatioConstraintHandler.h"
#include "prime/IList.h"
#include "windowtitle.h"

// ─── Globals ───────────────────────────────────────────────────────────────────

static bool            WndProcInstalled  = false;
static LONG_PTR        oWndProc          = NULL;
static WINDOWPLACEMENT g_wpPrev          = {sizeof(g_wpPrev)};
LONG                   oldWindowStandard = 0;
LONG                   oldWindowExtended = 0;
HWND                   unityWindow       = nullptr;

// ─── Borderless Fullscreen Toggle ───────────────────────────────────────────

/// Toggles between borderless fullscreen and the previous windowed placement.
void ToggleFullscreen(HWND hWnd)
{
  // Get window styles
  auto styleCurrentWindowStandard = GetWindowLong(hWnd, GWL_STYLE);
  auto styleCurrentWindowExtended = GetWindowLong(hWnd, GWL_EXSTYLE);

  if (styleCurrentWindowStandard & WS_OVERLAPPEDWINDOW) {

    oldWindowStandard = styleCurrentWindowStandard;
    oldWindowExtended = styleCurrentWindowExtended;

    // Compute new styles (XOR of the inverse of all the bits to filter)
    auto styleNewWindowStandard = styleCurrentWindowStandard
                                  & ~(WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_MAXIMIZEBOX
                                      | WS_MINIMIZEBOX // same as Group
                                  );

    auto styleNewWindowExtended = styleCurrentWindowExtended
                                  & ~(WS_EX_DLGMODALFRAME | WS_EX_COMPOSITED | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE
                                      | WS_EX_LAYERED | WS_EX_STATICEDGE | WS_EX_TOOLWINDOW | WS_EX_APPWINDOW);

    SetWindowLong(hWnd, GWL_STYLE, styleNewWindowStandard);
    SetWindowLong(hWnd, GWL_EXSTYLE, styleNewWindowExtended);

    MONITORINFO mi = {sizeof(mi)};
    if (GetWindowPlacement(hWnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
      SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLong(hWnd, GWL_STYLE, oldWindowStandard);
    SetWindowLong(hWnd, GWL_EXSTYLE, oldWindowExtended);
    SetWindowPlacement(hWnd, &g_wpPrev);
    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
}

/// WndProc sub-class that intercepts F11 for borderless fullscreen toggle.
LRESULT __stdcall WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) // WndProc wrapper
{
  switch (uMsg) {
    case WM_KEYUP: {
      if (wParam == VK_F11) {
        ToggleFullscreen(hWnd);
      }
    } break;
  }
  return CallWindowProcW((WNDPROC)oWndProc, hWnd, uMsg, wParam, lParam);
}

// ─── Window Style Hook ────────────────────────────────────────────────────────

decltype(SetWindowLongW)* oSetWindowLong = nullptr;
/**
 * @brief Hook: SetWindowLongW (Win32 API)
 *
 * Intercepts window style changes to inject free-resize and borderless support.
 * Original behavior: Unity calls SetWindowLongW to set the game window style.
 * Our modification: when the target is GWL_STYLE on the Unity window class,
 *   replaces the style with WS_OVERLAPPEDWINDOW (if free_resize is enabled)
 *   and installs the F11 WndProc sub-class (if borderless_fullscreen is enabled).
 */
LONG                      SetWindowLongW_Hook(_In_ HWND hWnd, _In_ int nIndex, _In_ LONG dwNewLong)
{
  if (nIndex == GWL_STYLE) {
    char clsName_v[256];
    GetClassNameA(hWnd, clsName_v, 256);
    if (clsName_v == std::string("UnityWndClass")) {
      unityWindow = hWnd;
      if (Config::Get().free_resize) {
        if (!(dwNewLong & WS_POPUP)) {
          dwNewLong = WS_OVERLAPPEDWINDOW;
        }
      }

      if (!WndProcInstalled) {
        if (Config::Get().borderless_fullscreen) {
          oWndProc         = SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
          WndProcInstalled = true;
        }
      }
    }
  }
  return SetWindowLong(hWnd, nIndex, dwNewLong);
}

// ─── Aspect Ratio & Resolution Hooks ────────────────────────────────────────

struct Resolution {
  int m_Width;
  int m_Height;
  int m_RefreshRate;
};

struct ResolutionArray {
  Il2CppObject obj;
  void*        bounds;
  size_t       maxlength;
  Resolution   data[1];
};

/**
 * @brief Hook: AspectRatioConstraintHandler::Update
 *
 * Intercepts per-frame aspect-ratio enforcement.
 * Original method: constrains the viewport to a fixed aspect ratio.
 * Our modification: bypasses the original entirely. Instead, detects when
 *   Unity reports 1024×768 while the monitor is larger (a common init quirk)
 *   and forces the resolution to match the monitor. Also applies the user's
 *   custom window title on the first call.
 */
void AspectRatioConstraintHandler_Update(auto original, void* _this)
{
  static auto set_title       = true;
  static auto get_fullscreen  = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Screen::get_fullScreen()");
  static auto get_height      = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_height()");
  static auto get_width       = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_width()");
  static auto get_resolutions = il2cpp_resolve_icall_typed<ResolutionArray*()>("UnityEngine.Screen::get_resolutions()");
  static auto SetResolution   = il2cpp_resolve_icall_typed<void(int, int, int, int)>(
      "UnityEngine.Screen::SetResolution(System.Int32,System.Int32,UnityEngine.FullScreenMode,System.Int32)");

  if (unityWindow) {
    if (get_fullscreen()) {
      static auto get_currentResolution_Injected = il2cpp_resolve_icall_typed<void(Resolution*)>(
          "UnityEngine.Screen::get_currentResolution_Injected(UnityEngine.Resolution&)");
      auto       height = get_height();
      auto       width  = get_width();
      Resolution res;
      get_currentResolution_Injected(&res);

      MONITORINFO mi = {sizeof(mi)};
      GetWindowPlacement(unityWindow, &g_wpPrev)
          && GetMonitorInfo(MonitorFromWindow(unityWindow, MONITOR_DEFAULTTOPRIMARY), &mi);
      auto m_width  = mi.rcMonitor.right - mi.rcMonitor.left;
      auto m_height = mi.rcMonitor.bottom - mi.rcMonitor.top;
      if (m_width != 1024 && m_height != 768 && width == 1024 && height == 768) {
        if (m_width != width || m_width != height) {
          SetResolution(m_width, m_height, 1, res.m_RefreshRate);
        }
      }
    }
  }

#if _WIN32
  if (set_title) {
    auto title = File::Title();
    if (!title.empty()) {
      if (WindowTitle::Set(title)) {
        set_title = false;
      }
    }
  }
#endif

  // SetResolution(1920, 1080, 4, 60);
}

/**
 * @brief Hook: AspectRatioConstraintHandler::WndProc
 *
 * Intercepts the game's custom WndProc for aspect-ratio messages.
 * Original method: filters WM_SIZE/WM_MOVE to enforce aspect ratio.
 * Our modification: when free_resize is enabled, bypasses the filter
 *   and forwards messages directly to Unity's original WndProc.
 */
intptr_t AspectRatioConstraintHandler_WndProc(auto original, HWND hWnd, uint32_t msg, intptr_t wParam, intptr_t lParam)
{
  if (Config::Get().free_resize) {
    return CallWindowProcA(AspectRatioConstraintHandler::_unityWndProc(), hWnd, msg, wParam, lParam);
  }
  return original(hWnd, msg, wParam, lParam);
}

/**
 * @brief Installs window resize and aspect-ratio bypass hooks.
 *
 * Hooks AspectRatioConstraintHandler::Update (resolution fix + title) and
 * AspectRatioConstraintHandler::WndProc (bypass aspect enforcement).
 * Note: SetWindowLongW is hooked separately at a lower level by the proxy DLL.
 */
void InstallFreeResizeHooks()
{
  auto AspectRatioConstraintHandler_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Utils", "AspectRatioConstraintHandler");
  if (!AspectRatioConstraintHandler_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Utils", "AspectRatioConstraintHandler");
  } else {
    auto ptr_update = AspectRatioConstraintHandler_helper.GetMethod("Update");
    if (ptr_update == nullptr) {
      ErrorMsg::MissingMethod("AspectRatioConstraintHandler", "Update");
      return;
    }

    auto ptr_wndproc = AspectRatioConstraintHandler_helper.GetMethod("WndProc");
    if (ptr_wndproc == nullptr) {
      ErrorMsg::MissingMethod("AspectRatioConstraintHandler", "WndProc");
      return;
    }

    SPUD_STATIC_DETOUR(ptr_update, AspectRatioConstraintHandler_Update);
    SPUD_STATIC_DETOUR(ptr_wndproc, AspectRatioConstraintHandler_WndProc);
  }
}
#endif
