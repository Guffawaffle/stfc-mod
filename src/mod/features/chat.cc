// Chat: the shortcuts that open it and switch channel; [ui] disable_galaxy_chat and disable_veil_chat, which grey
// out those tabs in full-screen chat, keep the chat preview off them, and drop their messages from the preview; and
// [ui] show_player_title, which can hide the title row under each sender's name.
// Chat portrait retention keeps downloaded avatars available when chat rows are reused.

#include "config.h"
#include "features/features.h"
#include "shortcuts.h"

#include "game/chat.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/core.h>
#include <unity/input.h>
#include <unity/tracker.h>

#include <optional>
#include <utility>

namespace
{
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

void HideChatTitle(UserProfileWidget* profile)
{
  auto owner = il2cpp::GcHandle::Strong(profile);
  if (!owner)
    return;
  auto row = il2cpp::GcHandle::Strong(chat_title_binding::Parent<ChatMessageWidget>(profile));
  if (!row)
    return;
  auto title  = il2cpp::GcHandle::Strong(profile->playerTitleWidget());
  auto object = il2cpp::GcHandle::Strong(title ? title.Get<PlayerTitleWidget>()->gameObject() : nullptr);
  if (object)
    object.Get<unity::GameObject>()->SetActive(false);
}

// The game shows the title again on every update, so it is hidden after each one.
void UserProfileWidget_UpdatePlayerTitle_Hook(auto original, UserProfileWidget* _this)
{
  original(_this);
  if (!Config::Get().ui.show_player_title)
    HideChatTitle(_this);
}

// Hook the substantive implementation, not the tiny shared SetWidgetData/OnDidBindContext wrappers.
void PlayerTitleWidget_Bind_Hook(auto original, PlayerTitleWidget* _this)
{
  original(_this);
  if (Config::Get().ui.show_player_title)
    return;
  auto profile = il2cpp::GcHandle::Strong(chat_title_binding::Parent<UserProfileWidget>(_this));
  HideChatTitle(profile.Get<UserProfileWidget>());
}

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

const Feature kChat{"Chat", &Config::Patches::chatpatches, InstallChat};
const Feature kChatAvatarRetention{"ChatAvatarRetention", &Config::Patches::chatpatches, InstallChatAvatarRetention};
const Feature kChatTitles{"ChatTitles", &Config::Patches::chattitledisplayhooks, InstallChatTitles};
