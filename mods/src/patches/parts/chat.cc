#include "prime/chat.h"
#include "prime/ui.h"

#include "config.h"

#include <spud/detour.h>
#include <tuple>

auto GetChatTabIndices()
{
  if (const auto chat_manager = ChatManager::Instance(); chat_manager) {
    const auto hasCadetChat    = chat_manager->CanSeeNewbieChat();
    const auto hasVeilChat     = chat_manager->CanSeeRegionalChat();

    const auto galaxyChatIndex = 0 + (hasCadetChat ? 1 : 0);
    const auto veilChatIndex   = galaxyChatIndex + 1;
    const auto allianceChatIndex = (hasCadetChat ? 1 : 0) + 1 + (hasVeilChat ? 1 : 0);

    return std::make_tuple(hasCadetChat ? 0 : -1, galaxyChatIndex, hasVeilChat ? veilChatIndex : -1, allianceChatIndex);
  }

  return std::make_tuple(-1, 0, -1, 1);
}

void DisableButtons(FullScreenChatViewController* _this)
{
  const auto disableGalaxyChat = Config::Get().ui.disable_galaxy_chat;
  const auto disableVeilChat = Config::Get().ui.disable_veil_chat;

  if (!(disableGalaxyChat || disableVeilChat))
    return;

  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();

  const auto viewController = _this->categoriesTabBarViewController();
  if (!viewController) {
    return;
  }

  const auto tabBar = viewController->tabBar();
  if (!tabBar) {
    return;
  }

  const auto listData = tabBar->data();
  if (!listData) {
    return;
  }

  if (disableGalaxyChat) {
    if (const auto elm = (*listData)[galaxyChatIdx]) {
      static_cast<GenericButtonContext*>(elm)->SetInteractable(false);
    }
  }

  if (disableVeilChat && veilChatIdx != -1) {
    if (const auto elm = (*listData)[veilChatIdx]) {
      static_cast<GenericButtonContext*>(elm)->SetInteractable(false);
    }
  }
}

void FullScreenChatViewController_AboutToShow(auto original, FullScreenChatViewController* _this)
{
  original(_this);
  DisableButtons(_this);
}

void FullScreenChatViewController_OnDidChangeSelectedTab(auto original, FullScreenChatViewController* _this, int32_t tabIdx, void* tab)
{
  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();
  if ((tabIdx == galaxyChatIdx && Config::Get().ui.disable_galaxy_chat) || (tabIdx == veilChatIdx && Config::Get().ui.disable_veil_chat)) {
    // don't show disabled chats if the associated tab was selected
    return;
  }

  original(_this, tabIdx, tab);
}

void ChatPreviewController_AboutToShow(auto original, ChatPreviewController* _this)
{
  original(_this);

  if (Config::Get().ui.disable_galaxy_chat || Config::Get().ui.disable_veil_chat) {
    const auto allianceChatIdx = std::get<3>(GetChatTabIndices());
    _this->SetFocusedPanel(ChatChannelCategory::Alliance);

    if (_this->swipeScroller()->currentContentIndex() != allianceChatIdx) {
      _this->swipeScroller()->FocusOnInstantly(allianceChatIdx);
    }
  }
}

void ChatPreviewController_OnPanelFocused(auto original, ChatPreviewController* _this, int32_t index)
{
  static const auto disableGalaxyChat = Config::Get().ui.disable_galaxy_chat;
  static const auto disableVeilChat   = Config::Get().ui.disable_veil_chat;

  if (!(disableGalaxyChat || disableVeilChat)) {
    original(_this, index);
    return;
  }

  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();
  if (disableGalaxyChat || (veilChatIdx != -1 && disableVeilChat)) {
    _this->SetFocusedPanel(ChatChannelCategory::Alliance);
    original(_this, allianceChatIdx);

    if (_this->swipeScroller()->currentContentIndex() != allianceChatIdx) {
      _this->swipeScroller()->FocusOnInstantly(allianceChatIdx);
    }

    return;
  }

  original(_this, index);
}

void ChatPreviewController_OnGlobalMessageReceived(auto original, ChatPreviewController* _this, void* message)
{
  if (Config::Get().ui.disable_galaxy_chat)
    return;

  original(_this, message);
}

void ChatPreviewController_OnRegionalMessageReceived(auto original, ChatPreviewController* _this, void* message)
{
  if (Config::Get().ui.disable_veil_chat)
    return;

  original(_this, message);
}

void InstallChatPatches()
{
  if (auto m = FullScreenChatViewController::Class::Method("AboutToShow"))
    SPUD_STATIC_DETOUR(m.Pointer(), FullScreenChatViewController_AboutToShow);
  if (auto m = FullScreenChatViewController::Class::Method("OnDidChangeSelectedTab"))
    SPUD_STATIC_DETOUR(m.Pointer(), FullScreenChatViewController_OnDidChangeSelectedTab);

  if (auto m = ChatPreviewController::Class::Method("AboutToShow"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_AboutToShow);
  if (auto m = ChatPreviewController::Class::Method("OnPanelFocused"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnPanelFocused);
  if (auto m = ChatPreviewController::Class::Method("OnGlobalMessageReceived"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnGlobalMessageReceived);
  if (auto m = ChatPreviewController::Class::Method("OnRegionalMessageReceived"))
    SPUD_STATIC_DETOUR(m.Pointer(), ChatPreviewController_OnRegionalMessageReceived);
}
