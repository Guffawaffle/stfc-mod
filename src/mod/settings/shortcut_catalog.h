#pragma once
#include "page_catalog.h"
#include "shortcuts.h"
#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mod_settings
{
// Change + Remove per binding, Add, status/Undo, Restore, optional explanation
// and save notice. Retain the established binding limit with three spare slots.
// Oversized player-authored lists stay live; the editor presents their prefix.
inline constexpr std::size_t ShortcutFixedRowLimit       = 8;
inline constexpr std::size_t ShortcutBindingDisplayLimit = (PageCatalog::NativeChildLimit - ShortcutFixedRowLimit) / 2;
constexpr std::size_t        VisibleShortcutBindingCount(std::size_t count)
{ return std::min(count, ShortcutBindingDisplayLimit); }
constexpr bool ShortcutCountFitsEdit(std::size_t before, std::size_t after)
{ return after <= ShortcutBindingDisplayLimit || after <= before; }
enum class ShortcutGroup {
  Screens,
  Previews,
  Interface,
  Fleet,
  Travel,
  Camera,
  Chat,
  Client,
  Uncategorized,
  Diagnostics
};
struct ShortcutGroupInfo {
  ShortcutGroup    group;
  std::string_view id, label;
};
// Category registration is display order: alphabetized, with Diagnostics last.
inline constexpr auto ShortcutGroups = std::to_array<ShortcutGroupInfo>({
    {ShortcutGroup::Camera, "camera", "Camera"},
    {ShortcutGroup::Chat, "chat", "Chat"},
    {ShortcutGroup::Client, "client", "Client"},
    {ShortcutGroup::Fleet, "fleet", "Fleet Controls"},
    {ShortcutGroup::Screens, "screens", "Game Screens"},
    {ShortcutGroup::Interface, "interface", "Interface Controls"},
    {ShortcutGroup::Travel, "travel", "Map & Travel"},
    {ShortcutGroup::Previews, "previews", "Previews & Cargo"},
    {ShortcutGroup::Uncategorized, "uncategorized", "Uncategorized"},
    {ShortcutGroup::Diagnostics, "diagnostics", "Diagnostics"},
});
struct ShortcutInfo {
  Action           action;
  ShortcutGroup    group;
  std::string_view label;
};
constexpr std::string_view ShortcutExplanation(Action action)
{
  switch (action) {
    case Action::kMoveUp:
    case Action::kMoveDown:
      return "Pan the system or galaxy map. Disabled when keyboard movement is turned off.";
    case Action::kMoveLeft:
    case Action::kMoveRight:
      return "Pan the map, rotate the station exterior, or change selection in supported screens.";
    case Action::kQuit:
      return "Force closes the client; allows up to 0.5s for pending saves.";
    case Action::kNativeShortcutGalaxy:
    case Action::kNativeShortcutEvents:
      return "Uses the game's own shortcut behavior for this screen.";
    case Action::kHavenOrbitDrag:
      return "Hold and drag on the Haven map: sideways rotates, up/down adjusts tilt. Release to keep the view.";
    case Action::kHavenOrbitReset:
      return "Restore Haven's native rotation and tilt.";
    case Action::kNavigationOrbitDrag:
      return "Hold and drag on the system map: sideways rotates, up/down adjusts tilt (5 to 90 degrees).";
    case Action::kNavigationOrbitReset:
      return "Restore the system map's native rotation and tilt.";
    default:
      return {};
  }
}
// Optional presentation overrides. Missing actions use their registered config
// key for a fallback label in Uncategorized. Config/defaults/dispatch/save
// identities remain owned by the parser/config; opening a screen is a UI action.
inline constexpr auto ShortcutCatalog = std::to_array<ShortcutInfo>({
    {Action::kMoveLeft, ShortcutGroup::Camera, "Pan left"},
    {Action::kMoveRight, ShortcutGroup::Camera, "Pan right"},
    {Action::kMoveUp, ShortcutGroup::Camera, "Pan up"},
    {Action::kMoveDown, ShortcutGroup::Camera, "Pan down"},
    {Action::kSelectChatAlliance, ShortcutGroup::Chat, "Switch to alliance chat"},
    {Action::kSelectChatGlobal, ShortcutGroup::Chat, "Switch to global chat"},
    {Action::kSelectChatPrivate, ShortcutGroup::Chat, "Switch to private chat"},
    {Action::kSelectShip1, ShortcutGroup::Fleet, "Select ship 1"},
    {Action::kSelectShip2, ShortcutGroup::Fleet, "Select ship 2"},
    {Action::kSelectShip3, ShortcutGroup::Fleet, "Select ship 3"},
    {Action::kSelectShip4, ShortcutGroup::Fleet, "Select ship 4"},
    {Action::kSelectShip5, ShortcutGroup::Fleet, "Select ship 5"},
    {Action::kSelectShip6, ShortcutGroup::Fleet, "Select ship 6"},
    {Action::kSelectShip7, ShortcutGroup::Fleet, "Select ship 7"},
    {Action::kSelectShip8, ShortcutGroup::Fleet, "Select ship 8"},
    {Action::kSelectCurrent, ShortcutGroup::Fleet, "Locate selected ship"},
    {Action::kShowAlliance, ShortcutGroup::Screens, "Open alliance"},
    {Action::kShowAllianceArmada, ShortcutGroup::Screens, "Open alliance armadas"},
    {Action::kShowAllianceHelp, ShortcutGroup::Screens, "Open alliance help"},
    {Action::kShowArtifacts, ShortcutGroup::Screens, "Open artifacts"},
    {Action::kShowOfficers, ShortcutGroup::Screens, "Open officers"},
    {Action::kShowOfficerPresets, ShortcutGroup::Screens, "Open officer presets"},
    {Action::kShowOfficerSwap, ShortcutGroup::Fleet, "Swap officers"},
    {Action::kShowShipSwap, ShortcutGroup::Fleet, "Swap ship"},
    {Action::kShowHaven, ShortcutGroup::Screens, "Open Haven"},
    {Action::kShowCommander, ShortcutGroup::Screens, "Open Fleet Commanders"},
    {Action::kShowRefinery, ShortcutGroup::Screens, "Open refinery"},
    {Action::kShowQTrials, ShortcutGroup::Screens, "Open Q's Trials"},
    {Action::kShowBookmarks, ShortcutGroup::Travel, "Open bookmarks"},
    {Action::kShowLookup, ShortcutGroup::Travel, "Find coordinates"},
    {Action::kShowExoComp, ShortcutGroup::Screens, "Open Exocomps"},
    {Action::kShowFactions, ShortcutGroup::Screens, "Open factions"},
    {Action::kShowGifts, ShortcutGroup::Screens, "Open gifts"},
    {Action::kShowDaily, ShortcutGroup::Screens, "Open daily goals"},
    {Action::kShowAwayTeam, ShortcutGroup::Screens, "Open Away Teams"},
    {Action::kShowMissions, ShortcutGroup::Screens, "Open missions"},
    {Action::kShowResearch, ShortcutGroup::Screens, "Open research"},
    {Action::kShowScrapYard, ShortcutGroup::Screens, "Open scrapyard"},
    {Action::kShowShips, ShortcutGroup::Screens, "Manage selected ship"},
    {Action::kShowInventory, ShortcutGroup::Screens, "Open inventory"},
    {Action::kShowStationInterior, ShortcutGroup::Travel, "View station interior"},
    {Action::kShowStationExterior, ShortcutGroup::Travel, "View station exterior"},
    {Action::kShowGalaxy, ShortcutGroup::Travel, "View galaxy"},
    {Action::kNativeShortcutGalaxy, ShortcutGroup::Travel, "View galaxy (native shortcut)"},
    {Action::kShowSystem, ShortcutGroup::Travel, "View system"},
    {Action::kShowChat, ShortcutGroup::Chat, "Open chat"},
    {Action::kShowChatSide1, ShortcutGroup::Chat, "Open side chat 1"},
    {Action::kShowChatSide2, ShortcutGroup::Chat, "Open side chat 2"},
    {Action::kShowEvents, ShortcutGroup::Screens, "Open events"},
    {Action::kNativeShortcutEvents, ShortcutGroup::Screens, "Open events (native shortcut)"},
    {Action::kShowSettings, ShortcutGroup::Screens, "Open settings"},
    {Action::kToggleShortcutHints, ShortcutGroup::Interface, "Toggle shortcut hints"},
    {Action::kZoomPreset1, ShortcutGroup::Camera, "Use zoom preset 1"},
    {Action::kZoomPreset2, ShortcutGroup::Camera, "Use zoom preset 2"},
    {Action::kZoomPreset3, ShortcutGroup::Camera, "Use zoom preset 3"},
    {Action::kZoomPreset4, ShortcutGroup::Camera, "Use zoom preset 4"},
    {Action::kZoomPreset5, ShortcutGroup::Camera, "Use zoom preset 5"},
    {Action::kZoomIn, ShortcutGroup::Camera, "Zoom in"},
    {Action::kZoomOut, ShortcutGroup::Camera, "Zoom out"},
    {Action::kZoomMin, ShortcutGroup::Camera, "Zoom to minimum"},
    {Action::kZoomMax, ShortcutGroup::Camera, "Zoom to maximum"},
    {Action::kHavenOrbitDrag, ShortcutGroup::Camera, "Rotate and tilt Haven (hold)"},
    {Action::kHavenOrbitReset, ShortcutGroup::Camera, "Reset Haven rotation and tilt"},
    {Action::kNavigationOrbitDrag, ShortcutGroup::Camera, "Rotate and tilt system (hold)"},
    {Action::kNavigationOrbitReset, ShortcutGroup::Camera, "Reset system rotation and tilt"},
    {Action::kZoomReset, ShortcutGroup::Camera, "Reset zoom"},
    {Action::kUiScaleUp, ShortcutGroup::Interface, "Increase interface size"},
    {Action::kUiScaleDown, ShortcutGroup::Interface, "Decrease interface size"},
    {Action::kUiShipScaleUp, ShortcutGroup::Interface, "Increase ship panel size"},
    {Action::kUiShipScaleDown, ShortcutGroup::Interface, "Decrease ship panel size"},
    {Action::kUiViewerScaleUp, ShortcutGroup::Interface, "Increase viewer size"},
    {Action::kUiViewerScaleDown, ShortcutGroup::Interface, "Decrease viewer size"},
    {Action::kActionPrimary, ShortcutGroup::Fleet, "Primary action"},
    {Action::kActionSecondary, ShortcutGroup::Fleet, "Secondary action"},
    {Action::kActionQueue, ShortcutGroup::Fleet, "Queue action"},
    {Action::kActionQueueClear, ShortcutGroup::Fleet, "Clear action queue"},
    {Action::kActionView, ShortcutGroup::Fleet, "View target details"},
    {Action::kActionRecall, ShortcutGroup::Fleet, "Recall selected ship"},
    {Action::kActionRecallCancel, ShortcutGroup::Fleet, "Cancel recall"},
    {Action::kActionRepair, ShortcutGroup::Fleet, "Repair selected ship"},
    {Action::kSetZoomPreset1, ShortcutGroup::Camera, "Save zoom preset 1"},
    {Action::kSetZoomPreset2, ShortcutGroup::Camera, "Save zoom preset 2"},
    {Action::kSetZoomPreset3, ShortcutGroup::Camera, "Save zoom preset 3"},
    {Action::kSetZoomPreset4, ShortcutGroup::Camera, "Save zoom preset 4"},
    {Action::kSetZoomPreset5, ShortcutGroup::Camera, "Save zoom preset 5"},
    {Action::kSetZoomDefault, ShortcutGroup::Camera, "Save default zoom"},
    {Action::kDisableHotKeys, ShortcutGroup::Client, "Disable mod shortcuts"},
    {Action::kEnableHotKeys, ShortcutGroup::Client, "Enable mod shortcuts"},
    {Action::kToggleQueue, ShortcutGroup::Fleet, "Toggle action queue"},
    {Action::kToggleAutoConfirmInstantWarp, ShortcutGroup::Travel, "Cycle instant warp mode"},
    {Action::kTogglePreviewLocate, ShortcutGroup::Previews, "Toggle Locate on previews"},
    {Action::kTogglePreviewRecall, ShortcutGroup::Previews, "Toggle Recall on previews"},
    {Action::kToggleCargoDefault, ShortcutGroup::Previews, "Toggle automatic cargo previews"},
    {Action::kToggleCargoPlayer, ShortcutGroup::Previews, "Toggle player cargo previews"},
    {Action::kToggleCargoStation, ShortcutGroup::Previews, "Toggle station cargo previews"},
    {Action::kToggleCargoHostile, ShortcutGroup::Previews, "Toggle hostile cargo previews"},
    {Action::kToggleCargoArmada, ShortcutGroup::Previews, "Toggle armada cargo previews"},
    {Action::kLogLevelDebug, ShortcutGroup::Diagnostics, "Set logging to Debug"},
    {Action::kLogLevelInfo, ShortcutGroup::Diagnostics, "Set logging to Info"},
    {Action::kLogLevelTrace, ShortcutGroup::Diagnostics, "Set logging to Trace"},
    {Action::kLogLevelError, ShortcutGroup::Diagnostics, "Set logging to Error"},
    {Action::kLogLevelWarn, ShortcutGroup::Diagnostics, "Set logging to Warning"},
    {Action::kLogLevelOff, ShortcutGroup::Diagnostics, "Turn logging off"},
    {Action::kRestart, ShortcutGroup::Client, "Clear localization cache and reload"},
    {Action::kQuit, ShortcutGroup::Client, "Force close client"},
    {Action::kFocusSearch, ShortcutGroup::Interface, "Focus search"},
    {Action::kShowShipConstruction, ShortcutGroup::Screens, "Open ship construction"},
    {Action::kShowShields, ShortcutGroup::Screens, "Open shields"},
    {Action::kShowBattlelogs, ShortcutGroup::Screens, "Open battle logs"},
});
constexpr bool        ValidShortcutCatalog(std::span<const ShortcutInfo> catalog)
{
  std::array<bool, std::to_underlying(Action::kCount)> seen{};
  for (const auto& info : catalog) {
    if (std::to_underlying(info.action) >= std::to_underlying(Action::kCount) || info.label.empty()
        || seen[std::to_underlying(info.action)])
      return false;
    bool groupFound = false;
    for (const auto& group : ShortcutGroups)
      groupFound |= group.group == info.group;
    if (!groupFound)
      return false;
    seen[std::to_underlying(info.action)] = true;
  }
  return true;
}
static_assert(ValidShortcutCatalog(ShortcutCatalog), "Shortcut overrides need unique valid actions, labels and groups");

inline std::string HumanizeShortcutKey(std::string_view key)
{
  std::string label;
  bool        space = false;
  for (char c : key) {
    if (c == '_' || c == '-' || c == ' ') {
      space = !label.empty();
      continue;
    }
    if (space)
      label += ' ';
    space = false;
    label += label.empty() && c >= 'a' && c <= 'z' ? static_cast<char>(c - ('a' - 'A')) : c;
  }
  return label.empty() ? "Shortcut" : label;
}

// Own generated text: descriptions survive sorting and never borrow temporary
// labels. The same resolver names editors, summaries and overlap warnings.
struct ShortcutDescription {
  Action        action;
  ShortcutGroup group;
  std::string   label;
  bool          fallback;
};
inline ShortcutDescription DescribeShortcut(Action action, std::string_view key,
                                            std::span<const ShortcutInfo> catalog = ShortcutCatalog)
{
  if (std::to_underlying(action) >= std::to_underlying(Action::kCount))
    throw std::out_of_range("shortcut action");
  for (const auto& info : catalog)
    if (info.action == action)
      return {action, info.group, std::string(info.label), false};
  return {action, ShortcutGroup::Uncategorized, HumanizeShortcutKey(key), true};
}

// Discover registered actions once at startup, including unbound (NONE) actions.
// An empty key excludes an unregistered or unavailable action. This never scans
// TOML or infers a setting type from its value.
inline std::vector<ShortcutDescription> DiscoverShortcuts(const auto&                   registeredKey,
                                                          std::span<const ShortcutInfo> catalog = ShortcutCatalog)
{
  std::vector<ShortcutDescription> result;
  for (int i = 0; i < std::to_underlying(Action::kCount); ++i) {
    const auto action = static_cast<Action>(i);
    const auto key    = registeredKey(action);
    if (!key.empty())
      result.push_back(DescribeShortcut(action, key, catalog));
  }
  // ASCII folding preserves existing English label ordering (Away Teams, etc.).
  std::stable_sort(result.begin(), result.end(), [](const auto& first, const auto& second) {
    return std::lexicographical_compare(
        first.label.begin(), first.label.end(), second.label.begin(), second.label.end(), [](char a, char b) {
          const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
          return lower(a) < lower(b);
        });
  });
  return result;
}
} // namespace mod_settings
