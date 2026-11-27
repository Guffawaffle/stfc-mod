// Downstream package discovery and [ui] check_for_mod_updates.
#include "mod_updates.h"
#include "build_identity.h"
#include "community_badge.h"
#include "config.h"
#include "frame.h"
#include "game/hud.h"
#include "game/player_profile.h"
#include "mod_state.h"
#include "release_index.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <optional>
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

  constexpr auto index_url   = "https://raw.githubusercontent.com/Guffawaffle/stfc-mod/play/releases/update-index-v2.json";
  constexpr auto release_url = "https://github.com/Guffawaffle/stfc-mod/releases/tag/";
  constexpr auto check_interval = 24h;
  constexpr auto startup_delay  = 30s;
  constexpr auto state_key      = "mod_update_checks";
  constexpr auto settings_page  = "community_mod.settings";
  constexpr auto updates_page   = "community_mod.updates";
  constexpr auto available_key  = "last_available_tag";

  struct WorkResult {
    CheckResult          result;
    std::chrono::seconds retry_after{};
    std::string          ignored_tag{};
    CheckResult          cached{CheckStatus::Idle, {}};
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
    bool                    check_failed = false;
    std::string             ignored_tag, ignore_edit_tag;
    uint64_t                ignore_revision = 0, release_generation = 1;
  };

  UpdateCheckState& State()
  {
    static UpdateCheckState state;
    return state;
  }

  bool UpdatePending()
  {
    const auto& state = State();
    return state.result.status == CheckStatus::Available && !IsIgnoredUpdate(state.result, state.ignored_tag);
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

  void OpenProfileModSettings(Il2CppObject*, const MethodInfo*) noexcept
  {
    try {
      if (Hub::CurrentSection() == SectionID::PlayerProfile)
        mod_settings::native::OpenSettingsPage(UpdatePending() ? updates_page : settings_page);
    } catch (...) {
    }
  }

  bool ConnectProfileButton(unity::GameObject* object, unity::Image* image, PlayerProfileViewController* owner)
  {
    static auto* callback = [] {
      auto* result = new mod_settings::NativeCallback<void>;
      result->Initialize(PlayerProfileViewController::Class::Method<void()>("AvatarEditButtonOnClickHandler").Info(),
                         OpenProfileModSettings);
      return result;
    }();
    auto* button = object->AddComponent<unity::Button>();
    auto* event  = button ? button->onClick() : nullptr;
    if (!event || !callback->method() || !button->TryCall<"set_targetGraphic">(image))
      return false;
    auto listener = il2cpp::GcHandle::Strong(il2cpp::NewClosedDelegate(
        unity::UnityAction::Class::Raw(), owner, il2cpp::Method(callback->method())));
    return listener && event->AddListener(listener.Get<unity::UnityAction>());
  }

  // A four-image glyph avoids font lookups on the HUD and matches the small cyan pip on both surfaces.
  unity::GameObject* AddUpdatePip(unity::RectTransform* badge, float size)
  {
    il2cpp::GcHandle pip;
    auto* rect   = unity::CreateUiObject("Community Mod update available", badge, pip);
    auto* object = pip.Get<unity::GameObject>();
    auto* image  = rect && object ? object->AddComponent<unity::Image>() : nullptr;
    if (!image) {
      if (object)
        unity::Destroy(object);
      return nullptr;
    }
    rect->Place({1, 1}, {1, 1}, {.7f, .7f}, {size, size}, {0, 0});
    image->SetColor({.08f, .32f, .43f, 1});
    image->SetRaycastTarget(false);
    for (const auto& [angle, x, y, extent] :
         std::array{std::array{0.f, 0.f, -.08f, .53f}, std::array{-45.f, -.15f, .05f, .40f},
                    std::array{45.f, .15f, .05f, .40f}}) {
      il2cpp::GcHandle stroke;
      auto* stroke_rect  = unity::CreateUiObject("Arrow", rect, stroke);
      auto* stroke_child = stroke.Get<unity::GameObject>();
      auto* stroke_image = stroke_rect && stroke_child ? stroke_child->AddComponent<unity::Image>() : nullptr;
      if (!stroke_image) {
        if (stroke_child)
          unity::Destroy(stroke_child);
        return nullptr;
      }
      stroke_rect->Place({.5f, .5f}, {.5f, .5f}, {.5f, .5f}, {size * .13f, size * extent}, {size * x, size * y});
      stroke_rect->Set<"localEulerAngles">(unity::Vector3{0, 0, angle});
      stroke_image->SetColor({.89f, .98f, 1, 1});
      stroke_image->SetRaycastTarget(false);
      stroke_child->SetActive(true);
    }
    return object;
  }

  class UpdateBadge {
  public:
    bool HasActiveAnchor() const
    {
      auto* object    = object_.Get<unity::GameObject>();
      auto* transform = unity::Alive(object) ? object->transform() : nullptr;
      auto* anchor    = transform ? transform->parent() : nullptr;
      auto* parent    = anchor ? anchor->gameObject() : nullptr;
      return unity::Alive(parent) && parent->activeInHierarchy();
    }

    void Clear()
    {
      auto* object = object_.Get<unity::GameObject>();
      if (unity::Alive(object)) {
        object->SetActive(false);
        unity::Destroy(object);
      }
      object_.Reset();
      pip_.Reset();
    }

    void Show(bool visible, bool pending)
    {
      SetActive(object_, visible);
      SetActive(pip_, pending);
    }

    void Create(unity::Component* anchor, PlayerProfileViewController* owner)
    {
      auto* anchor_object = unity::Alive(anchor) ? anchor->gameObject() : nullptr;
      if (!unity::Alive(anchor_object) || !anchor_object->activeInHierarchy())
        return;
      auto* parent = anchor->transform();
      auto* bounds = parent ? parent->As<unity::RectTransform>() : nullptr;
      const auto rect = bounds ? bounds->TryCall<"get_rect", unity::Rect>() : il2cpp::CallResult<unity::Rect>{};
      const auto size = rect ? std::min(rect.value.width, rect.value.height) : 0;
      if (!std::isfinite(size) || size <= 0)
        return;
      auto* sprite = CommunityBadgeSprite();
      if (!sprite)
        return;
      il2cpp::GcHandle root;
      auto* layout = unity::CreateUiObject(owner ? "Mod Settings" : "Community Mod update available", parent, root);
      auto* object = root.Get<unity::GameObject>();
      auto* image  = layout && object ? object->AddComponent<unity::Image>() : nullptr;
      if (!image) {
        if (object)
          unity::Destroy(object);
        return;
      }
      const float side = owner ? size : size * .27f;
      const auto corner = owner ? unity::Vector2{1, .5f} : unity::Vector2{1, 0};
      layout->Place(corner, corner, owner ? unity::Vector2{0, .5f} : unity::Vector2{1, 0}, {side, side},
                    owner ? unity::Vector2{size * .48f, 0} : unity::Vector2{-size * .055f, size * .055f});
      image->SetSprite(sprite);
      image->SetPreserveAspect(true);
      image->SetRaycastTarget(owner != nullptr);
      auto* pip = AddUpdatePip(layout, side * .40f);
      if (!pip || (owner && !ConnectProfileButton(object, image, owner))) {
        unity::Destroy(object);
        return;
      }
      object_ = il2cpp::GcHandle::Weak(object);
      pip_    = il2cpp::GcHandle::Weak(pip);
      if (!object_ || !pip_) {
        unity::Destroy(object);
        object_.Reset();
        pip_.Reset();
      }
    }

  private:
    static void SetActive(const il2cpp::GcHandle& handle, bool visible)
    {
      auto* object = handle.Get<unity::GameObject>();
      if (unity::Alive(object) && object->Get<"activeSelf", bool>() != visible)
        object->SetActive(visible);
    }

    il2cpp::GcHandle object_, pip_;
  };

  unity::RectTransform* ProfileRect(il2cpp::Object* component)
  {
    if (!unity::Alive(component))
      return nullptr;
    const auto transform = component->TryCall<"get_transform", unity::Transform*>();
    return transform && transform.value ? transform.value->As<unity::RectTransform>() : nullptr;
  }

  class ProfileHorizontalAlignment
  {
  public:
    bool Align(unity::RectTransform* target, unity::RectTransform* reference, unity::RectTransform* away_from = nullptr)
    {
      auto* space = unity::Alive(target) ? target->parent() : nullptr;
      if ((!target || target_.Get() != target || parent_.Get() != space) && !Restore())
        return false;
      if (!unity::Alive(space))
        return false;
      const auto target_bounds = unity::RectTransformUtility::OwnBounds(space, target);
      const auto name_bounds   = unity::RectTransformUtility::OwnBounds(space, reference);
      if (!target_bounds || !name_bounds || target_bounds.value.extents.x <= 0 || name_bounds.value.extents.x <= 0)
        return false;
      const float target_x = target_bounds.value.center.x;
      const float shift    = name_bounds.value.center.x - target_x;
      if (!std::isfinite(shift))
        return false;
      if (std::abs(shift) < .25f)
        return true;

      if (away_from) {
        const auto avoid_bounds = unity::RectTransformUtility::OwnBounds(space, away_from);
        if (!avoid_bounds || shift * (avoid_bounds.value.center.x - target_x) >= 0)
          return false;
      }
      const auto position = target->TryCall<"get_anchoredPosition", unity::Vector2>();
      if (!position || !std::isfinite(position.value.x) || !std::isfinite(position.value.y))
        return false;
      auto       tracked  = il2cpp::GcHandle::Weak(target);
      auto       parent   = il2cpp::GcHandle::Weak(space);
      const auto adjusted = unity::Vector2{position.value.x + shift, position.value.y};
      if (!tracked || !parent || !target->TryCall<"set_anchoredPosition", void(unity::Vector2)>(adjusted))
        return false;
      if (!target_ || position.value.x != applied_x_)
        original_x_ = position.value.x;
      applied_x_ = adjusted.x;
      target_    = std::move(tracked);
      parent_    = std::move(parent);
      return true;
    }

    bool Restore()
    {
      auto* target = target_.Get<unity::RectTransform>();
      auto* parent = parent_.Get<unity::Transform>();
      if (unity::Alive(target) && unity::Alive(parent) && target->parent() == parent) {
        auto position = target->TryCall<"get_anchoredPosition", unity::Vector2>();
        if (!position)
          return false;
        // Another profile can reuse this widget. Restore only the x value we still own.
        if (position.value.x == applied_x_) {
          position.value.x = original_x_;
          if (!target->TryCall<"set_anchoredPosition", void(unity::Vector2)>(position.value))
            return false;
        }
      }
      target_.Reset();
      parent_.Reset();
      return true;
    }

  private:
    il2cpp::GcHandle target_, parent_;
    float            original_x_ = 0, applied_x_ = 0;
  };

  class ProfileAvatarAlignment
  {
  public:
    void Refresh(PlayerProfileViewController* owner)
    {
      const auto local  = unity::Alive(owner) ? owner->TryIsLocalProfile() : il2cpp::CallResult<bool>{};
      auto*      info   = local && local.value ? owner->playerInfoWidget() : nullptr;
      auto*      avatar = unity::Alive(info) ? ProfileRect(info->profileFrameAndAvatar()) : nullptr;
      auto*      name   = unity::Alive(info) ? ProfileRect(info->nameLabel()) : nullptr;
      auto*      edit   = avatar ? ProfileRect(owner->avatarEditButton()) : nullptr;
      if (!avatar || !edit) {
        avatar_.Restore();
        background_.Restore();
        return;
      }
      // Keep the native edit button and its Mod Settings badge in place.
      if (!avatar_.Align(avatar, name, edit))
        return;
      auto* details = ProfileRect(info);
      // The decorative rings are a sibling of AvatarContainer, not part of AvatarWidget.
      auto* icon = details ? details->Find("PlayerBG/Icon") : nullptr;
      background_.Align(unity::Alive(icon) ? icon->As<unity::RectTransform>() : nullptr, name);
    }

  private:
    ProfileHorizontalAlignment avatar_, background_;
  };

  void RefreshHudAndProfile(bool pending)
  {
    static auto* hud       = new UpdateBadge;
    static auto* profile   = new UpdateBadge;
    static auto* alignment = new ProfileAvatarAlignment;
    static auto  next      = Clock::time_point{};
    const auto   now       = Clock::now();
    if (now < next)
      return;
    next                       = now + 300ms;
    const bool can_open        = mod_settings::native::CanOpenSettingsPage(settings_page);
    const bool in_profile      = Hub::CurrentSection() == SectionID::PlayerProfile;
    const bool profile_section = can_open && in_profile;
    const bool hud_visible     = can_open && pending && Hub::IsInSystemOrGalaxyOrStarbase();
    if (hud_visible && !hud->HasActiveAnchor()) {
      hud->Clear();
      auto* drawer = ScreenManager::RegisteredController<HudDrawerViewController, HudDrawerContext>();
      if (drawer)
        hud->Create(drawer->profileFrameAndAvatar(), nullptr);
    }
    auto* owner = in_profile
                      ? ScreenManager::RegisteredController<PlayerProfileViewController, PlayerProfileDataContext>()
                      : nullptr;
    alignment->Refresh(owner);
    if (in_profile) {
      if (profile_section && !profile->HasActiveAnchor()) {
        profile->Clear();
        if (owner)
          profile->Create(owner->avatarEditButton(), owner);
      }
    }
    hud->Show(hud_visible, hud_visible);
    profile->Show(profile_section, pending);
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

  WorkResult ReadCheckState(const InstalledRelease& installed, const std::optional<std::string>& session_ignore)
  {
    WorkResult work{{}, 0s, session_ignore.value_or("")};
    if (const auto snapshot = mod_state::Read()) {
      if (const auto* channel = FindChannelState(*snapshot, ChannelKey(installed.channel))) {
        if (!session_ignore)
          work.ignored_tag = StoredTag(*channel, "ignored_tag");
        const auto entry = channel->find(available_key);
        if (entry != channel->end() && entry->is_string())
          work.cached = RestoreCheck(entry->get_ref<const std::string&>(), installed);
      }
    }
    return work;
  }

  WorkResult Check(const InstalledRelease& installed, bool automatic, std::stop_token stop,
                   const std::optional<std::string>& session_ignore)
  {
    const auto channel = ChannelKey(installed.channel);
    if (automatic) {
      const auto now =
          std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      std::chrono::seconds remaining{};
      if (const auto saved = mod_state::Read()) {
        remaining = UntilDue(*saved, channel, now);
      }

      // Claim the daily attempt before issuing the GET across processes for this profile.
      if (remaining == 0s) {
        mod_state::Update([&](nlohmann::json& state) {
          remaining = UntilDue(state, channel, now);
          if (remaining == 0s)
            ChannelState(state, channel)["last_attempt"] = now;
        });
      }
      if (remaining > 0s) {
        auto work       = ReadCheckState(installed, session_ignore);
        work.retry_after = remaining;
        work.result      = work.cached;
        return work;
      }
    }
    if (stop.stop_requested())
      return {};

    auto result = FetchIndex(installed);
    if (stop.stop_requested())
      return {};
    // Keep the most recent successful result so the badge can return after a restart without another GET.
    if (result.status != CheckStatus::Failed)
      mod_state::Update([&](nlohmann::json& state) {
        if (!stop.stop_requested())
          ChannelState(state, channel)[available_key] = result.tag;
      });
    auto work   = ReadCheckState(installed, session_ignore);
    work.result = std::move(result);
    return work;
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
      state.check_failed = true;
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
      if (state.pending.wait_for(0s) == std::future_status::ready) {
        try {
          auto completed = state.pending.get();
          if (!state.stop.stop_requested()) {
            if (completed.retry_after > 0s)
              state.next_check = Clock::now() + completed.retry_after;
            state.check_failed = completed.result.status == CheckStatus::Failed;
            if (completed.result.status == CheckStatus::Available || completed.result.status == CheckStatus::Current)
              state.result = std::move(completed.result);
            else if (state.result.status == CheckStatus::Idle)
              state.result = std::move(completed.cached);
            state.ignored_tag = std::move(completed.ignored_tag);
            ++state.release_generation;
          }
        } catch (const std::exception& error) {
          if (!state.stop.stop_requested())
            state.check_failed = true;
          spdlog::warn("[ModUpdates] Update check failed: {}", error.what());
        }
        mod_settings::native::RequestPageRefresh();
      }
    }
    if (enabled && Installed() && Clock::now() >= state.next_check)
      Start(true);
    RefreshHudAndProfile(UpdatePending());
  }
} // namespace

void Prepare()
{
  if (Installed())
    State().writer_ready = mod_state::PrepareAsync();
  State().registered = RegisterFrameCallback(UpdateCheckFrame, "ModUpdates");
}

bool Available()
{ return Installed() && State().registered && FrameDeliveryAvailable(); }

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
  if (State().check_failed)
    return State().result.status == CheckStatus::Available
               ? "Couldn’t check · last seen " + State().result.tag.substr(1)
               : "Couldn’t check";
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
