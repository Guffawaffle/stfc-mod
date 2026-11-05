// Native Settings exact text/number entry. Canonical models own validation, mutation and persistence; this owner
// holds one native editor, its page visit and Back/EventSystem suppression through the dismissal key release.
#include "game/settings_text_entry.h"
#include "features/features.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"
#include "settings/native_text_entry.h"
#include "shortcuts.h"
#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>
#include <unity/application.h>
#include <unity/input.h>

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
namespace
{
using namespace mod_settings;
using namespace mod_settings::native;
struct NativeTextEntryState {
  SettingsTextEntrySurface surface;
  TextEntrySession         session;
  NativeCallback<void>     click;
  Shortcut::CaptureToken   capture       = 0;
  PageVisitToken           visit         = 0;
  TextEntrySession::Token  session_token = 0;
  std::thread::id          thread;
  bool                     installed = false, invoking = false, closing = false, opening = false;
};
NativeTextEntryState& NativeTextEntry()
{
  static auto* state = new NativeTextEntryState;
  return *state;
}
bool TextEntryThread()
{ return NativeTextEntry().installed && NativeTextEntry().thread == std::this_thread::get_id(); }
bool TextEntryDelivery()
{
  return std::ranges::any_of(FeatureInstallations(), [](const auto& feature) {
    return feature.name && std::string_view(feature.name) == "NativeSettingsTextEntry" && feature.frame_ready;
  });
}
void CleanupTextEntry()
{
  auto& state = NativeTextEntry();
  if (state.session.Active())
    return;
  if (!state.surface.DestroySurface())
    return;
  if (state.capture) {
    Shortcut::ReleaseCapture(state.capture);
    state.capture = 0;
  }
  // The shared capture owner drains every held raw key/mouse input. Back polls Escape independently from
  // ScreenManager/EventSystem, so keep both native holds until that boundary has ended too.
  if (!Shortcut::CaptureBlocksShortcuts() && state.surface.RestoreInput()) {
    state.closing = false;
    state.visit   = 0;
  }
}
void ValidateTextEntry()
{
  auto&      state = NativeTextEntry();
  const auto token = state.session_token;
  state.session.Draft(state.surface.Read());
  const auto valid = state.session.Validate();
  if (!state.session.Current(token))
    return;
  const auto* request = state.session.Request();
  state.surface.Status(state.session.Error().empty() ? request->help : state.session.Error(), valid);
}
void ApplyTextEntry()
{
  auto&      state = NativeTextEntry();
  const auto token = state.session_token;
  state.session.Draft(state.surface.Read());
  const auto result = state.session.Apply();
  if (state.session_token != token)
    return;
  if (result == TextEntrySession::Result::Applied) {
    state.closing = true;
    CleanupTextEntry();
    RefreshActions();
    RequestPageRefresh();
  } else if (state.session.Current(token)) {
    state.surface.Status(state.session.Error(), false);
  }
}
void TextEntryClick(Il2CppObject* token, const MethodInfo*)
{
  auto& state = NativeTextEntry();
  if (!TextEntryThread() || !state.session.Active() || state.invoking || state.closing
      || !Shortcut::CaptureReady(state.capture))
    return;
  state.invoking = true;
  struct Guard {
    bool& value;
    ~Guard()
    { value = false; }
  } guard{state.invoking};
  try {
    if (!unity::Application::isFocused() || !state.surface.OwnerActive() || !IsPageVisitCurrent(state.visit)) {
      CloseTextEntry();
      return;
    }
    if (token == state.surface.tokens[0].Get())
      ApplyTextEntry();
    else if (token == state.surface.tokens[1].Get())
      CloseTextEntry();
    else if (token == state.surface.tokens[2].Get() && state.session.RecordAgain()) {
      state.capture = Shortcut::ClaimCapture();
      state.surface.Write(state.session.Value());
      ValidateTextEntry();
    }
  } catch (...) {
    CloseTextEntry();
  }
}
void NativeTextEntryFrame()
{
  auto& state = NativeTextEntry();
  if (!TextEntryThread() || (!state.session.Active() && !state.closing))
    return;
  try {
    if (!state.session.Active()) {
      CleanupTextEntry();
      return;
    }
    if (!unity::Application::isFocused() || !state.surface.OwnerActive() || !IsPageVisitCurrent(state.visit)) {
      CloseTextEntry();
      return;
    }
    unity::Key::Poll();
    if (!Shortcut::CaptureReady(state.capture))
      return;
    if (unity::Key::Down(unity::KeyCode::Escape)) {
      CloseTextEntry();
      return;
    }
    if (state.session.Request()->record_frame) {
      const auto token   = state.session_token;
      const auto sampled = state.session.SampleRecording();
      if (!state.session.Current(token))
        return;
      if (sampled == TextEntrySession::Recording::Changed) {
        state.surface.Write(state.session.Value());
        ValidateTextEntry();
      } else if (sampled == TextEntrySession::Recording::Unavailable) {
        state.surface.Status(state.session.Error(), false);
      } else if (sampled == TextEntrySession::Recording::Stale) {
        CloseTextEntry();
      }
      return; // Return/KeypadEnter are recordable; only explicit Confirm submits a recording request.
    }
    const auto  token              = state.session_token;
    const auto* request            = state.session.Request();
    const auto  validation_notice  = request->validation_changed;
    const bool  validation_changed = validation_notice && validation_notice();
    if (!state.session.Current(token))
      return;
    if (validation_changed || state.surface.Read() != state.session.Value())
      ValidateTextEntry();
    if (!state.session.Active())
      return;
    if (unity::Key::Down(unity::KeyCode::Return) || unity::Key::Down(unity::KeyCode::KeypadEnter)) {
      if (!state.invoking) {
        state.invoking = true;
        struct Guard {
          bool& value;
          ~Guard()
          { value = false; }
        } guard{state.invoking};
        ApplyTextEntry();
      }
    }
  } catch (...) {
    CloseTextEntry();
  }
}
bool InstallNativeTextEntry()
{
  auto&      state  = NativeTextEntry();
  const auto schema = SettingsEditorToken::Class::Method<void()>(".ctor");
  if (!SettingsTextEntrySurface::Available() || !state.click.Initialize(schema.Info(), TextEntryClick))
    return false;
  state.thread    = std::this_thread::get_id();
  state.installed = true;
  return true;
}
} // namespace

bool mod_settings::TextEntryActive()
{ return TextEntryThread() && NativeTextEntry().session.Active(); }
void mod_settings::CloseTextEntry()
{
  if (!TextEntryThread())
    return;
  auto& state   = NativeTextEntry();
  state.closing = true;
  state.session.Cancel(); // Revokes request identity before an optional cancel callback can reenter.
  if (state.opening)
    return; // Construction owns live local surfaces until the binding returns or fails.
  try {
    CleanupTextEntry();
  } catch (...) {
  }
}
bool mod_settings::OpenTextEntry(TextEntryRequest request)
{
  auto&      state = NativeTextEntry();
  const auto visit = CurrentPageVisit();
  auto       host  = il2cpp::GcHandle::Strong(InvokingActionWidget());
  if (!TextEntryThread() || !TextEntryDelivery() || !host || !visit || !request.commit || state.invoking
      || (request.record_frame && !request.read_only) || state.session.Active() || state.closing || state.opening
      || state.surface.object || state.surface.HasInputHold() || Shortcut::CaptureBlocksShortcuts())
    return false;
  state.capture      = Shortcut::ClaimCapture();
  const auto capture = state.capture;
  state.visit        = visit;
  state.opening      = true;
  try {
    state.surface.Build(host.Get<il2cpp::Object>(), request, state.click.method());
    state.opening = false;
    if (state.closing || state.capture != capture || !IsPageVisitCurrent(visit))
      throw std::runtime_error("settings editor page changed during construction");
    state.session_token = state.session.Open(std::move(request), [visit] { return IsPageVisitCurrent(visit); });
    if (!state.session_token)
      throw std::runtime_error("settings editor request unavailable");
    ValidateTextEntry();
    return state.session.Current(state.session_token);
  } catch (...) {
    state.opening      = false;
    static bool warned = false;
    if (!std::exchange(warned, true))
      spdlog::warn("[NativeSettingsTextEntry] native editor unavailable");
    CloseTextEntry();
    return false;
  }
}
const Feature kNativeSettingsTextEntry{"NativeSettingsTextEntry", &Config::Patches::nativesettingshooks,
                                       InstallNativeTextEntry, NativeTextEntryFrame};
#else
namespace
{
bool InstallNativeTextEntry()
{ return false; }
} // namespace
const Feature kNativeSettingsTextEntry{"NativeSettingsTextEntry", &Config::Patches::nativesettingshooks,
                                       InstallNativeTextEntry};
#endif
