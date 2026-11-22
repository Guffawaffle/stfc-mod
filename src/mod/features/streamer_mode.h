#pragma once

#include "config.h"

namespace streamer_mode
{
// Call these presentation switches on the game thread.
inline bool HideAccount()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_account;
}
inline bool HideChatNames()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_chat_names;
}

bool SelfProfileAvailable();
bool SelfProfileDegraded();
bool GeneralAccountIdsAvailable();
bool GeneralAccountIdsDegraded();
void RefreshGeneralAccountIds();
bool StandaloneIdAvailable();
bool StandaloneIdDegraded();
bool ChatNamesAvailable();
bool ChatNamesDegraded();
bool CompactPlayerShipLabelsAvailable();
} // namespace streamer_mode
