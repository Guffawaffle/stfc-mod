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
inline bool HideIdentity()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_identity;
}
inline bool HideLocations()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_locations;
}
inline bool HideChatNames()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_chat_names;
}
inline bool HideReports()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_reports;
}
inline bool HideBalances()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_balances;
}

bool IdentityAvailable();
bool IdentityDegraded();
bool LocationsAvailable();
bool LocationsDegraded();
bool ChatNamesAvailable();
bool ChatNamesDegraded();
bool ReportsAvailable();
bool ReportsDegraded();
bool BalancesAvailable();
bool BalancesDegraded();
} // namespace streamer_mode
