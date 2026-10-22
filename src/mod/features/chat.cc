// Chat: the shortcuts that open it and switch channel; [ui] disable_galaxy_chat and disable_veil_chat, which grey
// out those tabs in full-screen chat, keep the chat preview off them, and drop their messages from the preview; and
// [ui] show_player_title, which can hide the title row under each sender's name.

#include "config.h"
#include "features/features.h"
#include "shortcuts.h"

#include "game/chat.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <unity/core.h>
#include <unity/tracker.h>

namespace
{
// Tab positions in both the full-screen chat and the preview. Cadet and veil chat only exist for some players,
// and shift the tabs after them; a missing tab is -1.
struct ChatTabs {
  int32_t cadet    = -1;
  int32_t galaxy   = 0;
  int32_t veil     = -1;
  int32_t alliance = 1;

  static ChatTabs Current()
  {
    ChatTabs tabs;
    auto*    chat = ChatManager::Instance();
    if (!chat)
      return tabs;
    const bool cadet = chat->CanSeeNewbieChat();
    const bool veil  = chat->CanSeeRegionalChat();
    tabs.cadet       = cadet ? 0 : -1;
    tabs.galaxy      = cadet ? 1 : 0;
    tabs.veil        = veil ? tabs.galaxy + 1 : -1;
    tabs.alliance    = tabs.galaxy + 1 + (veil ? 1 : 0);
    return tabs;
  }

  // Whether `index` is a tab the settings disable.
  bool Disabled(int32_t index) const
  {
    const auto& ui = Config::Get().ui;
    return (ui.disable_galaxy_chat && index == galaxy) || (ui.disable_veil_chat && veil != -1 && index == veil);
  }
};

bool AnyDisabled()
{ return Config::Get().ui.disable_galaxy_chat || Config::Get().ui.disable_veil_chat; }

void FullScreenChatViewController_AboutToShow_Hook(auto original, FullScreenChatViewController* _this)
{
  original(_this);
  if (!AnyDisabled())
    return;

  auto* tab_bar_view = _this->categoriesTabBarViewController();
  auto* tab_bar      = tab_bar_view ? tab_bar_view->tabBar() : nullptr;
  auto* buttons      = tab_bar ? tab_bar->data() : nullptr;
  if (!buttons)
    return;
  const auto tabs = ChatTabs::Current();
  for (int32_t index : {tabs.galaxy, tabs.veil})
    if (index >= 0 && index < buttons->Count() && tabs.Disabled(index))
      if (auto* button = (*buttons)[index])
        static_cast<GenericButtonContext*>(button)->SetInteractable(false);
}

// Selecting a disabled tab (with the keyboard, say) does nothing.
void FullScreenChatViewController_OnDidChangeSelectedTab_Hook(auto original, FullScreenChatViewController* _this,
                                                              int32_t index, void* tab)
{
  if (!ChatTabs::Current().Disabled(index))
    original(_this, index, tab);
}

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

// SetWidgetData and OnDidBindContext both bind a title from the title widget's side; one hook serves both.
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

// These targets are small, so on macOS each is checked against SPUD's patch first.
bool InstallChatTitles()
{
  void*           update_entry = il2cpp::CheckedEntry(UserProfileWidget::Class::Method("UpdatePlayerTitle", 0));
  il2cpp::HookSet hooks("ChatTitles");
  IL2CPP_STAGE_AT(hooks, update_entry, UserProfileWidget_UpdatePlayerTitle_Hook);
  // The feature is the update hook; the bind paths below only add to it.
  const bool installed = hooks.Commit();
  // The bind paths catch titles bound without an update, so they are a set of their own and add to the update hook
  // where they fit.
  const auto set_data = PlayerTitleWidget::Class::Method("SetWidgetData", 0);
  const auto bind     = PlayerTitleWidget::Class::Method("OnDidBindContext", 0);
  if (!set_data || !bind)
    return installed;
  // Both, or one of them and the update, often branch to one shared body. One detour on it covers them all; a second
  // would run the same body's hook twice, once with a receiver of the wrong class. An entry too small for a detour
  // is reported by CheckedEntry and left out.
  void*           set_entry  = il2cpp::CheckedEntry(set_data);
  void*           bind_entry = il2cpp::CheckedEntry(bind);
  il2cpp::HookSet bind_hooks("ChatTitles");
  if (set_entry && set_entry != update_entry)
    IL2CPP_STAGE_AT(bind_hooks, set_entry, PlayerTitleWidget_Bind_Hook);
  if (bind_entry && bind_entry != update_entry && bind_entry != set_entry)
    IL2CPP_STAGE_AT(bind_hooks, bind_entry, PlayerTitleWidget_Bind_Hook);
  bind_hooks.Commit();
  return installed;
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
    auto* view  = unity::Newest<FullScreenChatViewController>();
    auto* list  = view ? view->messageList() : nullptr;
    auto* input = list ? list->inputField() : nullptr;
    if (input)
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
  for (auto [shortcut, channel] : {std::pair{Action::kSelectChatGlobal, ChatChannelCategory::Global},
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
const Feature kChatTitles{"ChatTitles", &Config::Patches::chattitledisplayhooks, InstallChatTitles};
