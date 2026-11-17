#pragma once

// Exercise the production Windows callbacks with private window/cursor sinks.
// These supplied handles and metadata do not qualify a real window or native hook.
#include "file.h"
#include <Windows.h>
#include <chrono>

namespace window_fixture
{
HWND         handle    = reinterpret_cast<HWND>(uintptr_t{17});
bool         available = true, read_failure = false, set_success = true, apply_failed_set = false, unicode = true;
bool         custom = false;
std::string  profile, config = "community_patch_settings.toml";
std::wstring native_title = L"Star Trek Fleet Command", requested_title;
int enumerations = 0, length_reads = 0, title_reads = 0, title_sets = 0, cursor_loads = 0, cursor_sets = 0, clips = 0;
int forwarded_a = 0, forwarded_w = 0;
WNDPROC unity_proc = reinterpret_cast<WNDPROC>(uintptr_t{0x1234});

BOOL IsWindow(HWND window)
{ return available && window == handle; }
HWND GetTopWindow(HWND)
{
  ++enumerations;
  return available ? handle : nullptr;
}
HWND NextWindow(HWND, UINT)
{ return nullptr; }
DWORD GetCurrentProcessId()
{ return 17; }
DWORD GetWindowThreadProcessId(HWND window, LPDWORD process)
{
  Require(window == handle, "window discovery uses the supplied process-owned handle");
  *process = 17;
  return 19;
}
HWND GetWindow(HWND, UINT)
{ return nullptr; }
BOOL IsWindowVisible(HWND)
{ return true; }
HMONITOR Monitor(HWND, DWORD)
{ return nullptr; }
BOOL MonitorInfo(HMONITOR, LPMONITORINFO)
{ return false; }
BOOL DisplaySettings(LPCWSTR, DWORD, DEVMODEW*)
{ return false; }
int GetWindowTextLengthW(HWND window)
{
  Require(window == handle, "title length uses the selected window");
  ++length_reads;
  return static_cast<int>(native_title.size());
}
int GetWindowTextW(HWND window, LPWSTR buffer, int count)
{
  Require(window == handle, "title read uses the selected window");
  ++title_reads;
  if (read_failure)
    return 0;
  const auto length = std::min(native_title.size(), static_cast<size_t>(count - 1));
  std::copy_n(native_title.begin(), length, buffer);
  buffer[length] = L'\0';
  return static_cast<int>(length);
}
BOOL SetWindowTextW(HWND window, LPCWSTR title)
{
  Require(window == handle, "title write uses the selected window");
  ++title_sets;
  requested_title = title;
  if (set_success || apply_failed_set)
    native_title = requested_title;
  return set_success;
}
HCURSOR LoadArrow(HINSTANCE, LPCTSTR resource)
{
  Require(resource == IDC_ARROW, "disabled custom cursor requests the plain arrow");
  ++cursor_loads;
  return reinterpret_cast<HCURSOR>(uintptr_t{23});
}
HCURSOR SetCursor(HCURSOR cursor)
{
  Require(cursor == reinterpret_cast<HCURSOR>(uintptr_t{23}), "the loaded arrow reaches SetCursor");
  ++cursor_sets;
  return nullptr;
}
BOOL ClipCursor(const RECT* bounds)
{
  Require(!bounds, "disabled custom cursor releases clipping");
  ++clips;
  return true;
}
BOOL IsWindowUnicode(HWND window)
{
  Require(window == handle, "procedure routing uses the selected window");
  return unicode;
}
LRESULT Forward(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam, bool wide)
{
  Require(procedure == unity_proc && window == handle && message == WM_CHAR && wparam == 0x3a9 && lparam == -37,
          "window message forwards the exact Unity procedure and native arguments");
  ++(wide ? forwarded_w : forwarded_a);
  return wide ? 201 : 101;
}
LRESULT CallWindowProcW(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{ return Forward(procedure, window, message, wparam, lparam, true); }
LRESULT CallWindowProcA(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{ return Forward(procedure, window, message, wparam, lparam, false); }
} // namespace window_fixture

namespace WindowFixtureFile
{
const std::string& Profile()
{ return window_fixture::profile; }
const std::string& Config()
{ return window_fixture::config; }
bool HasCustomNames()
{ return window_fixture::custom; }
std::filesystem::path FromUtf8(std::string_view text)
{ return File::FromUtf8(text); }
} // namespace WindowFixtureFile

#pragma push_macro("LoadCursor")
#pragma push_macro("SetCursor")
#pragma push_macro("GetNextWindow")
#pragma push_macro("GetMonitorInfo")
#pragma push_macro("EnumDisplaySettings")
#undef LoadCursor
#undef SetCursor
#undef GetNextWindow
#undef GetMonitorInfo
#undef EnumDisplaySettings
#define File WindowFixtureFile
#define WindowDpiScale WindowFixtureDpiScale
#define IsWindow window_fixture::IsWindow
#define GetTopWindow window_fixture::GetTopWindow
#define GetNextWindow window_fixture::NextWindow
#define GetCurrentProcessId window_fixture::GetCurrentProcessId
#define GetWindowThreadProcessId window_fixture::GetWindowThreadProcessId
#define GetWindow window_fixture::GetWindow
#define IsWindowVisible window_fixture::IsWindowVisible
#define MonitorFromWindow window_fixture::Monitor
#define GetMonitorInfo window_fixture::MonitorInfo
#define EnumDisplaySettings window_fixture::DisplaySettings
#define GetWindowTextLengthW window_fixture::GetWindowTextLengthW
#define GetWindowTextW window_fixture::GetWindowTextW
#define SetWindowTextW window_fixture::SetWindowTextW
#define LoadCursor window_fixture::LoadArrow
#define SetCursor window_fixture::SetCursor
#define ClipCursor window_fixture::ClipCursor
#define IsWindowUnicode window_fixture::IsWindowUnicode
#define CallWindowProcW window_fixture::CallWindowProcW
#define CallWindowProcA window_fixture::CallWindowProcA
#include "../src/mod/features/window.cc"
#undef CallWindowProcA
#undef CallWindowProcW
#undef IsWindowUnicode
#undef ClipCursor
#undef SetCursor
#undef LoadCursor
#undef SetWindowTextW
#undef GetWindowTextW
#undef GetWindowTextLengthW
#undef IsWindowVisible
#undef EnumDisplaySettings
#undef GetMonitorInfo
#undef MonitorFromWindow
#undef GetWindow
#undef GetWindowThreadProcessId
#undef GetCurrentProcessId
#undef GetNextWindow
#undef GetTopWindow
#undef IsWindow
#undef WindowDpiScale
#undef File
#pragma pop_macro("GetNextWindow")
#pragma pop_macro("EnumDisplaySettings")
#pragma pop_macro("GetMonitorInfo")
#pragma pop_macro("SetCursor")
#pragma pop_macro("LoadCursor")

namespace
{
bool WindowFixtureReadStatic(FieldInfo* field, void* result)
{
  if (std::string_view(field->name) != "_unityWndProc")
    return false;
  const auto pointer = reinterpret_cast<intptr_t>(window_fixture::unity_proc);
  std::memcpy(result, &pointer, sizeof(pointer));
  return true;
}

void WindowTitleCases(std::string_view mode)
{
  using namespace std::chrono;
  using namespace window_fixture;
  const steady_clock::time_point now{seconds(42)};
  ProfileWindowTitle             state;
  if (mode == "window-title-ordinary") {
    state.Update(now);
    state.Update(now + seconds(2));
    Require(state.complete && enumerations == 0 && length_reads == 0 && title_reads == 0 && title_sets == 0,
            "ordinary launch completes without discovering or reading an unchanged window title");
    return;
  }
  custom                = true;
  profile               = "alt";
  std::wstring expected = L"[alt] Star Trek Fleet Command";
  if (mode == "window-title-unicode") {
    profile  = "J\xc3\xa9"
               "an \xce\xa9";
    expected = L"[J\u00e9an \u03a9] Star Trek Fleet Command";
  } else if (mode == "window-title-named") {
    profile  = "profile-immutable-42";
    config   = "C:/catalog/profile-immutable-42/config.toml";
    expected = L"[profile-immutable-42] Star Trek Fleet Command";
  } else if (mode == "window-title-empty-label") {
    profile.clear();
    config   = "custom/.toml";
    expected = L"[.toml] Star Trek Fleet Command";
  } else if (mode == "window-title-delayed") {
    available = false;
    state.Update(now);
    Require(!state.complete && title_reads == 0 && title_sets == 0, "missing window leaves title work pending");
    available = true;
    state.Update(now + seconds(1));
    Require(state.complete && requested_title == expected, "delayed window receives the profile title");
    return;
  } else if (mode == "window-title-read") {
    read_failure = true;
    state.Update(now);
    Require(!state.complete && title_sets == 0 && state.title.empty(),
            "failed title read cannot write or cache a prefix without the native title");
    read_failure = false;
    state.Update(now + seconds(1));
    Require(state.complete && requested_title == expected, "failed title read retries with the actual native title");
    return;
  } else if (mode == "window-title-cadence") {
    set_success = false;
    state.Update(now);
    const auto reads = title_reads;
    for (auto elapsed : {milliseconds(1), milliseconds(100), milliseconds(999)})
      state.Update(now + elapsed);
    Require(!state.complete && title_sets == 1 && title_reads == reads,
            "failed title writes retry no faster than once per second");
    set_success = true;
    state.Update(now + seconds(1));
    Require(state.complete && title_sets == 2 && requested_title == expected,
            "pending title write retries at the one-second boundary");
    return;
  } else if (mode == "window-title-cache") {
    set_success      = false;
    apply_failed_set = true;
    state.Update(now);
    Require(!state.complete && title_sets == 1, "failed title write remains pending");
    set_success = true;
    state.Update(now + seconds(1));
    Require(state.complete && title_sets == 2 && title_reads == 1 && requested_title == expected,
            "retry reuses the complete cached title even if the failed write changed the visible caption");
    return;
  } else {
    Require(mode == "window-title-oneshot" || mode == "window-title-unicode" || mode == "window-title-named"
                || mode == "window-title-empty-label",
            "known window title case");
  }
  state.Update(now);
  Require(state.complete && title_sets == 1 && requested_title == expected,
          "profile title uses its selected UTF-8 label and preserves the complete native title");
  const auto reads = title_reads;
  state.Update(now + seconds(2));
  Require(title_sets == 1 && title_reads == reads, "successful title update is one-shot");
}

void WindowCursorCases()
{
  using namespace window_fixture;
  const intptr_t       texture = 0x72345;
  const unity::Vector2 hotspot{3.5f, -7.25f};
  const MethodInfo     metadata{};
  int                  forwarded = 0;
  const auto           original  = [&](intptr_t actual_texture, const unity::Vector2* actual_hotspot, int32_t mode,
                                       const MethodInfo* method) {
    Require(actual_texture == texture && actual_hotspot == &hotspot && mode == 7 && method == &metadata,
            "allowed custom cursor forwards exact texture, hotspot, mode and hidden metadata");
    ++forwarded;
  };
  Config::Get().graphics.allow_cursor = true;
  Cursor_SetCursor_Injected_Hook(original, texture, &hotspot, 7, &metadata);
  Require(forwarded == 1 && cursor_loads == 0 && cursor_sets == 0 && clips == 0,
          "allowed custom cursor performs only the native call");
  Config::Get().graphics.allow_cursor = false;
  Cursor_SetCursor_Injected_Hook(original, texture, &hotspot, 7, &metadata);
  Require(forwarded == 1 && cursor_loads == 1 && cursor_sets == 1 && clips == 1,
          "disabled custom cursor sets the arrow, frees clipping and suppresses native cursor changes");
  Config::Get().graphics.allow_cursor = true;
  Cursor_SetCursor_Injected_Hook(original, texture, &hotspot, 7, &metadata);
  Require(forwarded == 2 && cursor_sets == 1 && clips == 1, "cursor behavior observes live setting changes");
}

void WindowResizeCases(std::string_view mode)
{
  using namespace window_fixture;
  int            updates = 0, originals = 0;
  il2cpp::Object receiver;
  const auto     update = [&](il2cpp::Object* actual) {
    Require(actual == &receiver, "disabled free resize forwards the original receiver");
    ++updates;
  };
  Config::Get().graphics.free_resize = false;
  AspectRatioConstraintHandler_Update_Hook(update, &receiver);
  Config::Get().graphics.free_resize = true;
  AspectRatioConstraintHandler_Update_Hook(update, &receiver);
  Require(updates == 1, "only disabled free resize calls native per-frame aspect enforcement");

  auto& handler = Define("Digit.Client.Utils", "AspectRatioConstraintHandler", IL2CPP_TYPE_CLASS);
  if (mode != "window-resize-missing")
    AddField(handler, "_unityWndProc", intptr_class, 0, true);
  tow_static_field_read_override = WindowFixtureReadStatic;
  const auto original            = [&](HWND window, uint32_t message, intptr_t wparam, intptr_t lparam) -> intptr_t {
    Require(window == handle && message == WM_CHAR && wparam == 0x3a9 && lparam == -37,
            "native aspect procedure receives its exact original arguments");
    ++originals;
    return 301;
  };
  Config::Get().graphics.free_resize = false;
  Require(AspectRatioConstraintHandler_WndProc_Hook(original, handle, WM_CHAR, 0x3a9, -37) == 301 && originals == 1
              && forwarded_a == 0 && forwarded_w == 0,
          "disabled free resize leaves procedure handling native");
  Config::Get().graphics.free_resize = true;
  if (mode == "window-resize-null" || mode == "window-resize-missing") {
    if (mode == "window-resize-null")
      unity_proc = nullptr;
    Require(AspectRatioConstraintHandler_WndProc_Hook(original, handle, WM_CHAR, 0x3a9, -37) == 301 && originals == 2
                && forwarded_a == 0 && forwarded_w == 0,
            "unavailable Unity procedure falls back to the game's original procedure");
  } else {
    Require(mode == "window-resize", "known window resize case");
    unicode = true;
    Require(AspectRatioConstraintHandler_WndProc_Hook(original, handle, WM_CHAR, 0x3a9, -37) == 201,
            "Unicode window forwards through CallWindowProcW");
    unicode = false;
    Require(AspectRatioConstraintHandler_WndProc_Hook(original, handle, WM_CHAR, 0x3a9, -37) == 101,
            "ANSI window forwards through CallWindowProcA");
    Require(originals == 1 && forwarded_w == 1 && forwarded_a == 1,
            "available Unity procedure bypasses only the aspect-clamping original");
  }
  tow_static_field_read_override = nullptr;
}

void WindowNativeCases(std::string_view mode)
{
  if (mode.starts_with("window-title-"))
    WindowTitleCases(mode);
  else if (mode == "window-cursor")
    WindowCursorCases();
  else
    WindowResizeCases(mode);
  std::cout << "Window callback fixture passed: " << mode << '\n';
}
} // namespace
