// Chat: the shortcuts that open it and switch channel; [ui] disable_galaxy_chat and disable_veil_chat, which grey
// out those tabs in full-screen chat, keep the chat preview off them, and drop their messages from the preview;
// [ui] show_player_title, which can hide the title row under each sender's name; and [ui] auto_open_chat and
// default_chat_width, which open alliance side chat and set its starting width once per login.
// Chat portrait retention keeps downloaded avatars available when chat rows are reused.
// Streamer Mode independently masks chat names and alliance tags, including the selected personal protections.

#include "config.h"
#include "features/chat_startup.h"
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"
#include "shortcuts.h"

#include "game/chat.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/core.h>
#include <unity/input.h>
#include <unity/tracker.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
// --- Side chat on login ---

// Explicit live resets release this weak owner; static teardown must not call a stopped runtime on Windows.
auto&             chat_preview = *new il2cpp::GcHandle;
ChatStartup::Gate chat_startup;
bool              chat_width_applied = false;

void ObserveChatPreview(ChatPreviewController* preview)
{
  if (!Config::Get().ui.auto_open_chat || chat_preview.Get() == preview)
    return;
  chat_preview             = il2cpp::GcHandle::Weak(preview);
  chat_startup.ready_since = -1;
}

void ChatFrame()
{
  const auto& ui           = Config::Get().ui;
  const bool  custom_width = ui.default_chat_width > 0;
  if (!ui.auto_open_chat && !custom_width)
    return;
  auto sections = il2cpp::GcHandle::Strong(Hub::Sections());
  if (!sections)
    return;
  const auto section = sections.Get<SectionManager>()->CurrentSection();
  if (section == SectionID::Login || section == SectionID::AppInit) {
    chat_startup       = {};
    chat_width_applied = false;
    // The preview may initialize during login. Retain its weak observation until the real readiness gate opens.
    return;
  }
  // Once both requests are complete, only the section observation above remains for the next login boundary.
  if ((!ui.auto_open_chat || chat_startup.completed) && (!custom_width || chat_width_applied))
    return;
  auto  manager = il2cpp::GcHandle::Strong(ChatManager::Existing());
  auto* chat    = manager.Get<ChatManager>();
  if (!chat)
    return;
  const bool side_open = chat->IsSideChatOpen();
  if (side_open && custom_width && !chat_width_applied) {
    auto screens = il2cpp::GcHandle::Strong(ScreenManager::Existing());
    auto frames  = il2cpp::GcHandle::Strong(screens ? screens.Get<ScreenManager>()->frameManager() : nullptr);
    if (frames)
      chat_width_applied = frames.Get<UIFrameManager>()->ResizeSideFrame(ui.default_chat_width);
  }
  if (!ui.auto_open_chat || chat_startup.completed)
    return;
  auto         preview = il2cpp::GcHandle::Strong(chat_preview.Get());
  const bool   station = section == SectionID::Starbase_Interior || section == SectionID::Starbase_Exterior;
  const bool   ready   = station && preview && preview.Get<ChatPreviewController>()->isSectionChangedCompleted()
                         && chat->isContextInitialised() && chat->IsSideChatAllowed();
  const double now     = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if (chat_startup.Tick(ready, side_open || Hub::IsInChat(), now)) {
    spdlog::info("[Chat] readiness settled; opening alliance side chat");
    chat->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Side);
  }
}

#ifdef _MODDBG
struct ChatSubmitTrace {
  int32_t                  frame   = -1;
  bool                     sampled = false, input_acquired = false;
  il2cpp::CallResult<bool> enabled, selected, alive, focused;
};

ChatSubmitTrace ObserveChatSubmit(ChatMessageListLocalViewController* receiver) noexcept
{
  ChatSubmitTrace state;
  try {
    if (spdlog::should_log(spdlog::level::debug)) {
      state.sampled = true;
      state.frame   = unity::Time::frameCount();
      auto source   = il2cpp::GcHandle::Strong(receiver);
      if (source) {
        auto* owner          = source.Get<ChatMessageListLocalViewController>();
        state.enabled        = owner->TryFocusOnTypeEnabled();
        state.selected       = owner->TryInputFieldSelected();
        auto input           = il2cpp::GcHandle::Strong(owner->inputField());
        state.input_acquired = bool(input);
        state.alive          = input ? unity::TryAlive(input.Get()) : il2cpp::CallResult<bool>{};
        if (state.alive && state.alive.value)
          state.focused = input.Get<unity::TMP_InputField>()->TryCall<"get_isFocused", bool>();
      }
    }
  } catch (...) {
    // Trace availability never decides whether the separate repair or native callback may run.
  }
  return state;
}

void ReportChatSubmit(const ChatSubmitTrace& state, bool submit_down, bool capture, bool reconciled) noexcept
{
  try {
    if (state.sampled)
      spdlog::debug("[Chat] submit-state phase=pre_native_callback frame={} native_enable_known={} native_enabled={} "
                    "native_selected_known={} native_selected={} input_acquired={} input_alive_known={} "
                    "input_alive={} text_focus_known={} text_focused={} submit_down={} capture={} reconciled={}",
                    state.frame, state.enabled.succeeded, state.enabled && state.enabled.value,
                    state.selected.succeeded, state.selected && state.selected.value, state.input_acquired,
                    state.alive.succeeded, state.alive && state.alive.value, state.focused.succeeded,
                    state.focused && state.focused.value, submit_down, capture, reconciled);
  } catch (...) {
    // No logger failure may suppress, retry or undo the native submit or a completed repair.
  }
}
#endif

void ChatMessageListLocalViewController_OnSubmit_Hook(auto original, ChatMessageListLocalViewController* _this,
                                                      Il2CppString* text)
{
  bool submit_down = false, capture = true, keys_observed = false, reconciled = false;
  try {
    unity::Key::Poll();
    // Poll can leave an old cache when frame-count observation is unavailable. Query the engine's current edge
    // directly, matching the native submit gate, rather than treating an old cached Return as this user's intent.
    submit_down =
        unity::Input::GetKeyDown(unity::KeyCode::Return) || unity::Input::GetKeyDown(unity::KeyCode::KeypadEnter);
    capture       = Shortcut::CaptureBlocksShortcuts();
    keys_observed = true;
    // Native submit can clear text selection before the popup's keyboard-confirm callback runs. Preserve this
    // frame's text ownership so that the same Enter does not also confirm the window behind side chat.
    if (submit_down)
      (void)unity::Key::IsInputFocused();
  } catch (...) {
  }
#ifdef _MODDBG
  const auto trace = ObserveChatSubmit(_this);
#endif
  try {
    reconciled = chat_submit_binding::ReconcileSelection(_this, keys_observed && submit_down && !capture);
  } catch (...) {
    // The optional correction never replaces the game's callback or retries a failed submission.
  }
#ifdef _MODDBG
  ReportChatSubmit(trace, submit_down, capture, reconciled);
#endif
  original(_this, text);
}

// --- Chat portrait retention ---

void ChatMessageWidget_SetWidgetData_Hook(auto original, ChatMessageWidget* _this)
{
  bool retained = false;
  try {
    retained = _this->RetainAvatars();
  } catch (...) {
    // Optional portrait bookkeeping must never suppress or retry the native row binding.
  }
  if (!retained) {
    static bool reported = false;
    if (!std::exchange(reported, true)) {
      try {
        spdlog::warn("[ChatAvatarRetention] portrait retention unavailable for this row; native binding retained");
      } catch (...) {
      }
    }
  }
  original(_this);
}

// --- Disabled channels ---

// Tab positions in both the full-screen chat and the preview. Cadet chat, first, and veil chat, after galaxy,
// only exist for some players and shift the tabs after them; a missing veil tab is -1.
struct ChatTabs {
  int32_t galaxy   = 0;
  int32_t veil     = -1;
  int32_t alliance = 1;

  static std::optional<ChatTabs> Current()
  {
    auto  root = il2cpp::GcHandle::Strong(ChatManager::Instance());
    auto* chat = root.Get<ChatManager>();
    if (!chat)
      return std::nullopt;
    const auto cadet = chat->TryCall<"CanSeeNewbieChat", bool()>();
    const auto veil  = chat->TryCall<"CanSeeRegionalChat", bool()>();
    if (!cadet || !veil)
      return std::nullopt; // Unknown availability cannot identify a tab to disable.
    const int32_t galaxy = cadet.value ? 1 : 0;
    return ChatTabs{.galaxy = galaxy, .veil = veil.value ? galaxy + 1 : -1, .alliance = galaxy + (veil.value ? 2 : 1)};
  }

  // Whether `index` is a tab the settings disable.
  bool Disabled(int32_t index) const
  {
    const auto& ui = Config::Get().ui;
    return (ui.disable_galaxy_chat && index == galaxy) || (ui.disable_veil_chat && veil != -1 && index == veil);
  }
};

void FullScreenChatViewController_AboutToShow_Hook(auto original, FullScreenChatViewController* _this)
{
  original(_this);
  const auto& ui = Config::Get().ui;
  if (!ui.disable_galaxy_chat && !ui.disable_veil_chat)
    return;

  const auto tabs = ChatTabs::Current();
  if (!tabs)
    return;
  auto view    = il2cpp::GcHandle::Strong(_this->categoriesTabBarViewController());
  auto bar     = il2cpp::GcHandle::Strong(view ? view.Get<TabBarViewController>()->tabBar() : nullptr);
  auto buttons = il2cpp::GcHandle::Strong(bar ? bar.Get<TabBar>()->data() : nullptr);
  for (const auto index : {tabs->galaxy, tabs->veil}) {
    auto button = il2cpp::GcHandle::Strong(
        tabs->Disabled(index) ? il2cpp::At(buttons.Get<il2cpp::List<il2cpp::Object*>>(), index) : nullptr);
    if (auto* context = button ? button.Get<il2cpp::Object>()->As<GenericButtonContext>() : nullptr)
      context->SetInteractable(false);
  }
}

// Selecting a disabled tab (with the keyboard, say) does nothing.
void FullScreenChatViewController_OnDidChangeSelectedTab_Hook(auto original, FullScreenChatViewController* _this,
                                                              int32_t index, void* tab)
{
  const auto tabs = ChatTabs::Current();
  if (!tabs || !tabs->Disabled(index))
    original(_this, index, tab);
}

// Lines the preview's scroller up with the alliance panel.
void FocusAlliance(ChatPreviewController* preview, int32_t alliance)
{
  auto root = il2cpp::GcHandle::Strong(preview->swipeScroller());
  if (auto* scroller = root.Get<SwipeScroller>(); scroller && scroller->currentContentIndex() != alliance)
    scroller->FocusOnInstantly(alliance);
}

void ChatPreviewController_AboutToShow_Hook(auto original, ChatPreviewController* _this)
{
  original(_this);
  ObserveChatPreview(_this);
  // The preview reopens on the last panel; move off it only if that channel is disabled.
  const auto tabs = ChatTabs::Current();
  auto       root = il2cpp::GcHandle::Strong(tabs ? _this->swipeScroller() : nullptr);
  if (auto* scroller = root.Get<SwipeScroller>(); scroller && tabs->Disabled(scroller->currentContentIndex())) {
    _this->SetFocusedPanel(ChatChannelCategory::Alliance);
    FocusAlliance(_this, tabs->alliance);
  }
}

void ChatPreviewController_OnPanelFocused_Hook(auto original, ChatPreviewController* _this, int32_t index)
{
  const auto tabs = ChatTabs::Current();
  if (!tabs || !tabs->Disabled(index))
    return original(_this, index);
  // Swiped onto a disabled channel: focus alliance before the game handles the change, then line the scroller up.
  _this->SetFocusedPanel(ChatChannelCategory::Alliance);
  original(_this, tabs->alliance);
  FocusAlliance(_this, tabs->alliance);
}

void ChatPreviewController_OnGlobalMessageReceived_Hook(auto original, ChatPreviewController* _this, void* message)
{
  if (!Config::Get().ui.disable_galaxy_chat)
    original(_this, message);
}

void ChatPreviewController_OnRegionalMessageReceived_Hook(auto original, ChatPreviewController* _this, void* message)
{
  if (!Config::Get().ui.disable_veil_chat)
    original(_this, message);
}

// --- Player titles ---
//
// Only chat rows lose their title: the same profile widget appears on other screens.

struct ChatTitleLayout {
  il2cpp::GcHandle element;
  bool             original_ignore = false;
};
auto& chat_title_layouts = *new std::unordered_map<unity::LayoutElement*, ChatTitleLayout>;

void RestoreChatTitleLayout(UserProfileWidget* profile)
{
  auto owner     = il2cpp::GcHandle::Strong(profile);
  auto title     = il2cpp::GcHandle::Strong(owner ? profile->playerTitleWidget() : nullptr);
  auto container = il2cpp::GcHandle::Strong(title ? title.Get<PlayerTitleWidget>()->container() : nullptr);
  auto element   = il2cpp::GcHandle::Strong(
      container ? container.Get<unity::GameObject>()->GetComponent<unity::LayoutElement>() : nullptr);
  const auto saved = chat_title_layouts.find(element.Get<unity::LayoutElement>());
  if (saved == chat_title_layouts.end())
    return;
  if (saved->second.element.Get() == element.Get())
    element.Get<unity::LayoutElement>()->SetIgnoreLayout(saved->second.original_ignore);
  chat_title_layouts.erase(saved);
}

void HideChatTitle(UserProfileWidget* profile)
{
  auto owner = il2cpp::GcHandle::Strong(profile);
  if (!owner)
    return;
  auto row = il2cpp::GcHandle::Strong(chat_title_binding::Parent<ChatMessageWidget>(profile));
  if (!row) {
    RestoreChatTitleLayout(profile);
    return;
  }
  auto title  = il2cpp::GcHandle::Strong(profile->playerTitleWidget());
  auto object = il2cpp::GcHandle::Strong(title ? title.Get<PlayerTitleWidget>()->gameObject() : nullptr);
  if (object)
    object.Get<unity::GameObject>()->SetActive(false);
  // The container reserves height even after its title child is hidden. Native binding can reactivate it, so
  // exclude its LayoutElement until titles are enabled again as well.
  auto container = il2cpp::GcHandle::Strong(title ? title.Get<PlayerTitleWidget>()->container() : nullptr);
  if (container) {
    auto element = il2cpp::GcHandle::Strong(container.Get<unity::GameObject>()->GetComponent<unity::LayoutElement>());
    if (element) {
      std::erase_if(chat_title_layouts, [](const auto& item) { return !item.second.element.Get(); });
      auto* key = element.Get<unity::LayoutElement>();
      if (!chat_title_layouts.contains(key)) {
        const auto original_ignore = key->TryCall<"get_ignoreLayout", bool()>();
        auto       weak            = il2cpp::GcHandle::Weak(key);
        if (original_ignore && weak)
          chat_title_layouts.emplace(key, ChatTitleLayout{std::move(weak), original_ignore.value});
      }
      if (chat_title_layouts.contains(key))
        key->SetIgnoreLayout(true);
    }
    container.Get<unity::GameObject>()->SetActive(false);
  }
  // The virtualized list caches complete row heights. Refresh its measurement, not only the inner profile.
  auto row_object = il2cpp::GcHandle::Strong(row.Get<ChatMessageWidget>()->gameObject());
  auto rect       = il2cpp::GcHandle::Strong(
      row_object ? row_object.Get<unity::GameObject>()->GetComponent<unity::RectTransform>() : nullptr);
  if (rect) {
    unity::LayoutRebuilder::ForceRebuildLayoutImmediate(rect.Get<unity::RectTransform>());
    unity::LayoutRebuilder::MarkLayoutForRebuild(rect.Get<unity::RectTransform>());
  }
}

// The game shows the title again on every update, so it is hidden after each one.
void UserProfileWidget_UpdatePlayerTitle_Hook(auto original, UserProfileWidget* _this)
{
  if (Config::Get().ui.show_player_title)
    RestoreChatTitleLayout(_this);
  original(_this);
  if (!Config::Get().ui.show_player_title)
    HideChatTitle(_this);
}

// Hook the substantive implementation, not the tiny shared SetWidgetData/OnDidBindContext wrappers.
void PlayerTitleWidget_Bind_Hook(auto original, PlayerTitleWidget* _this)
{
  if (Config::Get().ui.show_player_title) {
    auto profile = il2cpp::GcHandle::Strong(chat_title_binding::Parent<UserProfileWidget>(_this));
    RestoreChatTitleLayout(profile.Get<UserProfileWidget>());
  }
  original(_this);
  if (Config::Get().ui.show_player_title)
    return;
  auto profile = il2cpp::GcHandle::Strong(chat_title_binding::Parent<UserProfileWidget>(_this));
  HideChatTitle(profile.Get<UserProfileWidget>());
}

// --- Streamer Mode chat names ---

enum class StreamerChatViewKind { Preview, Message, Header, PrivateChannel, BlockedUser };

struct StreamerChatView {
  il2cpp::GcHandle            owner;
  StreamerChatViewKind        kind;
  il2cpp::GcHandle            label;
  TextLocalizer*              label_key = nullptr; // Index bookkeeping only; never dereferenced.
  streamer_mode::SurfaceCover cover;
  bool                        updating = false, healthy = true;
};

struct StreamerChatState {
  std::vector<std::shared_ptr<StreamerChatView>> views;
  std::unordered_map<TextLocalizer*, std::weak_ptr<StreamerChatView>> labels;
  std::thread::id                                game_thread;
  streamer_mode::ChatPolicy                                           last_policy;
  bool                                           installed = false, formatter = false, warned = false;
  bool                                           untracked = false;
};

StreamerChatState& StreamerChat()
{
  // Weak owners retire during use. Their final bookkeeping must not call a stopped runtime.
  static auto* state = new StreamerChatState;
  return *state;
}

enum class StreamerChatFormat { Preview, Profile, Header, UnknownHeader };

struct StreamerChatFormatBind {
  TextLocalizer* label;
  streamer_mode::ChatVisibility visibility;
  StreamerChatFormat            format          = StreamerChatFormat::Preview;
  UserProfileWidget*            profile         = nullptr;
  il2cpp::Object*               owner           = nullptr;
  StreamerChatViewKind          kind            = StreamerChatViewKind::Preview;
  bool                          ownership_known = true;
  bool           rewritten = false;
  bool           failed    = false;
};
thread_local StreamerChatFormatBind* streamer_chat_format_bind = nullptr;

bool StreamerChatOnGameThread()
{
  const auto& state = StreamerChat();
  return state.installed && state.game_thread == std::this_thread::get_id();
}

void WarnStreamerChat()
{
  if (!std::exchange(StreamerChat().warned, true))
    spdlog::warn("[StreamerChatPrivacy] could not verify the selected chat protections");
}

streamer_mode::Subject StreamerChatSubject(il2cpp::CallResult<Il2CppString*> sender)
{
  using streamer_mode::Subject;
  auto sender_id = il2cpp::GcHandle::Strong(sender ? reinterpret_cast<Il2CppObject*>(sender.value) : nullptr);
  if (!sender_id || reinterpret_cast<const Il2CppString*>(sender_id.Get())->length == 0)
    return Subject::Unknown;
  const auto found   = UserProfileManager::TryInstance();
  auto       manager = il2cpp::GcHandle::Strong(found ? found.value : nullptr);
  if (!manager || !manager.Get<UserProfileManager>()->Is<UserProfileManager>())
    return Subject::Unknown;
  const auto found_profile = manager.Get<UserProfileManager>()->TryLocalUserProfile();
  auto       profile       = il2cpp::GcHandle::Strong(found_profile ? found_profile.value : nullptr);
  if (!profile || !profile.Get<UserProfile>()->Is<UserProfile>())
    return Subject::Unknown;
  const auto found_id = profile.Get<UserProfile>()->TryUserId();
  auto       local_id = il2cpp::GcHandle::Strong(found_id ? reinterpret_cast<Il2CppObject*>(found_id.value) : nullptr);
  if (!local_id || reinterpret_cast<const Il2CppString*>(local_id.Get())->length == 0)
    return Subject::Unknown;
  const auto* sender_value = reinterpret_cast<const Il2CppString*>(sender_id.Get());
  const auto* local_value  = reinterpret_cast<const Il2CppString*>(local_id.Get());
  return sender_value->length == local_value->length
                 && std::equal(sender_value->chars, sender_value->chars + sender_value->length, local_value->chars)
             ? Subject::Self
             : Subject::Other;
}

template <class Owner> streamer_mode::Subject StreamerChatMessageSubject(Owner* owner)
{
  const auto found   = owner->TryMessage();
  auto       message = il2cpp::GcHandle::Strong(found ? found.value : nullptr);
  return StreamerChatSubject(message ? message.template Get<ChatMessage>()->TrySenderId()
                                     : il2cpp::CallResult<Il2CppString*>{});
}

streamer_mode::Subject StreamerChatProfileSubject(UserProfileWidget* widget)
{
  auto       source  = il2cpp::GcHandle::Strong(widget);
  const auto found   = source ? source.Get<UserProfileWidget>()->TryProfile() : il2cpp::CallResult<ChatProfile*>{};
  auto       profile = il2cpp::GcHandle::Strong(found ? found.value : nullptr);
  return StreamerChatSubject(profile ? profile.Get<ChatProfile>()->TryUserId() : il2cpp::CallResult<Il2CppString*>{});
}

bool StreamerChatOwnershipKnown(streamer_mode::ChatPolicy policy, streamer_mode::Subject subject)
{ return subject != streamer_mode::Subject::Unknown || (!policy.my_name && !policy.my_alliance); }

bool RememberStreamerChatLabel(StreamerChatView& view, TextLocalizer* label)
{
  auto& state    = StreamerChat();
  auto* previous = std::exchange(view.label_key, label);
  if (previous && previous != label) {
    const auto found = state.labels.find(previous);
    if (found != state.labels.end() && found->second.lock().get() == &view)
      state.labels.erase(found);
  }
  view.label = il2cpp::GcHandle::Weak(label);
  if (!label)
    return true;
  const auto owner = std::ranges::find_if(state.views, [&view](const auto& item) { return item.get() == &view; });
  if (!view.label || owner == state.views.end()) {
    state.untracked = true;
    return false;
  }
  state.labels[label] = *owner;
  return true;
}

bool CoverStreamerChatLabel(StreamerChatView& view, TextLocalizer* label)
{
  // Some admin and unpopulated rows have no sender label.
  if (!label)
    return view.cover.Restore();
  auto root = il2cpp::GcHandle::Strong(label);
  if (!root)
    return false;
  const auto observed = root.Get<TextLocalizer>()->TryCall<"get_gameObject", unity::GameObject*>();
  auto       target   = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  return target && view.cover.Hide(target.Get<unity::GameObject>());
}

TextLocalizer* StreamerChatProfileName(UserProfileWidget* profile)
{
  auto root = il2cpp::GcHandle::Strong(profile);
  return root ? root.Get<UserProfileWidget>()->nameLabel() : nullptr;
}

UserProfileWidget* StreamerChatProfile(StreamerChatViewKind kind, il2cpp::Object* owner)
{
  switch (kind) {
    case StreamerChatViewKind::Message:
      return static_cast<ChatMessageWidget*>(owner)->profile();
    case StreamerChatViewKind::PrivateChannel:
      return static_cast<ChatChannelItemWidget*>(owner)->profile();
    case StreamerChatViewKind::BlockedUser:
      return static_cast<ChatBlockUserWidget*>(owner)->profile();
    default:
      return nullptr;
  }
}

streamer_mode::Subject StreamerChatViewSubject(StreamerChatViewKind kind, il2cpp::Object* owner)
{
  if (kind == StreamerChatViewKind::Message)
    return StreamerChatMessageSubject(static_cast<ChatMessageWidget*>(owner));
  if (kind == StreamerChatViewKind::Preview)
    return StreamerChatMessageSubject(static_cast<ChatPreviewMessageWidget*>(owner));
  if (kind != StreamerChatViewKind::Header)
    return StreamerChatProfileSubject(StreamerChatProfile(kind, owner));
  const auto found   = static_cast<FullScreenChatViewController*>(owner)->TryChatContext();
  auto       context = il2cpp::GcHandle::Strong(found ? found.value : nullptr);
  auto       profile = il2cpp::GcHandle::Strong(context ? context.Get<ChatSectionContext>()->headerProfile() : nullptr);
  return StreamerChatSubject(profile ? profile.Get<UserProfile>()->TryUserId() : il2cpp::CallResult<Il2CppString*>{});
}

bool ApplyStreamerChatNames(StreamerChatView& view, streamer_mode::ChatPolicy policy)
{
  if (std::exchange(view.updating, true))
    return true;
  struct Done {
    bool& updating;
    ~Done()
    { updating = false; }
  } done{view.updating};

  auto* managed_owner = view.owner.Get();
  if (!managed_owner)
    return view.cover.Restore();
  auto       owner = il2cpp::GcHandle::Strong(managed_owner);
  const auto alive = owner ? unity::TryAlive(owner.Get()) : il2cpp::CallResult<bool>{};
  if (!alive)
    return false;
  if (!alive.value)
    return view.cover.Restore();

  // Rebind native presentation at every policy edge. Native models and their unmasked arguments stay intact.
  if (view.kind == StreamerChatViewKind::Preview)
    return owner.Get<ChatPreviewMessageWidget>()->TryCall<"SetWidgetData", void>() && view.healthy;

  auto           profile        = il2cpp::GcHandle::Strong(StreamerChatProfile(view.kind, owner.Get<il2cpp::Object>()));
  TextLocalizer* observed_label = profile ? profile.Get<UserProfileWidget>()->nameLabel() : nullptr;
  if (view.kind == StreamerChatViewKind::Header) {
    auto* header = owner.Get<FullScreenChatViewController>();
    const auto private_header = header->TryPrivateHeader();
    if (!private_header) {
      auto* target = header->headerTitle();
      RememberStreamerChatLabel(view, target);
      if (!target)
        return view.cover.Restore();
      const auto mask = policy.For(streamer_mode::Subject::Unknown);
      if (mask.name || mask.alliance) {
        CoverStreamerChatLabel(view, target);
        return false;
      }
      return view.cover.Restore();
    }
    if (private_header.value)
      observed_label = header->headerTitle();
  }
  auto label = il2cpp::GcHandle::Strong(observed_label);
  if (!label) {
    RememberStreamerChatLabel(view, nullptr);
    return view.cover.Restore();
  }
  if (!RememberStreamerChatLabel(view, label.Get<TextLocalizer>()))
    return false;
  const auto subject =
      policy.Any() ? StreamerChatViewSubject(view.kind, owner.Get<il2cpp::Object>()) : streamer_mode::Subject::Other;
  const auto             visibility = policy.For(subject);
  const bool             hidden     = visibility.name || visibility.alliance;
  StreamerChatFormatBind bind{label.Get<TextLocalizer>(),
                              visibility,
                              view.kind == StreamerChatViewKind::Header ? StreamerChatFormat::Header
                                                                        : StreamerChatFormat::Profile,
                              profile.Get<UserProfileWidget>(),
                              owner.Get<il2cpp::Object>(),
                              view.kind};
  bind.ownership_known = StreamerChatOwnershipKnown(policy, subject);
  struct RestoreBind {
    StreamerChatFormatBind* previous;
    ~RestoreBind()
    { streamer_chat_format_bind = previous; }
  } restore{std::exchange(streamer_chat_format_bind, &bind)};
  const auto refreshed = view.kind == StreamerChatViewKind::Header
                             ? owner.Get<FullScreenChatViewController>()->TryCall<"RefreshContent", void>()
                             : profile.Get<UserProfileWidget>()->TryRefreshName();
  const bool requested = bind.visibility.name || bind.visibility.alliance;
  const bool safe      = refreshed && !bind.failed && (!requested || bind.rewritten);
  if (!safe) {
    if (hidden)
      CoverStreamerChatLabel(view, label.Get<TextLocalizer>());
    return false;
  }
  return view.cover.Restore() && bind.ownership_known;
}

std::shared_ptr<StreamerChatView> ObserveStreamerChatName(il2cpp::Object* owner, StreamerChatViewKind kind,
                                                          bool preview_healthy = true, bool refresh = true)
{
  if (!StreamerChatOnGameThread())
    return {};
  auto& state = StreamerChat();
  auto  found = std::ranges::find_if(state.views, [owner](const auto& view) { return view->owner.Get() == owner; });
  std::shared_ptr<StreamerChatView> view;
  if (found != state.views.end()) {
    view = *found;
  } else {
    auto weak = il2cpp::GcHandle::Weak(owner);
    if (!weak) {
      state.untracked = true;
      WarnStreamerChat();
      return {};
    }
    std::vector<std::shared_ptr<StreamerChatView>> retired;
    std::erase_if(state.views, [&](const auto& item) {
      if (item->owner.Get())
        return false;
      retired.push_back(item);
      return true;
    });
    view = std::make_shared<StreamerChatView>(StreamerChatView{std::move(weak), kind});
    state.views.push_back(view);
    // Destroying an owned Unity group can reenter the game. Retire outside the vector walk.
    for (const auto& item : retired) {
      RememberStreamerChatLabel(*item, nullptr);
      if (!item->cover.Restore()) {
        item->healthy = false;
        state.views.push_back(item);
        WarnStreamerChat();
      }
    }
  }
  if (!refresh) {
    auto* label = kind == StreamerChatViewKind::Preview  ? static_cast<ChatPreviewMessageWidget*>(owner)->messageText()
                  : kind == StreamerChatViewKind::Header ? nullptr
                                                         : StreamerChatProfileName(StreamerChatProfile(kind, owner));
    if (kind == StreamerChatViewKind::Header) {
      auto* header = static_cast<FullScreenChatViewController*>(owner);
      const auto private_header = header->TryPrivateHeader();
      if (!private_header || private_header.value)
        label = header->headerTitle();
      if (!private_header) {
        const auto mask = streamer_mode::ChatProtections().For(streamer_mode::Subject::Unknown);
        if (mask.name || mask.alliance)
          CoverStreamerChatLabel(*view, label);
      }
    }
    view->healthy = RememberStreamerChatLabel(*view, label);
    return view;
  }
  view->healthy = kind == StreamerChatViewKind::Preview
                      ? preview_healthy
                      : ApplyStreamerChatNames(*view, streamer_mode::ChatProtections());
  if (!view->healthy)
    WarnStreamerChat();
  return view;
}

void StreamerChatFrame()
{
  auto&      state  = StreamerChat();
  const auto policy = streamer_mode::ChatProtections();
  if (state.last_policy == policy)
    return;
  if (!policy.Any())
    state.untracked = false;
  // A managed visibility call can admit another view. Iterate a snapshot of known owners.
  const auto views    = state.views;
  bool       restored = true;
  for (const auto& view : views) {
    view->healthy = ApplyStreamerChatNames(*view, policy);
    restored &= view->healthy;
    if (!view->healthy)
      WarnStreamerChat();
  }
  if (policy.Any() || restored)
    state.last_policy = policy;
}

std::optional<std::array<bool, 5>> StreamerChatSlots(StreamerChatFormatBind& bind, size_t count)
{
  std::array<bool, 5> slots{};
  if (bind.format == StreamerChatFormat::UnknownHeader)
    return {};
  if (bind.format == StreamerChatFormat::Preview) {
    if (count != 4 && count != 5)
      return {};
    slots[count - 3] = bind.visibility.name;
    if (count == 5)
      slots[1] = bind.visibility.alliance;
  } else if (bind.format == StreamerChatFormat::Header) {
    if (count != 1 && count != 2)
      return {};
    slots[count - 1] = bind.visibility.name;
    if (count == 2)
      slots[0] = bind.visibility.alliance;
  } else {
    // Native UpdatePlayerName always owns five parameters. GetSuitableNameStyle writes the final style
    // before SetContext/SetLocalTextParameters; unused parameters are cleared by native code.
    if (count != 5 || !bind.profile)
      return {};
    const auto style = bind.profile->TryNameStyle();
    if (!style)
      return {};
    switch (style.value) {
      case 1: // NameOnly
      case 2: // NameAndLevel
      case 5: // NameWithRivalServer
        slots[0] = bind.visibility.name;
        break;
      case 3: // NameWithTag
      case 6: // NameWithTagAndRivalServer
        slots[0] = slots[2] = bind.visibility.alliance;
        slots[1]            = bind.visibility.name;
        break;
      case 4: // NameWithDiplomacy
        slots[0] = bind.visibility.alliance;
        slots[1] = bind.visibility.name;
        break;
      case 7: // AllInfo
        slots[0] = bind.visibility.name;
        slots[2] = slots[3] = bind.visibility.alliance;
        break;
      default:
        return {};
    }
  }
  return slots;
}

void TextLocalizer_SetLocalTextParameters_StreamerChatHook(auto original, TextLocalizer* _this, bool parse_id,
                                                           il2cpp::ArrayObject* arguments)
{
  if (!arguments) {
    original(_this, parse_id, arguments);
    return;
  }
  if (!StreamerChatOnGameThread()) {
    original(_this, parse_id, arguments);
    return;
  }
  const auto policy = streamer_mode::ChatProtections();
  if (!policy.Any()) {
    original(_this, parse_id, arguments);
    return;
  }
  auto*                             bind = streamer_chat_format_bind;
  std::shared_ptr<StreamerChatView> tracked;
  il2cpp::GcHandle                  owner, profile;
  StreamerChatFormatBind            observed{_this, {}};
  if (bind && bind->label == _this) {
    const auto found = StreamerChat().labels.find(_this);
    if (found != StreamerChat().labels.end())
      tracked = found->second.lock();
  }
  if (!bind || bind->label != _this) {
    auto&      labels = StreamerChat().labels;
    const auto found  = labels.find(_this);
    if (found == labels.end()) {
      original(_this, parse_id, arguments);
      return;
    }
    tracked = found->second.lock();
    if (!tracked || tracked->label.Get() != _this) {
      labels.erase(found);
      original(_this, parse_id, arguments);
      return;
    }
    owner = il2cpp::GcHandle::Strong(tracked->owner.Get());
    if (!owner) {
      original(_this, parse_id, arguments);
      return;
    }
    observed.owner = owner.Get<il2cpp::Object>();
    observed.kind  = tracked->kind;
    profile        = il2cpp::GcHandle::Strong(StreamerChatProfile(tracked->kind, observed.owner));
    if (profile && profile.Get<UserProfileWidget>()->nameLabel() != _this)
      profile.Reset();
    observed.format  = tracked->kind == StreamerChatViewKind::Header    ? StreamerChatFormat::Header
                       : tracked->kind == StreamerChatViewKind::Preview ? StreamerChatFormat::Preview
                                                                        : StreamerChatFormat::Profile;
    observed.profile = profile.Get<UserProfileWidget>();
    bind             = &observed;
  }
  auto subject = streamer_mode::Subject::Unknown;
  bool public_header = false;
  if (bind->kind == StreamerChatViewKind::Header) {
    const auto private_header = bind->owner
        ? static_cast<FullScreenChatViewController*>(bind->owner)->TryPrivateHeader()
        : il2cpp::CallResult<bool>{};
    if (!private_header)
      bind->format = StreamerChatFormat::UnknownHeader;
    else if (!private_header.value)
      public_header = true;
    else {
      bind->format = StreamerChatFormat::Header;
      subject = StreamerChatViewSubject(bind->kind, bind->owner);
    }
  } else if (bind->owner)
    subject = StreamerChatViewSubject(bind->kind, bind->owner);
  bind->visibility = public_header ? streamer_mode::ChatVisibility{} : policy.For(subject);
  bind->ownership_known = public_header || StreamerChatOwnershipKnown(policy, subject);
  const bool hidden     = bind->visibility.name || bind->visibility.alliance;
  // SetContext and ClearTextParameters can precede the final native identity arguments. Their empty calls
  // contain nothing to redact; retain the cover until a qualified nonempty render has completed.
  if (!hidden) {
    original(_this, parse_id, arguments);
    if (tracked)
      tracked->healthy = tracked->cover.Restore() && bind->ownership_known;
    return;
  }
  auto       incoming = il2cpp::GcHandle::Strong(arguments);
  const auto checked  = incoming ? il2cpp::TryReferenceArray<Il2CppObject*>(incoming.Get())
                                 : il2cpp::CallResult<il2cpp::Array<Il2CppObject*>*>{};
  const auto count    = checked && checked.value ? checked.value->size() : 0;
  if (checked && count == 0) {
    original(_this, parse_id, arguments);
    return;
  }
  const auto prefix = checked ? chat_identity_binding::ColourPrefix(_this, checked.value)
                              : il2cpp::CallResult<size_t>{};
  const auto slots = prefix ? StreamerChatSlots(*bind, count - prefix.value)
                             : std::optional<std::array<bool, 5>>{};
  auto blank = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString("")));
  auto       copy =
      il2cpp::GcHandle::Strong(slots ? il2cpp::Array<Il2CppObject*>::New(il2cpp::ObjectClass(), count) : nullptr);
  if (!slots || !blank || !copy) {
    bind->failed = true;
    if (tracked) {
      CoverStreamerChatLabel(*tracked, _this);
      tracked->healthy = false;
      WarnStreamerChat();
    }
    original(_this, parse_id, arguments);
    return;
  }
  auto* args = copy.Get<il2cpp::Array<Il2CppObject*>>();
  for (size_t index = 0; index < count; ++index)
    args->Set(index, index >= prefix.value && (*slots)[index - prefix.value] ? blank.Get()
                                                                          : (*checked.value)[index]);
  original(_this, parse_id, copy.Get<il2cpp::ArrayObject>());
  bind->rewritten = true;
  bind->failed    = false;
  if (tracked) {
    tracked->healthy = tracked->cover.Restore() && bind->ownership_known;
    if (!tracked->healthy)
      WarnStreamerChat();
  }
}

bool StreamerChatPreviewDisabled(TextLocalizer* label)
{
  // The native empty-preview path disables its graphic without invoking the formatter.
  const auto target = il2cpp::GcHandle::Strong(label->Target());
  const auto enabled =
      target ? target.Get<il2cpp::Object>()->TryCall<"get_enabled", bool>() : il2cpp::CallResult<bool>{};
  return enabled && !enabled.value;
}

void ChatPreviewMessageWidget_SetWidgetData_Hook(auto original, ChatPreviewMessageWidget* _this)
{
  if (!StreamerChatOnGameThread()) {
    original(_this);
    return;
  }
  const auto policy = streamer_mode::ChatProtections();
  ObserveStreamerChatName(_this, StreamerChatViewKind::Preview, true, false);
  const auto             subject    = policy.Any() ? StreamerChatMessageSubject(_this) : streamer_mode::Subject::Other;
  const auto             visibility = policy.For(subject);
  auto                   label      = il2cpp::GcHandle::Strong(policy.Any() ? _this->messageText() : nullptr);
  StreamerChatFormatBind bind{label.Get<TextLocalizer>(),   visibility, StreamerChatFormat::Preview, nullptr, _this,
                              StreamerChatViewKind::Preview};
  bind.ownership_known = StreamerChatOwnershipKnown(policy, subject);
  {
    struct RestoreBind {
      StreamerChatFormatBind* previous;
      ~RestoreBind()
      { streamer_chat_format_bind = previous; }
    } restore{std::exchange(streamer_chat_format_bind, bind.label ? &bind : nullptr)};
    original(_this);
  }
  const bool requested = bind.visibility.name || bind.visibility.alliance;
  const bool disabled = bind.label && !bind.rewritten && StreamerChatPreviewDisabled(bind.label);
  const bool safe      = !requested
                         || (bind.label && _this->messageText() == bind.label && !bind.failed
                             && (bind.rewritten || disabled));
  const auto view = ObserveStreamerChatName(_this, StreamerChatViewKind::Preview,
                                           safe && (disabled || bind.ownership_known));
  if (view) {
    if (!RememberStreamerChatLabel(*view, label.Get<TextLocalizer>()))
      view->healthy = false;
    const bool restored = safe ? view->cover.Restore() : CoverStreamerChatLabel(*view, bind.label);
    view->healthy &= restored;
    if (!view->healthy)
      WarnStreamerChat();
  }
}

void ChatMessageWidget_OnDidBindContext_StreamerHook(auto original, ChatMessageWidget* _this)
{
  ObserveStreamerChatName(_this, StreamerChatViewKind::Message, true, false);
  original(_this);
  ObserveStreamerChatName(_this, StreamerChatViewKind::Message);
}

void FullScreenChatViewController_RefreshContent_Hook(auto original, FullScreenChatViewController* _this)
{
  ObserveStreamerChatName(_this, StreamerChatViewKind::Header, true, false);
  original(_this);
  ObserveStreamerChatName(_this, StreamerChatViewKind::Header);
}

void ChatChannelItemWidget_SetWidgetData_Hook(auto original, ChatChannelItemWidget* _this)
{
  ObserveStreamerChatName(_this, StreamerChatViewKind::PrivateChannel, true, false);
  original(_this);
  ObserveStreamerChatName(_this, StreamerChatViewKind::PrivateChannel);
}

void ChatBlockUserWidget_SetWidgetData_Hook(auto original, ChatBlockUserWidget* _this)
{
  ObserveStreamerChatName(_this, StreamerChatViewKind::BlockedUser, true, false);
  original(_this);
  ObserveStreamerChatName(_this, StreamerChatViewKind::BlockedUser);
}

template <class Owner> void* StreamerChatEntry(il2cpp::Method method)
{ return method && method.Info()->klass == Owner::Class::Raw() ? il2cpp::CheckedEntry(method) : nullptr; }

bool InstallStreamerChat()
{
  if (!unity::CanvasGroup::AlphaAvailable() || !ChatPreviewMessageWidget::Class::HasField<"_text", TextLocalizer*>()
      || !ChatMessageWidget::Class::HasField<"_userProfileWidget", UserProfileWidget*>()
      || !UserProfileWidget::Class::HasField<"_name", TextLocalizer*>()
      || !FullScreenChatViewController::Class::HasField<"_headerTitle", TextLocalizer*>()
      || !FullScreenChatViewController::Class::Method<ChatSectionContext*()>("get_CanvasContext")
      || !ChatSectionContext::Class::HasField<"ChannelID", Il2CppString*>()
      || !ChatSectionContext::Class::HasField<"ChatUserProfile", il2cpp::Object*>()
      || !ChatChannelItemWidget::Class::HasField<"_userProfileWidget", UserProfileWidget*>()
      || !ChatBlockUserWidget::Class::HasField<"_userProfileWidget", UserProfileWidget*>())
    return false;

  const auto preview =
      StreamerChatEntry<ChatPreviewMessageWidget>(ChatPreviewMessageWidget::Class::Method<void()>("SetWidgetData"));
  const auto message =
      StreamerChatEntry<ChatMessageWidget>(ChatMessageWidget::Class::Method<void()>("OnDidBindContext"));
  const auto format = StreamerChatEntry<TextLocalizer>(
      TextLocalizer::Class::Method<void(bool, il2cpp::ArrayObject*)>("SetLocalTextParameters"));
  const auto header = StreamerChatEntry<FullScreenChatViewController>(
      FullScreenChatViewController::Class::Method<void()>("RefreshContent"));
  const auto channel =
      StreamerChatEntry<ChatChannelItemWidget>(ChatChannelItemWidget::Class::Method<void()>("SetWidgetData"));
  const auto blocked =
      StreamerChatEntry<ChatBlockUserWidget>(ChatBlockUserWidget::Class::Method<void()>("SetWidgetData"));
  std::array entries{preview, message, header, channel, blocked};
  std::ranges::sort(entries);
  if (std::ranges::adjacent_find(entries) != entries.end()
      || (format && std::ranges::find(entries, format) != entries.end()))
    return false;
  il2cpp::HookSet hooks("StreamerChatPrivacy");
  IL2CPP_STAGE_AT(hooks, preview, ChatPreviewMessageWidget_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, message, ChatMessageWidget_OnDidBindContext_StreamerHook);
  if (format)
    IL2CPP_STAGE_AT(hooks, format, TextLocalizer_SetLocalTextParameters_StreamerChatHook);
  else
    spdlog::warn("[StreamerChatPrivacy] selective formatting unavailable; retaining coarse label protection");
  IL2CPP_STAGE_AT(hooks, header, FullScreenChatViewController_RefreshContent_Hook);
  IL2CPP_STAGE_AT(hooks, channel, ChatChannelItemWidget_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, blocked, ChatBlockUserWidget_SetWidgetData_Hook);
  auto& state       = StreamerChat();
  state.game_thread = std::this_thread::get_id();
  state.installed   = hooks.Commit();
  state.formatter   = state.installed && format;
  return state.installed;
}

// Called before reload tears down the managers. Do not call back into native UI from this reset.
void ResetChat()
{
  chat_preview.Reset();
  chat_startup       = {};
  chat_width_applied = false;
}

void ResetChatTitles()
{ chat_title_layouts.clear(); }

bool InstallChat()
{
  il2cpp::HookSet hooks("Chat");
  IL2CPP_STAGE(hooks, FullScreenChatViewController::Class::Method<void()>("AboutToShow"),
               FullScreenChatViewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, FullScreenChatViewController::Class::Method<void(int32_t, void*)>("OnDidChangeSelectedTab"),
               FullScreenChatViewController_OnDidChangeSelectedTab_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method<void()>("AboutToShow"),
               ChatPreviewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method<void(int32_t)>("OnPanelFocused"),
               ChatPreviewController_OnPanelFocused_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method<void(void*)>("OnGlobalMessageReceived"),
               ChatPreviewController_OnGlobalMessageReceived_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method<void(void*)>("OnRegionalMessageReceived"),
               ChatPreviewController_OnRegionalMessageReceived_Hook);
  IL2CPP_STAGE_AT(
      hooks, il2cpp::CheckedEntry(ChatMessageListLocalViewController::Class::Method<void(Il2CppString*)>("OnSubmit")),
      ChatMessageListLocalViewController_OnSubmit_Hook);
  return hooks.Commit();
}

bool InstallChatAvatarRetention()
{
  const auto status = ChatMessageWidget::AvatarRetentionStatus();
  if (status != ChatMessageWidget::RetentionStatus::Ready) {
    if (status == ChatMessageWidget::RetentionStatus::NativeBehavior)
      spdlog::info("[ChatAvatarRetention] client has no serialized retention switch; native behavior retained");
    else
      spdlog::warn("[ChatAvatarRetention] portrait field contract unavailable; native behavior retained");
    return false;
  }
  il2cpp::HookSet hooks("ChatAvatarRetention");
  IL2CPP_STAGE(hooks, ChatMessageWidget::AvatarRetentionTarget(), ChatMessageWidget_SetWidgetData_Hook);
  return hooks.Commit();
}

bool InstallChatTitles()
{
  void*           update_entry = il2cpp::CheckedEntry(UserProfileWidget::Class::Method<void()>("UpdatePlayerTitle"));
  void*           bind_entry   = il2cpp::CheckedEntry(PlayerTitleWidget::Class::Method<void()>("SetIdentifiers"));
  il2cpp::HookSet hooks("ChatTitles");
  IL2CPP_STAGE_AT(hooks, update_entry, UserProfileWidget_UpdatePlayerTitle_Hook);
  if (bind_entry != update_entry)
    IL2CPP_STAGE_AT(hooks, bind_entry, PlayerTitleWidget_Bind_Hook);
  return hooks.Commit();
}
} // namespace

void OpenChatShortcut()
{
  const bool side = Shortcut::IsDown(Action::kShowChatSide1) || Shortcut::IsDown(Action::kShowChatSide2);
  if (!side && !Shortcut::IsDown(Action::kShowChat))
    return;
  auto  manager = il2cpp::GcHandle::Strong(ChatManager::Instance());
  auto* chat    = manager.Get<ChatManager>();
  if (!chat)
    return;

  // With side chat already open, the shortcut focuses its input instead.
  const auto side_open = chat->TryCall<"get_IsSideChatOpen", bool()>();
  if (!side_open)
    return;
  if (side_open.value) {
    auto view = il2cpp::GcHandle::Strong(unity::Newest<FullScreenChatViewController>());
    auto list = il2cpp::GcHandle::Strong(view ? view.Get<FullScreenChatViewController>()->messageList() : nullptr);
    auto input =
        il2cpp::GcHandle::Strong(list ? list.Get<ChatMessageListLocalViewController>()->inputField() : nullptr);
    if (input)
      input.Get<unity::TMP_InputField>()->ActivateInputField();
  } else {
    chat->OpenChannel(ChatChannelCategory::Alliance, side ? ChatViewMode::Side : ChatViewMode::Fullscreen);
  }
}

bool ChatChannelShortcuts()
{
  auto  manager = il2cpp::GcHandle::Strong(ChatManager::Instance());
  auto* chat    = manager.Get<ChatManager>();
  if (!chat)
    return false;
  for (const auto [shortcut, channel] : {std::pair{Action::kSelectChatGlobal, ChatChannelCategory::Global},
                                         std::pair{Action::kSelectChatAlliance, ChatChannelCategory::Alliance},
                                         std::pair{Action::kSelectChatPrivate, ChatChannelCategory::Private}}) {
    if (Shortcut::IsDown(shortcut)) {
      chat->OpenChannel(channel);
      return true;
    }
  }
  return false;
}

bool streamer_mode::ChatNamesAvailable()
{
  const auto& state = StreamerChat();
  return state.installed && FrameDeliveryAvailable();
}

bool streamer_mode::ChatNamesDegraded()
{
  const auto& state = StreamerChat();
  return state.installed && ChatProtections().Any()
         && (!state.formatter || state.untracked
             || std::ranges::any_of(state.views, [](const auto& view) { return view->owner.Get() && !view->healthy; }));
}

bool streamer_mode::ChatAlliancesAvailable()
{ return ChatNamesAvailable() && StreamerChat().formatter; }

bool streamer_mode::ChatAlliancesDegraded()
{ return ChatNamesDegraded(); }

const Feature kChat{"Chat",    &Config::Patches::chatpatches, InstallChat,
                    ChatFrame, InstallRequirement::kOptional, ResetChat};
const Feature kChatAvatarRetention{"ChatAvatarRetention", &Config::Patches::chatpatches, InstallChatAvatarRetention};
const Feature kChatTitles{"ChatTitles", &Config::Patches::chattitledisplayhooks, InstallChatTitles,
                          nullptr,      InstallRequirement::kOptional,           ResetChatTitles};
const Feature kStreamerChatPrivacy{"StreamerChatPrivacy", &Config::Patches::streamermodehooks, InstallStreamerChat,
                                   StreamerChatFrame};
