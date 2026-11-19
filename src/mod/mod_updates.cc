// Downstream package discovery and [ui] check_for_mod_updates.
#include "mod_updates.h"
#include "build_identity.h"
#include "config.h"
#include "features/notifications.h"
#include "frame.h"
#include "game/update_notice.h"
#include "mod_state.h"
#include "release_index.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"
#include "shortcuts.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <optional>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>
#include <unity/application.h>
#include <utility>

#if _WIN32
#include <Windows.h>

#include <shellapi.h>
#elif __APPLE__
bool OpenModReleaseMac(std::string_view url);
#endif

namespace mod_updates
{
namespace
{
  using Clock = std::chrono::steady_clock;
  using namespace std::chrono_literals;

  constexpr auto index_url   = "https://raw.githubusercontent.com/Guffawaffle/stfc-mod/play/releases/update-index.json";
  constexpr auto release_url = "https://github.com/Guffawaffle/stfc-mod/releases/tag/";
  constexpr auto check_interval = 24h;
  constexpr auto startup_delay  = 30s;
  constexpr auto state_key      = "mod_update_checks";
  constexpr auto updates_page   = "community_mod.updates";
  constexpr auto notice_title   = "STFC Community Mod update available";

  struct WorkResult {
    CheckResult          result;
    std::chrono::seconds retry_after{};
    bool                 notify = false;
    std::string          ignored_tag{};
  };

  const auto& Installed()
  {
    static const auto installed = IdentifyRelease(build_identity::DisplayVersion());
    return installed;
  }

  struct UpdateCheckState {
    Clock::time_point       next_check = Clock::now() + startup_delay;
    std::future<WorkResult> pending;
    std::stop_source        stop{std::nostopstate};
    CheckResult             result{CheckStatus::Idle, {}};
    bool                    registered = false;
    bool                    writer_ready = false;
    std::string             queued_notice;
    std::string             shown_notice;
    std::string             dispatched_notice;
    Clock::time_point       next_notice_attempt{};
    il2cpp::GcHandle        notice_target;
    std::string             ignored_tag, ignore_edit_tag;
    uint64_t                ignore_revision = 0, release_generation = 1;
  };

  UpdateCheckState& State()
  {
    static UpdateCheckState state;
    return state;
  }

  void OpenRelease(std::string_view tag)
  {
    const std::string url = release_url + std::string(tag);
#if _WIN32
    const std::wstring wide(url.begin(), url.end()); // Fixed HTTPS origin and ASCII tag.
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
      spdlog::warn("[ModUpdates] Could not open the release page");
#elif __APPLE__
    if (!OpenModReleaseMac(url))
      spdlog::warn("[ModUpdates] Could not open the release page");
#endif
  }

  void SelectNotice(Il2CppObject* target, MessageBoxResult choice, const MethodInfo*) noexcept
  {
    auto& state = State();
    if (target != state.notice_target.Get())
      return;
    state.notice_target = {};
    auto tag            = std::exchange(state.shown_notice, {});
    if (!Config::Get().ui.check_for_mod_updates)
      return;
    try {
      if (choice == MessageBoxResult::Left)
        OpenRelease(tag);
      else if (choice == MessageBoxResult::Right && !mod_settings::native::OpenSettingsPage(updates_page))
        EmitDesktopNotification(notice_title, tag + " is available on the project's GitHub Releases page.");
    } catch (...) {
    }
  }

  void MarkNoticeDispatched(UpdateCheckState& state, std::string tag)
  {
    mod_state::QueueUpdateOffer(state_key, Installed()->channel, tag);
    state.dispatched_notice = std::move(tag);
  }

  void DispatchDesktopNotice(UpdateCheckState& state, std::string tag, bool has_updates_page)
  {
    EmitDesktopNotification(notice_title, tag + (has_updates_page ? " is available in Mod Settings → Updates."
                                                                 : " is available on the project's GitHub Releases page."));
    MarkNoticeDispatched(state, std::move(tag));
  }

  void ShowQueuedNotice()
  {
    auto& state = State();
    if (state.queued_notice.empty() || Clock::now() < state.next_notice_attempt)
      return;
    state.next_notice_attempt = Clock::now() + 1s;
    const bool has_updates_page = mod_settings::native::CanOpenSettingsPage(updates_page);
    if (!has_updates_page) {
      DispatchDesktopNotice(state, std::exchange(state.queued_notice, {}), false);
      return;
    }
    auto* sections            = Hub::Sections();
    if (!sections || sections->IsSectionChangeInProgress() || !Hub::IsInSystemOrGalaxyOrStarbase()
        || !unity::Application::isFocused() || Shortcut::CaptureBlocksShortcuts()
        || !ShortcutsManager::CanUseShortcuts())
      return;
    const auto ready = update_notice::Ready();
    if (!ready) {
      DispatchDesktopNotice(state, std::exchange(state.queued_notice, {}), true);
      return;
    }
    if (!*ready)
      return;

    static auto* callback = [] {
      auto*      owned = new mod_settings::NativeCallback<void, MessageBoxResult>;
      const auto donor = update_notice::CallbackTarget::Class::Method<void(MessageBoxResult)>("OnDialogCloseButton");
      owned->Initialize(donor.Info(), SelectNotice, update_notice::IsSelectionType);
      return owned;
    }();
    auto target         = il2cpp::GcHandle::Strong(il2cpp::New<update_notice::CallbackTarget>());
    auto tag            = std::exchange(state.queued_notice, {});
    state.shown_notice  = tag;
    state.notice_target = std::move(target);
    if (update_notice::Show(notice_title, "Version " + tag.substr(1) + " is available.",
                            "View release", "Options", state.notice_target.Get<update_notice::CallbackTarget>(),
                            callback->method())) {
      MarkNoticeDispatched(state, std::move(tag));
      return;
    }
    state.notice_target = {};
    state.shown_notice.clear();
    DispatchDesktopNotice(state, std::move(tag), true);
  }

  CheckResult FetchIndex(const InstalledRelease& installed)
  {
    std::string body;
    body.reserve(max_index_bytes);
    cpr::Session http;
    http.SetUrl(cpr::Url{index_url});
    http.SetUserAgent("STFC Community Mod update check");
    http.SetConnectTimeout(cpr::ConnectTimeout{3000});
    http.SetTimeout(cpr::Timeout{8000});
    http.SetRedirect(cpr::Redirect{0, false});
    http.SetWriteCallback(cpr::WriteCallback{[&body](std::string_view chunk, intptr_t) {
      if (chunk.size() > max_index_bytes - body.size())
        return false;
      body.append(chunk);
      return true;
    }});
    const auto response = http.Get();
    if (response.error || response.status_code != 200)
      return {};
    return CheckIndex(body, installed);
  }

  const nlohmann::json* FindChannelState(const nlohmann::json& state, std::string_view channel)
  {
    const auto checks = state.find(state_key);
    if (checks == state.end() || !checks->is_object())
      return nullptr;
    const auto entry = checks->find(channel);
    return entry != checks->end() && entry->is_object() ? &*entry : nullptr;
  }

  std::chrono::seconds UntilDue(const nlohmann::json& state, std::string_view channel, int64_t now)
  {
    const auto* channel_state = FindChannelState(state, channel);
    if (!channel_state)
      return {};
    const auto last = channel_state->find("last_attempt");
    if (last == channel_state->end() || !last->is_number_integer())
      return {};
    const auto stamp = last->get<int64_t>();
    if (stamp <= 0 || stamp > now)
      return {};
    return std::max(0s, check_interval - std::chrono::seconds(now - stamp));
  }

  nlohmann::json& ChannelState(nlohmann::json& state, std::string_view channel)
  {
    auto& channels = state[state_key];
    if (!channels.is_object())
      channels = nlohmann::json::object();
    auto& entry = channels[std::string(channel)];
    if (!entry.is_object())
      entry = nlohmann::json::object();
    return entry;
  }

  std::string StoredTag(const nlohmann::json& channel, std::string_view key)
  {
    const auto entry = channel.find(key);
    return entry != channel.end() && entry->is_string() ? entry->get<std::string>() : std::string{};
  }

  WorkResult Check(const InstalledRelease& installed, bool automatic, std::stop_token stop,
                   const std::optional<std::string>& session_ignore)
  {
    const auto channel = ChannelKey(installed.channel);
    bool claimed = false;
    if (automatic) {
      const auto now =
          std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      if (const auto saved = mod_state::Read()) {
        const auto remaining = UntilDue(*saved, channel, now);
        if (remaining > 0s)
          return {{}, remaining};
      }

      // Claim the daily attempt before issuing the GET across processes for this profile.
      std::chrono::seconds remaining{};
      claimed = mod_state::Update([&](nlohmann::json& state) {
        remaining = UntilDue(state, channel, now);
        if (remaining == 0s)
          ChannelState(state, channel)["last_attempt"] = now;
      });
      if (remaining > 0s)
        return {{}, remaining};
    }
    if (stop.stop_requested())
      return {};

    auto        result  = FetchIndex(installed);
    bool        notify  = false;
    std::string ignored = session_ignore.value_or("");
    if (!stop.stop_requested()) {
      const auto  saved = mod_state::Read();
      const auto* entry = saved ? FindChannelState(*saved, channel) : nullptr;
      if (entry) {
        if (!session_ignore)
          ignored = StoredTag(*entry, "ignored_tag");
        notify = claimed && result.status == CheckStatus::Available && !IsIgnoredUpdate(result, ignored)
                 && IsNewerReleaseTag(result.tag, StoredTag(*entry, "last_offered_tag"), installed.channel);
      }
    }
    return {std::move(result), 0s, notify, std::move(ignored)};
  }

  void Start(bool automatic)
  {
    auto& state = State();
    if (state.pending.valid() || !Installed())
      return;
    state.next_check = Clock::now() + check_interval;
    try {
      // A check can start while the background writer still owns the latest toggle edit.
      const auto save           = mod_state::Status(mod_state::Group::UpdateIgnore);
      auto       session_ignore = state.ignore_revision && save.revision == state.ignore_revision
                                          && save.outcome != mod_state::SaveOutcome::Saved
                                      ? std::optional{state.ignored_tag}
                                      : std::nullopt;
      state.stop                = automatic ? std::stop_source{} : std::stop_source{std::nostopstate};
      std::packaged_task<WorkResult()> task(
          [installed = *Installed(), automatic, stop = state.stop.get_token(),
           session_ignore = std::move(session_ignore)] { return Check(installed, automatic, stop, session_ignore); });
      auto pending = task.get_future();
      std::thread(std::move(task)).detach();
      state.pending = std::move(pending);
    } catch (const std::system_error& error) {
      state.result = {};
      spdlog::warn("[ModUpdates] Could not start the update check: {}", error.what());
    }
    mod_settings::native::RequestPageRefresh();
  }

  void UpdateCheckFrame()
  {
    auto&      state   = State();
    const bool enabled = Config::Get().ui.check_for_mod_updates;
    if (state.pending.valid()) {
      if (!enabled)
        state.stop.request_stop();
      if (state.pending.wait_for(0s) != std::future_status::ready)
        return;
      try {
        auto completed = state.pending.get();
        if (completed.retry_after > 0s)
          state.next_check = Clock::now() + completed.retry_after;
        else if (!state.stop.stop_requested()) {
          state.result      = std::move(completed.result);
          state.ignored_tag = std::move(completed.ignored_tag);
          ++state.release_generation;
          if (completed.notify && IsNewerReleaseTag(state.result.tag, state.dispatched_notice, Installed()->channel))
            state.queued_notice = state.result.tag;
          else if (state.result.status != CheckStatus::Failed
                   && (state.queued_notice != state.result.tag || IsIgnoredUpdate(state.result, state.ignored_tag)))
            state.queued_notice.clear();
        }
      } catch (const std::exception& error) {
        if (!state.stop.stop_requested())
          state.result = {};
        spdlog::warn("[ModUpdates] Update check failed: {}", error.what());
      }
      mod_settings::native::RequestPageRefresh();
    }
    if (enabled && Clock::now() >= state.next_check) {
      InitializeDesktopNotifications();
      Start(true);
    }
    if (enabled)
      ShowQueuedNotice();
    else {
      state.queued_notice.clear();
      state.notice_target = {};
      state.shown_notice.clear();
    }
  }
} // namespace

void Prepare()
{
  if (Installed()) {
    State().writer_ready = mod_state::PrepareAsync();
    State().registered = RegisterFrameCallback(UpdateCheckFrame, "ModUpdates");
  }
}

bool Available()
{ return State().registered && FrameDeliveryAvailable(); }

bool Checking()
{ return State().pending.valid(); }

void CheckNow()
{
  if (Available())
    Start(false);
}

std::string_view InstalledVersion()
{ return build_identity::DisplayVersion(); }

std::string Status()
{
  if (!Installed())
    return "No published update channel";
  if (!Available())
    return "Update checks unavailable";
  if (Checking())
    return "Checking…";
  switch (State().result.status) {
    case CheckStatus::Current:
      return "Up to date";
    case CheckStatus::Available:
      return "Update available · " + State().result.tag.substr(1);
    case CheckStatus::Failed:
      return "Couldn’t check";
    default:
      return {};
  }
}

bool HasRelease()
{ return !Checking() && State().result.status == CheckStatus::Available; }

void ViewRelease()
{
  if (Available() && HasRelease())
    OpenRelease(State().result.tag);
}
bool Ignored()
{ return HasRelease() && IsIgnoredUpdate(State().result, State().ignored_tag); }

bool IgnoreAvailable()
{ return HasRelease() && State().writer_ready; }

uint64_t ReleaseGeneration()
{ return State().release_generation; }

bool SetIgnored(bool value, uint64_t generation)
{
  auto& state = State();
  if (!IgnoreAvailable() || generation != state.release_generation || !Installed())
    return false;
  auto       tag      = value ? state.result.tag : std::string{};
  const auto revision = mod_state::QueueUpdateIgnore(state_key, Installed()->channel, tag);
  if (!revision)
    return false;
  state.ignore_revision = revision;
  state.ignore_edit_tag = state.result.tag;
  state.ignored_tag     = std::move(tag);
  if (value && state.queued_notice == state.result.tag)
    state.queued_notice.clear();
  mod_settings::native::RequestPageRefresh();
  return true;
}

mod_settings::SaveStatus IgnoreSaveStatus()
{
  const auto& state = State();
  if (!HasRelease() || state.ignore_edit_tag != state.result.tag)
    return {};
  const auto save = mod_state::Status(mod_state::Group::UpdateIgnore);
  if (save.revision != state.ignore_revision)
    return {};
  switch (save.outcome) {
    case mod_state::SaveOutcome::Pending:
      return {mod_settings::SaveState::Pending, save.revision};
    case mod_state::SaveOutcome::Saved:
      return {mod_settings::SaveState::Saved, save.revision};
    case mod_state::SaveOutcome::Failed:
      return {mod_settings::SaveState::Failed, save.revision};
    default:
      return {};
  }
}
} // namespace mod_updates
