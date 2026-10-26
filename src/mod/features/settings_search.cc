// Native root-page search. The inline TMP field reuses PLAY's panel layout and font; metadata/results stay in
// maintenance_pages. Search result navigation is deferred through the existing native page owner.
#include "game/settings_search.h"
#include "features/features.h"
#include "settings/maintenance_pages.h"
#include "settings/native_controls.h"
#include "shortcuts.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <thread>
#include <unity/application.h>
#include <unity/input.h>

namespace
{
using namespace mod_settings;
using namespace mod_settings::native;
struct NativeSearchState {
  SettingsSearchSurface  surface;
  Shortcut::CaptureToken capture = 0;
  PageVisitToken         visit = 0, attempted = 0;
  std::uint64_t          revision = 0;
  std::thread::id        thread;
  bool                   active = false, closing = false, opening = false;
};
NativeSearchState& NativeSearch()
{
  static auto* state = new NativeSearchState;
  return *state;
}
bool SearchThread()
{ return NativeSearch().active && NativeSearch().thread == std::this_thread::get_id(); }
bool SearchDelivery()
{
  return std::ranges::any_of(FeatureInstallations(), [](const auto& feature) {
    return feature.name && std::string_view(feature.name) == "NativeSettingsSearch" && feature.frame_ready;
  });
}
bool RootSearchVisit(SettingsPageController*& controller, SettingsContainerContext*& context, PageVisitToken& token)
{
  std::string_view page;
  const auto       pages = NativeSettingsPages();
  if (pages.empty() || !CurrentSettingsPage(controller, context, page) || page != pages.front().id)
    return false;
  token = CurrentPageVisit();
  return token != 0;
}
void CleanupSearch()
{
  auto& state = NativeSearch();
  if (!state.surface.Destroy())
    return;
  if (state.capture) {
    Shortcut::ReleaseCapture(state.capture);
    state.capture = 0;
  }
  if (!Shortcut::CaptureBlocksShortcuts() && state.surface.navigation.RestoreInput()) {
    state.closing = false;
    state.visit   = 0;
  }
}
void CloseSearch()
{
  auto& state   = NativeSearch();
  state.closing = true;
  if (state.opening)
    return;
  try {
    CleanupSearch();
  } catch (...) {
  }
}
void ArmSearchInput()
{
  auto& state = NativeSearch();
  if (state.capture && Shortcut::CaptureOwnedBy(state.capture))
    return;
  if (Shortcut::CaptureBlocksShortcuts())
    return;
  auto owner = il2cpp::GcHandle::Strong(state.surface.controller.Get());
  if (!owner)
    throw std::runtime_error("native search owner unavailable");
  auto* back    = owner.Get<SettingsPageController>()->Field<"_backButtonViewController", SettingsEditorBack*>();
  state.capture = Shortcut::ClaimCapture();
  state.surface.navigation.HoldInput(back);
}
bool OpenSearch(SettingsPageController* controller, PageVisitToken token)
{
  auto& state = NativeSearch();
  if (!SearchThread() || !SearchDelivery() || state.opening || state.closing || state.surface.object
      || TextEntryActive())
    return false;
  auto owner = il2cpp::GcHandle::Strong(controller);
  if (!owner || !IsPageVisitCurrent(token))
    return false;
  state.attempted = token;
  state.visit     = token;
  state.opening   = true;
  try {
    state.surface.Build(controller, SettingsSearchQuery());
    state.opening = false;
    if (state.closing || !IsPageVisitCurrent(token))
      throw std::runtime_error("native search page changed");
    state.revision = SettingsSearchRevision();
    return true;
  } catch (...) {
    state.opening = false;
    CloseSearch();
    SetSettingsSearchQuery({});
    SetSettingsSearchError("The native search field could not open. Reopen Settings or try Search again.");
    return false;
  }
}
void SearchPageVisit(SettingsPageController* controller, SettingsContainerContext*, std::string_view page,
                     PageVisitToken token, bool entering)
{
  if (!SearchThread())
    return;
  const auto pages = NativeSettingsPages();
  if (pages.empty() || page != pages.front().id)
    return;
  if (!entering) {
    CloseSearch();
    return;
  }
  OpenSearch(controller, token);
}
void NativeSearchFrame()
{
  auto& state = NativeSearch();
  if (!SearchThread())
    return;
  try {
    if (state.closing) {
      CleanupSearch();
      return;
    }
    SettingsPageController*   controller = nullptr;
    SettingsContainerContext* context    = nullptr;
    PageVisitToken            token      = 0;
    if (!RootSearchVisit(controller, context, token)) {
      if (state.surface.object || state.capture || state.surface.navigation.HasInputHold())
        CloseSearch();
      return;
    }
    if (!state.surface.object && state.attempted != token)
      OpenSearch(controller, token);
    if (!state.surface.object || !IsPageVisitCurrent(state.visit) || TextEntryActive())
      return;
    if (!unity::Application::isFocused())
      return; // Keep field/query while unfocused; input ownership waits too.
    unity::Key::Poll();
    if (state.revision != SettingsSearchRevision()) {
      state.surface.Write(SettingsSearchQuery());
      state.revision = SettingsSearchRevision();
    } else {
      const auto query = state.surface.Read();
      if (query != SettingsSearchQuery()) {
        SetSettingsSearchQuery(query);
        state.revision = SettingsSearchRevision();
      }
    }
    if (state.surface.Focused()) {
      ArmSearchInput();
      if (state.capture && Shortcut::CaptureOwnedBy(state.capture) && unity::Key::Down(unity::KeyCode::Escape)) {
        unity::Key::ClearInputFocus();
        Shortcut::ReleaseCapture(state.capture);
        state.capture = 0;
      }
    } else if (state.capture) {
      Shortcut::ReleaseCapture(state.capture);
      state.capture = 0;
    }
    if (!state.capture && !Shortcut::CaptureBlocksShortcuts())
      state.surface.navigation.RestoreInput();
  } catch (...) {
    CloseSearch();
    SetSettingsSearchQuery({});
    SetSettingsSearchError("Search became unavailable; normal Settings navigation was restored.");
  }
}
bool InstallNativeSearch()
{
  if (!SettingsSearchSurface::Available() || NativeSettingsPages().empty())
    return false;
  auto& state  = NativeSearch();
  state.thread = std::this_thread::get_id();
  state.active = RegisterPageVisitObserver(SearchPageVisit);
  return state.active;
}
} // namespace

bool mod_settings::native::SettingsSearchAvailable()
{ return SearchThread() && SearchDelivery(); }
bool mod_settings::native::FocusSettingsSearch()
{
  if (!SettingsSearchAvailable() || TextEntryActive())
    return false;
  auto&                     state      = NativeSearch();
  SettingsPageController*   controller = nullptr;
  SettingsContainerContext* context    = nullptr;
  PageVisitToken            token      = 0;
  if (!RootSearchVisit(controller, context, token))
    return false;
  try {
    if (!state.surface.object && !OpenSearch(controller, token))
      return false;
    if (!IsPageVisitCurrent(state.visit))
      return false;
    state.surface.Focus();
    ArmSearchInput();
    return state.surface.Focused();
  } catch (...) {
    CloseSearch();
    return false;
  }
}
bool mod_settings::native::AllowsSettingsSearchAction(std::string_view id)
{
  if (!SearchThread() || (id != "community_mod.search.results" && id != "community_mod.search.focus"))
    return false;
  auto& state = NativeSearch();
  return !state.closing && state.surface.object && state.capture && Shortcut::CaptureOwnedBy(state.capture)
         && IsPageVisitCurrent(state.visit) && !TextEntryActive();
}
const Feature kNativeSettingsSearch{"NativeSettingsSearch", &Config::Patches::nativesettingshooks, InstallNativeSearch,
                                    NativeSearchFrame};
