#pragma once
#include "patches/gamefunctions.h"
#include <array>
#include <stdexcept>
#include <string_view>

namespace mod_settings
{
enum class ShortcutGroup { Interface, Fleet, Travel, Camera, Chat, Client, Diagnostics };
struct ShortcutGroupInfo {
  ShortcutGroup    group;
  std::string_view id, label;
};
inline constexpr auto ShortcutGroups = std::to_array<ShortcutGroupInfo>({
    {ShortcutGroup::Interface, "interface", "User Interface"},
    {ShortcutGroup::Fleet, "fleet", "Fleet Controls"},
    {ShortcutGroup::Travel, "travel", "Map & Travel"},
    {ShortcutGroup::Camera, "camera", "Camera"},
    {ShortcutGroup::Chat, "chat", "Chat"},
    {ShortcutGroup::Client, "client", "Client"},
    {ShortcutGroup::Diagnostics, "diagnostics", "Diagnostics"},
});
struct ShortcutInfo {
  GameFunction     action;
  ShortcutGroup    group;
  std::string_view label;
};
// Presentation metadata is explicit. Config names, defaults, dispatch and save
// identities remain owned by MapKey/config; opening a screen is a UI action.
inline constexpr auto ShortcutCatalog = std::to_array<ShortcutInfo>({
    {MoveLeft, ShortcutGroup::Camera, "Pan left"},
    {MoveRight, ShortcutGroup::Camera, "Pan right"},
    {MoveUp, ShortcutGroup::Camera, "Pan up"},
    {MoveDown, ShortcutGroup::Camera, "Pan down"},
    {SelectChatAlliance, ShortcutGroup::Chat, "Switch to alliance chat"},
    {SelectChatGlobal, ShortcutGroup::Chat, "Switch to global chat"},
    {SelectChatPrivate, ShortcutGroup::Chat, "Switch to private chat"},
    {SelectShip1, ShortcutGroup::Fleet, "Select ship 1"},
    {SelectShip2, ShortcutGroup::Fleet, "Select ship 2"},
    {SelectShip3, ShortcutGroup::Fleet, "Select ship 3"},
    {SelectShip4, ShortcutGroup::Fleet, "Select ship 4"},
    {SelectShip5, ShortcutGroup::Fleet, "Select ship 5"},
    {SelectShip6, ShortcutGroup::Fleet, "Select ship 6"},
    {SelectShip7, ShortcutGroup::Fleet, "Select ship 7"},
    {SelectShip8, ShortcutGroup::Fleet, "Select ship 8"},
    {SelectCurrent, ShortcutGroup::Fleet, "Locate selected ship"},
    {ShowAlliance, ShortcutGroup::Interface, "Open alliance"},
    {ShowAllianceArmada, ShortcutGroup::Interface, "Open alliance armadas"},
    {ShowAllianceHelp, ShortcutGroup::Interface, "Open alliance help"},
    {ShowArtifacts, ShortcutGroup::Interface, "Open artifacts"},
    {ShowOfficers, ShortcutGroup::Interface, "Open officers"},
    {ShowCommander, ShortcutGroup::Interface, "Open Fleet Commanders"},
    {ShowRefinery, ShortcutGroup::Interface, "Open refinery"},
    {ShowQTrials, ShortcutGroup::Interface, "Open Q's Trials"},
    {ShowBookmarks, ShortcutGroup::Travel, "Open bookmarks"},
    {ShowLookup, ShortcutGroup::Travel, "Find coordinates"},
    {ShowExoComp, ShortcutGroup::Interface, "Open Exocomps"},
    {ShowFactions, ShortcutGroup::Interface, "Open factions"},
    {ShowGifts, ShortcutGroup::Interface, "Open gifts"},
    {ShowDaily, ShortcutGroup::Interface, "Open daily goals"},
    {ShowAwayTeam, ShortcutGroup::Interface, "Open Away Teams"},
    {ShowMissions, ShortcutGroup::Interface, "Open missions"},
    {ShowResearch, ShortcutGroup::Interface, "Open research"},
    {ShowScrapYard, ShortcutGroup::Interface, "Open scrapyard"},
    {ShowShips, ShortcutGroup::Interface, "Manage selected ship"},
    {ShowInventory, ShortcutGroup::Interface, "Open inventory"},
    {ShowStationInterior, ShortcutGroup::Travel, "View station interior"},
    {ShoWStationExterior, ShortcutGroup::Travel, "View station exterior"},
    {ShowGalaxy, ShortcutGroup::Travel, "View galaxy"},
    {NativeShortcutGalaxy, ShortcutGroup::Travel, "View galaxy (native shortcut)"},
    {ShowSystem, ShortcutGroup::Travel, "View system"},
    {ShowChat, ShortcutGroup::Chat, "Open chat"},
    {ShowChatSide1, ShortcutGroup::Chat, "Open side chat 1"},
    {ShowChatSide2, ShortcutGroup::Chat, "Open side chat 2"},
    {ShowEvents, ShortcutGroup::Interface, "Open events"},
    {NativeShortcutEvents, ShortcutGroup::Interface, "Open events (native shortcut)"},
    {ShowSettings, ShortcutGroup::Interface, "Open settings"},
    {ToggleShortcutHints, ShortcutGroup::Interface, "Toggle shortcut hints"},
    {ZoomPreset1, ShortcutGroup::Camera, "Use zoom preset 1"},
    {ZoomPreset2, ShortcutGroup::Camera, "Use zoom preset 2"},
    {ZoomPreset3, ShortcutGroup::Camera, "Use zoom preset 3"},
    {ZoomPreset4, ShortcutGroup::Camera, "Use zoom preset 4"},
    {ZoomPreset5, ShortcutGroup::Camera, "Use zoom preset 5"},
    {ZoomIn, ShortcutGroup::Camera, "Zoom in"},
    {ZoomOut, ShortcutGroup::Camera, "Zoom out"},
    {ZoomMin, ShortcutGroup::Camera, "Zoom to minimum"},
    {ZoomMax, ShortcutGroup::Camera, "Zoom to maximum"},
    {ZoomReset, ShortcutGroup::Camera, "Reset zoom"},
    {UiScaleUp, ShortcutGroup::Interface, "Increase interface size"},
    {UiScaleDown, ShortcutGroup::Interface, "Decrease interface size"},
    {UiShipScaleUp, ShortcutGroup::Interface, "Increase ship panel size"},
    {UiShipScaleDown, ShortcutGroup::Interface, "Decrease ship panel size"},
    {UiViewerScaleUp, ShortcutGroup::Interface, "Increase viewer size"},
    {UiViewerScaleDown, ShortcutGroup::Interface, "Decrease viewer size"},
    {ActionPrimary, ShortcutGroup::Fleet, "Primary action"},
    {ActionSecondary, ShortcutGroup::Fleet, "Secondary action"},
    {ActionQueue, ShortcutGroup::Fleet, "Queue action"},
    {ActionQueueClear, ShortcutGroup::Fleet, "Clear action queue"},
    {ActionView, ShortcutGroup::Fleet, "View target details"},
    {ActionRecall, ShortcutGroup::Fleet, "Recall selected ship"},
    {ActionRecallCancel, ShortcutGroup::Fleet, "Cancel recall"},
    {ActionRepair, ShortcutGroup::Fleet, "Repair selected ship"},
    {SetZoomPreset1, ShortcutGroup::Camera, "Save zoom preset 1"},
    {SetZoomPreset2, ShortcutGroup::Camera, "Save zoom preset 2"},
    {SetZoomPreset3, ShortcutGroup::Camera, "Save zoom preset 3"},
    {SetZoomPreset4, ShortcutGroup::Camera, "Save zoom preset 4"},
    {SetZoomPreset5, ShortcutGroup::Camera, "Save zoom preset 5"},
    {SetZoomDefault, ShortcutGroup::Camera, "Save default zoom"},
    {DisableHotKeys, ShortcutGroup::Client, "Disable mod shortcuts"},
    {EnableHotKeys, ShortcutGroup::Client, "Enable mod shortcuts"},
    {ToggleQueue, ShortcutGroup::Fleet, "Toggle action queue"},
    {ToggleAutoConfirmInstantWarp, ShortcutGroup::Travel, "Cycle instant warp mode"},
    {TogglePreviewLocate, ShortcutGroup::Interface, "Toggle Locate on previews"},
    {TogglePreviewRecall, ShortcutGroup::Interface, "Toggle Recall on previews"},
    {ToggleCargoDefault, ShortcutGroup::Interface, "Toggle automatic cargo previews"},
    {ToggleCargoPlayer, ShortcutGroup::Interface, "Toggle player cargo previews"},
    {ToggleCargoStation, ShortcutGroup::Interface, "Toggle station cargo previews"},
    {ToggleCargoHostile, ShortcutGroup::Interface, "Toggle hostile cargo previews"},
    {ToggleCargoArmada, ShortcutGroup::Interface, "Toggle armada cargo previews"},
    {LogLevelDebug, ShortcutGroup::Diagnostics, "Set logging to Debug"},
    {LogLevelInfo, ShortcutGroup::Diagnostics, "Set logging to Info"},
    {LogLevelTrace, ShortcutGroup::Diagnostics, "Set logging to Trace"},
    {LogLevelError, ShortcutGroup::Diagnostics, "Set logging to Error"},
    {LogLevelWarn, ShortcutGroup::Diagnostics, "Set logging to Warning"},
    {LogLevelOff, ShortcutGroup::Diagnostics, "Turn logging off"},
    {Restart, ShortcutGroup::Client, "Clear localization cache and reload"},
    {Quit, ShortcutGroup::Client, "Quit client"},
    {FocusSearch, ShortcutGroup::Interface, "Focus search"},
#ifdef _MODDBG
    {DevConsoleToggle, ShortcutGroup::Diagnostics, "Toggle console"},
    {DevConsoleClear, ShortcutGroup::Diagnostics, "Clear console"},
    {DevConsoleScrollUp, ShortcutGroup::Diagnostics, "Scroll console up"},
    {DevConsoleScrollDown, ShortcutGroup::Diagnostics, "Scroll console down"},
    {DevConsoleScrollLive, ShortcutGroup::Diagnostics, "Follow live console output"},
    {DevConsoleCycleOpacity, ShortcutGroup::Diagnostics, "Cycle console opacity"},
#endif
    {ShowShipConstruction, ShortcutGroup::Interface, "Open ship construction"},
    {ShowShields, ShortcutGroup::Interface, "Open shields"},
    {ShowBattlelogs, ShortcutGroup::Interface, "Open battle logs"},
});
consteval bool CompleteShortcutCatalog()
{
  std::array<bool, GameFunction::Max> seen{};
  for (const auto& info : ShortcutCatalog) {
    if (info.action < 0 || info.action >= GameFunction::Max || info.label.empty() || seen[info.action])
      return false;
    bool groupFound = false;
    for (const auto& group : ShortcutGroups)
      groupFound |= group.group == info.group;
    if (!groupFound)
      return false;
    seen[info.action] = true;
  }
  for (bool present : seen)
    if (!present)
      return false;
  return true;
}
static_assert(CompleteShortcutCatalog(), "Assign every shortcut a human label and impact group");
inline const ShortcutInfo& DescribeShortcut(GameFunction action)
{
  for (const auto& info : ShortcutCatalog)
    if (info.action == action)
      return info;
  throw std::out_of_range("shortcut metadata");
}
} // namespace mod_settings
