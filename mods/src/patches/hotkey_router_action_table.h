/**
 * @file hotkey_router_action_table.h
 * @brief Per-frame InputAction grouping tables consumed by the hotkey router.
 *
 * Each constexpr array enumerates the actions that one logical concern
 * (ship-selection, chat-open, table-dispatch, …) cares about during a frame.
 * `kHotkeyFrameActions` is the union of all groups — the dispatcher uses it to
 * derive the watched-key set so unrelated keys don't enter the per-frame
 * snapshot.
 *
 * Header-only on purpose: every `_inputs` consumer of these arrays just wants
 * compile-time data, not a translation-unit dependency.
 */
#pragma once

#include "patches/input_binding/action_registry.h"

#include <array>

namespace hotkey_router_actions
{
inline constexpr std::array kStartup{
    input_binding::InputActionId::HotkeysDisable,
    input_binding::InputActionId::HotkeysEnable,
};

inline constexpr std::array kQuit{
    input_binding::InputActionId::Quit,
};

inline constexpr std::array kQueue{
    input_binding::InputActionId::FleetQueueToggle,
};

inline constexpr std::array kShipSelection{
    input_binding::InputActionId::SelectShip1, input_binding::InputActionId::SelectShip2,
    input_binding::InputActionId::SelectShip3, input_binding::InputActionId::SelectShip4,
    input_binding::InputActionId::SelectShip5, input_binding::InputActionId::SelectShip6,
    input_binding::InputActionId::SelectShip7, input_binding::InputActionId::SelectShip8,
};

inline constexpr std::array kSelectCurrent{
    input_binding::InputActionId::SelectCurrent,
};

inline constexpr std::array kChatOpen{
    input_binding::InputActionId::ShowChat,
    input_binding::InputActionId::ShowChatSide1,
    input_binding::InputActionId::ShowChatSide2,
};

inline constexpr std::array kChatChannel{
    input_binding::InputActionId::SelectChatGlobal,
    input_binding::InputActionId::SelectChatAlliance,
    input_binding::InputActionId::SelectChatPrivate,
};

inline constexpr std::array kOfficerCanvas{
    input_binding::InputActionId::MoveLeft,
    input_binding::InputActionId::MoveRight,
};

inline constexpr std::array kSearchFocus{
    input_binding::InputActionId::FocusSearch,
};

inline constexpr std::array kSimpleFleet{
    input_binding::InputActionId::FleetQueueClear,
    input_binding::InputActionId::FleetViewInfo,
};

inline constexpr std::array kRuntimeSpace{
    input_binding::InputActionId::FleetPrimary,      input_binding::InputActionId::FleetSecondary,
    input_binding::InputActionId::FleetService,      input_binding::InputActionId::FleetQueueAdd,
    input_binding::InputActionId::FleetRecallCancel, input_binding::InputActionId::FleetRecall,
    input_binding::InputActionId::FleetRepair,
};

inline constexpr std::array kTableDispatch{
    input_binding::InputActionId::ShowQTrials,
    input_binding::InputActionId::ShowBookmarks,
    input_binding::InputActionId::ShowLookup,
    input_binding::InputActionId::ShowRefinery,
    input_binding::InputActionId::ShowFactions,
    input_binding::InputActionId::ShowStationExterior,
    input_binding::InputActionId::ShowGalaxy,
    input_binding::InputActionId::ShowStationInterior,
    input_binding::InputActionId::ShowSystem,
    input_binding::InputActionId::ShowArtifacts,
    input_binding::InputActionId::ShowInventory,
    input_binding::InputActionId::ShowMissions,
    input_binding::InputActionId::ShowResearch,
    input_binding::InputActionId::ShowScrapYard,
    input_binding::InputActionId::ShowOfficers,
    input_binding::InputActionId::ShowCommander,
    input_binding::InputActionId::ShowAwayTeam,
    input_binding::InputActionId::ShowEvents,
    input_binding::InputActionId::ShowExoComp,
    input_binding::InputActionId::ShowDaily,
    input_binding::InputActionId::ShowGifts,
    input_binding::InputActionId::ShowAlliance,
    input_binding::InputActionId::ShowAllianceHelp,
    input_binding::InputActionId::ShowAllianceArmada,
    input_binding::InputActionId::ShowSettings,
    input_binding::InputActionId::UiScaleUp,
    input_binding::InputActionId::UiScaleDown,
    input_binding::InputActionId::UiViewerScaleUp,
    input_binding::InputActionId::UiViewerScaleDown,
    input_binding::InputActionId::TogglePreviewLocate,
    input_binding::InputActionId::TogglePreviewRecall,
    input_binding::InputActionId::ToggleCargoDefault,
    input_binding::InputActionId::ToggleCargoPlayer,
    input_binding::InputActionId::ToggleCargoStation,
    input_binding::InputActionId::ToggleCargoHostile,
    input_binding::InputActionId::ToggleCargoArmada,
    input_binding::InputActionId::LogOff,
    input_binding::InputActionId::LogError,
    input_binding::InputActionId::LogWarn,
    input_binding::InputActionId::LogInfo,
    input_binding::InputActionId::LogDebug,
    input_binding::InputActionId::LogTrace,
    input_binding::InputActionId::ShowShips,
};

/**
 * @brief Union of all per-frame InputAction groups above.
 *
 * The dispatcher uses this list to compute the watched-key set so that only
 * keys actually mapped to a frame-router-handled action enter the per-frame
 * snapshot.
 */
inline constexpr auto kFrameActions = [] {
  std::array<input_binding::InputActionId, kStartup.size() + kQuit.size() + kQueue.size() + kShipSelection.size()
                                               + kSelectCurrent.size() + kSimpleFleet.size() + kRuntimeSpace.size()
                                               + kChatOpen.size() + kChatChannel.size() + kOfficerCanvas.size()
                                               + kSearchFocus.size() + kTableDispatch.size()>
       actions{};
  auto output = actions.begin();

  for (const auto action : kStartup) {
    *output++ = action;
  }
  for (const auto action : kQuit) {
    *output++ = action;
  }
  for (const auto action : kQueue) {
    *output++ = action;
  }
  for (const auto action : kShipSelection) {
    *output++ = action;
  }
  for (const auto action : kSelectCurrent) {
    *output++ = action;
  }
  for (const auto action : kSimpleFleet) {
    *output++ = action;
  }
  for (const auto action : kRuntimeSpace) {
    *output++ = action;
  }
  for (const auto action : kChatOpen) {
    *output++ = action;
  }
  for (const auto action : kChatChannel) {
    *output++ = action;
  }
  for (const auto action : kOfficerCanvas) {
    *output++ = action;
  }
  for (const auto action : kSearchFocus) {
    *output++ = action;
  }
  for (const auto action : kTableDispatch) {
    *output++ = action;
  }

  return actions;
}();
} // namespace hotkey_router_actions
