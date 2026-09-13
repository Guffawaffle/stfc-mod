#pragma once
#include "page_catalog.h"
#include "patches/gamefunctions.h"
#include <array>
#include <stdexcept>
#include <string_view>

namespace mod_settings
{
// Change + Remove per binding, four singleton command rows and More options.
// Oversized player-authored lists stay live; the editor presents their prefix.
inline constexpr std::size_t ShortcutBindingDisplayLimit = (PageCatalog::NativeChildLimit - 5) / 2;
constexpr std::size_t        VisibleShortcutBindingCount(std::size_t count)
{ return std::min(count, ShortcutBindingDisplayLimit); }
constexpr bool ShortcutCountFitsEdit(std::size_t before, std::size_t after)
{ return after <= ShortcutBindingDisplayLimit || after <= before; }
enum class ShortcutGroup { Screens, Previews, Interface, Fleet, Travel, Camera, Chat, Client, Diagnostics };
struct ShortcutGroupInfo {
  ShortcutGroup    group;
  std::string_view id, label;
};
inline constexpr auto ShortcutGroups = std::to_array<ShortcutGroupInfo>({
    {ShortcutGroup::Screens, "screens", "Game Screens"},
    {ShortcutGroup::Previews, "previews", "Previews & Cargo"},
    {ShortcutGroup::Interface, "interface", "Interface Controls"},
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
    {ShowAlliance, ShortcutGroup::Screens, "Open alliance"},
    {ShowAllianceArmada, ShortcutGroup::Screens, "Open alliance armadas"},
    {ShowAllianceHelp, ShortcutGroup::Screens, "Open alliance help"},
    {ShowArtifacts, ShortcutGroup::Screens, "Open artifacts"},
    {ShowOfficers, ShortcutGroup::Screens, "Open officers"},
    {ShowCommander, ShortcutGroup::Screens, "Open Fleet Commanders"},
    {ShowRefinery, ShortcutGroup::Screens, "Open refinery"},
    {ShowQTrials, ShortcutGroup::Screens, "Open Q's Trials"},
    {ShowBookmarks, ShortcutGroup::Travel, "Open bookmarks"},
    {ShowLookup, ShortcutGroup::Travel, "Find coordinates"},
    {ShowExoComp, ShortcutGroup::Screens, "Open Exocomps"},
    {ShowFactions, ShortcutGroup::Screens, "Open factions"},
    {ShowGifts, ShortcutGroup::Screens, "Open gifts"},
    {ShowDaily, ShortcutGroup::Screens, "Open daily goals"},
    {ShowAwayTeam, ShortcutGroup::Screens, "Open Away Teams"},
    {ShowMissions, ShortcutGroup::Screens, "Open missions"},
    {ShowResearch, ShortcutGroup::Screens, "Open research"},
    {ShowScrapYard, ShortcutGroup::Screens, "Open scrapyard"},
    {ShowShips, ShortcutGroup::Screens, "Manage selected ship"},
    {ShowInventory, ShortcutGroup::Screens, "Open inventory"},
    {ShowStationInterior, ShortcutGroup::Travel, "View station interior"},
    {ShoWStationExterior, ShortcutGroup::Travel, "View station exterior"},
    {ShowGalaxy, ShortcutGroup::Travel, "View galaxy"},
    {NativeShortcutGalaxy, ShortcutGroup::Travel, "View galaxy (native shortcut)"},
    {ShowSystem, ShortcutGroup::Travel, "View system"},
    {ShowChat, ShortcutGroup::Chat, "Open chat"},
    {ShowChatSide1, ShortcutGroup::Chat, "Open side chat 1"},
    {ShowChatSide2, ShortcutGroup::Chat, "Open side chat 2"},
    {ShowEvents, ShortcutGroup::Screens, "Open events"},
    {NativeShortcutEvents, ShortcutGroup::Screens, "Open events (native shortcut)"},
    {ShowSettings, ShortcutGroup::Screens, "Open settings"},
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
    {TogglePreviewLocate, ShortcutGroup::Previews, "Toggle Locate on previews"},
    {TogglePreviewRecall, ShortcutGroup::Previews, "Toggle Recall on previews"},
    {ToggleCargoDefault, ShortcutGroup::Previews, "Toggle automatic cargo previews"},
    {ToggleCargoPlayer, ShortcutGroup::Previews, "Toggle player cargo previews"},
    {ToggleCargoStation, ShortcutGroup::Previews, "Toggle station cargo previews"},
    {ToggleCargoHostile, ShortcutGroup::Previews, "Toggle hostile cargo previews"},
    {ToggleCargoArmada, ShortcutGroup::Previews, "Toggle armada cargo previews"},
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
    {ShowShipConstruction, ShortcutGroup::Screens, "Open ship construction"},
    {ShowShields, ShortcutGroup::Screens, "Open shields"},
    {ShowBattlelogs, ShortcutGroup::Screens, "Open battle logs"},
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
