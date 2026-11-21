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
inline bool HideChat()
{
  const auto& ui = Config::Get().ui;
  return ui.streamer_mode && ui.streamer_hide_chat;
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
bool LocationsAvailable();
bool ChatAvailable();
bool ReportsAvailable();
bool BalancesAvailable();
} // namespace streamer_mode
