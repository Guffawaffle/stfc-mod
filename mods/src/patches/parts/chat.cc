// disable_galaxy_chat and disable_veil_chat: greys out those tabs in full-screen chat, keeps the chat preview off
// them, and drops their messages from the preview.

#include "config.h"
#include "patches/patches.h"

#include <prime/chat.h>
#include <prime/ui.h>

#include <spud/detour.h>

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
  auto*      scroller = _this ? _this->swipeScroller() : nullptr;
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
} // namespace

void InstallChatPatches()
{
  if (auto m = FullScreenChatViewController::Class::Method("AboutToShow"))
    SPUD_STATIC_DETOUR(m.Pointer(), FullScreenChatViewController_AboutToShow_Hook);
  if (auto m = FullScreenChatViewController::Class::Method("OnDidChangeSelectedTab"))
    SPUD_STATIC_DETOUR(m.Pointer(), FullScreenChatViewController_OnDidChangeSelectedTab_Hook);
  if (auto m = ChatPreviewController::Class::Method("AboutToShow"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_AboutToShow_Hook);
  if (auto m = ChatPreviewController::Class::Method("OnPanelFocused"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnPanelFocused_Hook);
  if (auto m = ChatPreviewController::Class::Method("OnGlobalMessageReceived"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnGlobalMessageReceived_Hook);
  if (auto m = ChatPreviewController::Class::Method("OnRegionalMessageReceived"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnRegionalMessageReceived_Hook);
}
