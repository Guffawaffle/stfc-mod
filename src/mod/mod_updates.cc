// Downstream package discovery and [ui] check_for_mod_updates.
#include "mod_updates.h"
#include "build_identity.h"
#include "config.h"
#include "features/notifications.h"
#include "frame.h"
#include "mod_state.h"
#include "release_index.h"
#include "settings/native_controls.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>
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

  // Local signed smoke build. Use the exact draft PR index and a known older Pre version for comparison.
  constexpr auto index_url      = "https://raw.githubusercontent.com/Guffawaffle/stfc-mod/"
                                  "c0fffc6a37f640209d3f46e1edbf9b7a5034bfa0/releases/update-index.json";
  constexpr auto release_url    = "https://github.com/Guffawaffle/stfc-mod/releases/tag/";
  constexpr auto check_interval = 24h;
  constexpr auto startup_delay  = 30s;
  constexpr auto state_key      = "mod_update_smoke_checks";

  struct WorkResult {
    CheckResult          result;
    std::chrono::seconds retry_after{};
    bool                 notify = false;
  };

  const auto& Installed()
  {
    static const auto installed = [] {
      const auto display = build_identity::DisplayVersion();
      if (display.contains(" · Pre b"))
        return IdentifyRelease("1.1.9.5-guffa.6 · Pre");
      return IdentifyRelease(display);
    }();
    return installed;
  }

  struct UpdateCheckState {
    Clock::time_point       next_check = Clock::now() + startup_delay;
    std::future<WorkResult> pending;
    std::stop_source        stop{std::nostopstate};
    CheckResult             result{CheckStatus::Idle, {}};
    bool                    registered = false;
  };

  UpdateCheckState& State()
  {
    static UpdateCheckState state;
    return state;
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

  std::chrono::seconds UntilDue(const nlohmann::json& state, std::string_view channel, int64_t now)
  {
    const auto checks = state.find(state_key);
    if (checks == state.end() || !checks->is_object())
      return {};
    const auto channel_state = checks->find(channel);
    if (channel_state == checks->end() || !channel_state->is_object())
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

  WorkResult Check(const InstalledRelease& installed, bool automatic, std::stop_token stop)
  {
    if (!automatic)
      return {FetchIndex(installed)};

    const auto channel = ChannelKey(installed.channel);
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (const auto saved = mod_state::Read()) {
      const auto remaining = UntilDue(*saved, channel, now);
      if (remaining > 0s)
        return {{}, remaining};
    }

    // Claim the daily attempt before issuing the GET. Other game processes for this profile
    // see the same claim even when the network is slow or unavailable.
    std::chrono::seconds remaining{};
    const bool           saved = mod_state::Update([&](nlohmann::json& state) {
      remaining = UntilDue(state, channel, now);
      if (remaining == 0s)
        ChannelState(state, channel)["last_attempt"] = now;
    });
    if (remaining > 0s)
      return {{}, remaining};
    if (stop.stop_requested())
      return {};

    auto result = FetchIndex(installed);
    bool notify = false;
    if (saved && result.status == CheckStatus::Available && !stop.stop_requested()) {
      const bool recorded = mod_state::Update([&](nlohmann::json& state) {
        if (stop.stop_requested())
          return;
        auto&      entry        = ChannelState(state, channel);
        const auto last         = entry.find("last_offered_tag");
        const auto last_version = last != entry.end() && last->is_string()
                                      ? ParseReleaseTag(last->get_ref<const std::string&>(), installed.channel)
                                      : std::nullopt;
        const auto available    = ParseReleaseTag(result.tag, installed.channel);
        if (available && (!last_version || *available > *last_version)) {
          entry["last_offered_tag"] = result.tag;
          notify                    = true;
        }
      });
      notify              = recorded && notify;
    }
    return {std::move(result), 0s, notify};
  }

  void Start(bool automatic)
  {
    auto& state = State();
    if (state.pending.valid() || !Installed())
      return;
    state.next_check = Clock::now() + check_interval;
    try {
      state.stop = automatic ? std::stop_source{} : std::stop_source{std::nostopstate};
      std::packaged_task<WorkResult()> task([installed = *Installed(), automatic, stop = state.stop.get_token()] {
        return Check(installed, automatic, stop);
      });
      auto                             pending = task.get_future();
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
          state.result = std::move(completed.result);
          if (completed.notify)
            EmitDesktopNotification("STFC Community Mod update available",
                                    state.result.tag + " is available in Mod Settings → Updates.");
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
  }
} // namespace

void Prepare()
{
  if (Installed())
    State().registered = RegisterFrameCallback(UpdateCheckFrame, "ModUpdates");
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
{
  static const std::string label = std::string(build_identity::DisplayVersion()) + " · Update smoke";
  return label;
}

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
  if (!Available() || !HasRelease())
    return;
  const std::string url = release_url + State().result.tag;
#if _WIN32
  const std::wstring wide(url.begin(), url.end()); // Fixed HTTPS origin and ASCII tag.
  if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
    spdlog::warn("[ModUpdates] Could not open the release page");
#elif __APPLE__
  if (!OpenModReleaseMac(url))
    spdlog::warn("[ModUpdates] Could not open the release page");
#endif
}
} // namespace mod_updates
