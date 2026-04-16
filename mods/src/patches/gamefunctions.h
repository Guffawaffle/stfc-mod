/**
 * @file gamefunctions.h
 * @brief Enum of all bindable game functions for the keyboard mapping system.
 *
 * Each value corresponds to a configurable action that can be bound to one or
 * more key combinations in the TOML settings file. MapKey maintains an array
 * of bindings indexed by these values.
 */
#pragma once

enum GameFunction {
  // ─── Navigation ──────────────────────────────────────────────────────────
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,

  // ─── Chat Channel Selection ──────────────────────────────────────────────
  SelectChatAlliance,
  SelectChatGlobal,
  SelectChatPrivate,

  // ─── Ship Selection ──────────────────────────────────────────────────────
  SelectShip1,
  SelectShip2,
  SelectShip3,
  SelectShip4,
  SelectShip5,
  SelectShip6,
  SelectShip7,
  SelectShip8,
  SelectCurrent,              ///< Re-select the currently active ship.

  // ─── UI Panel Toggles ───────────────────────────────────────────────────
  ShowAlliance,
  ShowAllianceArmada,
  ShowAllianceHelp,
  ShowArtifacts,
  ShowOfficers,
  ShowCommander,
  ShowRefinery,
  ShowQTrials,                ///< Q's Trials event panel.
  ShowBookmarks,
  ShowLookup,                 ///< System / player search panel.
  ShowExoComp,
  ShowFactions,
  ShowGifts,
  ShowDaily,                  ///< Daily goals / login rewards.
  ShowAwayTeam,
  ShowMissions,
  ShowResearch,
  ShowScrapYard,
  ShowShips,
  ShowInventory,
  ShowStationInterior,
  ShoWStationExterior,        ///< Note: typo preserved from original enum.
  ShowGalaxy,                 ///< Switch to galaxy map view.
  ShowSystem,                 ///< Switch to system view.

  // ─── Chat & Side Panels ─────────────────────────────────────────────────
  ShowChat,
  ShowChatSide1,              ///< First side-panel chat slot.
  ShowChatSide2,              ///< Second side-panel chat slot.
  ShowEvents,
  ShowSettings,

  // ─── Zoom ────────────────────────────────────────────────────────────────
  ZoomPreset1,
  ZoomPreset2,
  ZoomPreset3,
  ZoomPreset4,
  ZoomPreset5,
  ZoomIn,
  ZoomOut,
  ZoomMin,                    ///< Zoom all the way out.
  ZoomMax,                    ///< Zoom all the way in.
  ZoomReset,

  // ─── UI Scaling ──────────────────────────────────────────────────────────
  UiScaleUp,
  UiScaleDown,
  UiViewerScaleUp,            ///< Object viewer panel scale.
  UiViewerScaleDown,

  // ─── Ship Actions ────────────────────────────────────────────────────────
  ActionPrimary,              ///< Context-dependent primary action (e.g. warp, mine).
  ActionSecondary,            ///< Context-dependent secondary action.
  ActionQueue,                ///< Queue the next action.
  ActionQueueClear,           ///< Clear the entire action queue.
  ActionView,                 ///< Open the object viewer for the selected target.
  ActionRecall,
  ActionRecallCancel,
  ActionRepair,

  // ─── Zoom Preset Assignment ──────────────────────────────────────────────
  SetZoomPreset1,
  SetZoomPreset2,
  SetZoomPreset3,
  SetZoomPreset4,
  SetZoomPreset5,
  SetZoomDefault,

  // ─── Hotkey System Control ───────────────────────────────────────────────
  DisableHotKeys,             ///< Temporarily disable all keyboard shortcuts.
  EnableHotKeys,              ///< Re-enable keyboard shortcuts.

  // ─── Toggles ─────────────────────────────────────────────────────────────
  ToggleQueue,                ///< Toggle action queue display.
  TogglePreviewLocate,        ///< Toggle locate preview on map.
  TogglePreviewRecall,        ///< Toggle recall preview on map.

  // ─── Cargo Filter Toggles ───────────────────────────────────────────────
  ToggleCargoDefault,
  ToggleCargoPlayer,
  ToggleCargoStation,
  ToggleCargoHostile,
  ToggleCargoArmada,

  // ─── Log Level ───────────────────────────────────────────────────────────
  LogLevelDebug,
  LogLevelInfo,
  LogLevelTrace,
  LogLevelError,
  LogLevelWarn,
  LogLevelOff,

  // ─── Application ─────────────────────────────────────────────────────────
  Quit,

  // Automatic max value
  Max
};
