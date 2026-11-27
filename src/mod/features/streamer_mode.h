#pragma once

#include "config.h"

namespace streamer_mode
{
// Shared presentation decisions. Unknown ownership must not expose a requested personal mask.
enum class Subject { Self, Other, Unknown };

struct AccountPolicy {
  bool player = false, account = false;
  bool operator==(const AccountPolicy&) const = default;
  bool Any() const
  { return player || account; }
};

struct ProfilePolicy {
  bool name = false, alliance = false;
  bool operator==(const ProfilePolicy&) const = default;
  bool Any() const
  { return name || alliance; }
};

struct ChatVisibility {
  bool name = false, alliance = false;
};

struct ChatPolicy {
  bool names = false, alliances = false, my_name = false, my_alliance = false;
  bool operator==(const ChatPolicy&) const = default;
  bool Any() const
  { return names || alliances || my_name || my_alliance; }
  ChatVisibility For(Subject subject) const
  { return {names || (my_name && subject != Subject::Other), alliances || (my_alliance && subject != Subject::Other)}; }
};

// Call these presentation switches on the game thread; never retain the reloadable config.
inline AccountPolicy AccountProtections()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode ? AccountPolicy{ui.streamer_hide_player_id, ui.streamer_hide_account_id} : AccountPolicy{};
}
inline ProfilePolicy SelfProfileProtections()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode ? ProfilePolicy{ui.streamer_hide_my_name, ui.streamer_hide_my_alliance} : ProfilePolicy{};
}
inline ChatPolicy ChatProtections()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode ? ChatPolicy{ui.streamer_hide_chat_sender_names, ui.streamer_hide_chat_alliances,
                                       ui.streamer_hide_my_name, ui.streamer_hide_my_alliance}
                          : ChatPolicy{};
}
inline bool HidePlayerId()
{ return AccountProtections().player; }
inline bool HideAccountId()
{ return AccountProtections().account; }
inline bool HideMyName()
{ return SelfProfileProtections().name; }
inline bool HideMyAlliance()
{ return SelfProfileProtections().alliance; }
inline bool HideChatNames()
{ return ChatProtections().names; }
inline bool HideChatAlliances()
{ return ChatProtections().alliances; }

bool SelfProfileAvailable();
bool SelfProfileDegraded();
bool GeneralAccountIdsAvailable();
bool GeneralAccountIdsDegraded();
void RefreshGeneralAccountIds();
bool StandaloneIdAvailable();
bool StandaloneIdDegraded();
bool ChatNamesAvailable();
bool ChatNamesDegraded();
bool ChatAlliancesAvailable();
bool ChatAlliancesDegraded();
bool CompactPlayerShipLabelsAvailable();
} // namespace streamer_mode
