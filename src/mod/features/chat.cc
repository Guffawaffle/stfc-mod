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
#include <unity/tracker.h>

#include <utility>

namespace
{
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

  static ChatTabs Current()
  {
    auto* chat = ChatManager::Instance();
    if (!chat)
      return {};
    const int32_t galaxy = chat->CanSeeNewbieChat() ? 1 : 0;
    const bool    veil   = chat->CanSeeRegionalChat();
    return {.galaxy = galaxy, .veil = veil ? galaxy + 1 : -1, .alliance = galaxy + (veil ? 2 : 1)};
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

  auto*      tab_bar_view = _this->categoriesTabBarViewController();
  auto*      tab_bar      = tab_bar_view ? tab_bar_view->tabBar() : nullptr;
  auto*      buttons      = tab_bar ? tab_bar->data() : nullptr;
  const auto tabs         = ChatTabs::Current();
  for (const auto index : {tabs.galaxy, tabs.veil})
    if (auto* button = tabs.Disabled(index) ? il2cpp::At(buttons, index) : nullptr)
      static_cast<GenericButtonContext*>(button)->SetInteractable(false);
}

// Selecting a disabled tab (with the keyboard, say) does nothing.
void FullScreenChatViewController_OnDidChangeSelectedTab_Hook(auto original, FullScreenChatViewController* _this,
                                                              int32_t index, void* tab)
{
  if (!ChatTabs::Current().Disabled(index))
    original(_this, index, tab);
}

// Lines the preview's scroller up with the alliance panel.
void FocusAlliance(ChatPreviewController* preview, int32_t alliance)
{
  if (auto* scroller = preview->swipeScroller(); scroller && scroller->currentContentIndex() != alliance)
    scroller->FocusOnInstantly(alliance);
}

void ChatPreviewController_AboutToShow_Hook(auto original, ChatPreviewController* _this)
{
  original(_this);
  // The preview reopens on the last panel; move off it only if that channel is disabled.
  auto*      scroller = _this->swipeScroller();
  const auto tabs     = ChatTabs::Current();
  if (scroller && tabs.Disabled(scroller->currentContentIndex())) {
    _this->SetFocusedPanel(ChatChannelCategory::Alliance);
    FocusAlliance(_this, tabs.alliance);
  }
}

void ChatPreviewController_OnPanelFocused_Hook(auto original, ChatPreviewController* _this, int32_t index)
{
  const auto tabs = ChatTabs::Current();
  if (!tabs.Disabled(index))
    return original(_this, index);
  // Swiped onto a disabled channel: focus alliance before the game handles the change, then line the scroller up.
  _this->SetFocusedPanel(ChatChannelCategory::Alliance);
  original(_this, tabs.alliance);
  FocusAlliance(_this, tabs.alliance);
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
  auto* title     = profile ? profile->playerTitleWidget() : nullptr;
  auto* transform = title ? profile->transform() : nullptr;
  if (!transform || !transform->FindInParents<ChatMessageWidget>())
    return;
  if (auto* object = title->gameObject())
    object->SetActive(false);
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
  if (auto* transform = _this->transform())
    HideChatTitle(transform->FindInParents<UserProfileWidget>());
}

bool InstallChat()
{
  il2cpp::HookSet hooks("Chat");
  IL2CPP_STAGE(hooks, FullScreenChatViewController::Class::Method("AboutToShow"),
               FullScreenChatViewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, FullScreenChatViewController::Class::Method("OnDidChangeSelectedTab"),
               FullScreenChatViewController_OnDidChangeSelectedTab_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method("AboutToShow"), ChatPreviewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method("OnPanelFocused"),
               ChatPreviewController_OnPanelFocused_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method("OnGlobalMessageReceived"),
               ChatPreviewController_OnGlobalMessageReceived_Hook);
  IL2CPP_STAGE(hooks, ChatPreviewController::Class::Method("OnRegionalMessageReceived"),
               ChatPreviewController_OnRegionalMessageReceived_Hook);
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
  void*           update_entry = il2cpp::CheckedEntry(UserProfileWidget::Class::Method("UpdatePlayerTitle", 0));
  void*           bind_entry   = il2cpp::CheckedEntry(PlayerTitleWidget::Class::Method("SetIdentifiers", 0));
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
  auto* chat = ChatManager::Instance();
  if (!chat)
    return;

  // With side chat already open, the shortcut focuses its input instead.
  if (chat->IsSideChatOpen()) {
    auto* view = unity::Newest<FullScreenChatViewController>();
    auto* list = view ? view->messageList() : nullptr;
    if (auto* input = list ? list->inputField() : nullptr)
      input->ActivateInputField();
  } else {
    chat->OpenChannel(ChatChannelCategory::Alliance, side ? ChatViewMode::Side : ChatViewMode::Fullscreen);
  }
}

bool ChatChannelShortcuts()
{
  auto* chat = ChatManager::Instance();
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
