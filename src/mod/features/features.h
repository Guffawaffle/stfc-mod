#pragma once

// The mod's features: the descriptors mod.cc installs, and the few calls one feature makes into another, mostly
// from the shortcut dispatch in hotkeys.cc. Grouped by the file that defines them.

#include "config.h"
#include "installation.h"

#include <chrono>
#include <cstdint>
#include <span>

struct NavigationZoom;
enum class NodeDepth : int32_t;

// What mod.cc installs. Only the [patches] groups decide what is installed, so a clash with another mod or a game
// update can be switched off; feature settings are read inside the hooks, so features switch on and off at runtime.
struct Feature {
  // Names the feature in the install log, and tags its own log lines: "[Name] ...".
  const char* name;
  // The [patches] group that installs it; null for the few that always install.
  bool Config::Patches::* group;
  // Installs its hooks; false when they did not go in, after the library or the feature has said why. Null for frame
  // work that needs no hooks.
  bool (*install)();
  // Runs every frame from the frame tick once installed, in every hotkey mode, before the shortcuts. Never runs for a
  // feature that failed to install. Returns at once when its feature is off.
  void (*frame)() = nullptr;
  // Required bootstrap phases fail closed. Existing gameplay features are optional; Profiles' three-phase
  // bootstrap remains outside this table and must use the same required-failure policy when integrated.
  InstallRequirement requirement = InstallRequirement::kOptional;
};

struct FeatureInstallation {
  const char*  name             = nullptr;
  InstallState state            = InstallState::kNotAttempted;
  bool         frame_registered = false;
  bool         frame_ready      = false;
};

// Bootstrap/game thread only. Installation and frame delivery are separate observations, including when a
// feature's hooks installed but the one shared FrameTick did not. No initialization is triggered by this query.
std::span<const FeatureInstallation> FeatureInstallations();

// aai.cc: the private native product owns its two callbacks; host-owned
// transactions, per-target reporting and the shared frame owner stay central.
#if defined(STFC_AAI_ENABLED)
extern const Feature kAaiReportObserver;
extern const Feature kAaiBack;
extern const Feature kBattleLogSummary;
#endif

// runtime_config.cc: registered-key saves and native quit/drain lifecycle.
extern const Feature kRuntimeConfig;
extern const Feature kModState;
extern const Feature kFleetNotifications;
extern const Feature kFleetNodeDepletion;

// Frame work that only needs to run every so often, such as a scan of the UI. Game thread only.
class Throttle
{
public:
  explicit Throttle(std::chrono::steady_clock::duration interval)
      : interval_(interval)
  {
  }
  // True on the first call, then at most once per interval.
  bool Due()
  {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_)
      return false;
    next_ = now + interval_;
    return true;
  }

private:
  std::chrono::steady_clock::duration   interval_;
  std::chrono::steady_clock::time_point next_{};
};

// action_queue.cc. Shared installers check native metadata and macOS entry fit before staging.
extern const Feature kActionQueueRecovery;
extern const Feature kThinQueueProtection;
extern const Feature kQueueAddressGuard;
// Forgets the pending recovery attempts when the queue is switched off or on. Never touches the game's queues.
void ResetQueueRecoveryTracking();

// anomaly_timer.cc
extern const Feature kAnomalyTimer;

// audio.cc
extern const Feature kAudio;

// base_card.cc
extern const Feature kBaseCard;

// buffs.cc
extern const Feature kOutOfDockPower;
extern const Feature kBuffCrashGuard;

// camera.cc
extern const Feature kZoom;
extern const Feature kScrollZoomGuard;
extern const Feature kPan;
extern const Feature kInteriorZoom;

// cargo.cc. The instant counter is Windows only.
extern const Feature kCargoFormat;
#if _WIN32
extern const Feature kInstantCargo;
#endif

// chat.cc
extern const Feature kChat;
extern const Feature kChatAvatarRetention;
extern const Feature kChatTitles;
// Opens chat, or focuses side chat's input. Never swallows the key.
void OpenChatShortcut();
// In chat, the channel shortcuts. True when the key was used.
bool ChatChannelShortcuts();

// confirmations.cc
extern const Feature kForbiddenTechConfirm;
extern const Feature kRevealConfirm;

// dock.cc
extern const Feature kPinnedShips;
extern const Feature kPinnedShipClick;
extern const Feature kShipTechIndicators;
bool                 EnsureShipTileBinding();
extern const Feature kDoubleClickAssign;
extern const Feature kAssignOnEnter;
extern const Feature kDockSelection;

// fleet_actions.cc
extern const Feature kQueueSwitch;
extern const Feature kAbilityButtons;
// The pre-scan cargo hooks; installed with the hotkeys.
void InstallFleetActions();
// A target or object viewer card is open in space.
bool TargetViewerShown();
// The number keys. True when the key was used.
bool SelectShipShortcuts();
// Select current and the queue switch. True when the key was used.
bool FleetShortcuts();
// The actions on the selected fleet and the open viewer. True when the key must not reach the game.
bool FleetActionShortcuts();

// fleet_labels.cc
bool                 EnsureNavigationFleetLifecycle();
bool                 EnsureNavigationLodUpdates();
bool                 EnsureNavigationZoomUpdates();
extern const Feature kFleetLabels;
extern const Feature kGalaxyLabels;
extern const Feature kGalaxySelection;
extern const Feature kShipShortcutBadges;
// Fed by camera.cc's zoom hooks: the camera's depth on each update and system entry, then its zoom after the
// game's own update.
void FleetLabelsFollowDepth(NavigationZoom* zoom, NodeDepth depth);
void FleetLabelsAfterZoom(NavigationZoom* zoom);
// Queue label reapplication for the next live system camera update.
void RefreshFleetLabelControls();

// opc_indicators.cc
extern const Feature kOpcIndicators;

// mining_preview.cc
extern const Feature kMiningPreview;

// navigation_orbit.cc
extern const Feature kNavigationOrbit;

// forbidden_tech_ship_name.cc
extern const Feature kForbiddenTechShipName;

// game_config.cc
extern const Feature kGameConfigOverrides;

// haven.cc
extern const Feature kHavenZoom;
extern const Feature kHavenHistory;

// haven_orbit.cc shares camera.cc's single native constraint owner.
extern const Feature kHavenOrbit;
bool                 EnsureOrbitConstraints();

// hotkeys.cc
extern const Feature kHotkeys;
// The shortcut dispatch for one frame, run by the frame tick. True when the game's own ScreenManager.Update must
// not run, so it does not handle the key as well. False until kHotkeys is installed.
bool HandleShortcuts();

// instant_warp.cc
extern const Feature kInstantWarp;
// Steps auto_confirm_instant_warp through none, warp and jump, until the next settings reload.
void CycleInstantWarpConfirmation();

// keybind_badges.cc, installed with the hotkeys.
void InstallKeybindBadges();
// Toggles the game's keybind badges. False when the badges are not the mod's, so the key is not used.
bool ToggleKeybindBadges();
void HideKeybindBadges();

// loading_screens.cc
extern const Feature kLoginScreen;
extern const Feature kTransitionScreen;
extern const Feature kLoadingTips;

// missions.cc
extern const Feature kMissionHud;
extern const Feature kDailyClaimFilter;
namespace mission_hud
{
bool Available() noexcept;
// Native refresh/application rejects reentrant preference changes.
bool CanChange() noexcept;
// Re-run the native setup paths on weakly tracked, currently enabled HUDs.
void Refresh() noexcept;
} // namespace mission_hud

// officers.cc
extern const Feature kOfficerSort;
// officer_presets.cc: local ordering; native save and state-file receipts remain separate.
extern const Feature kOfficerPresetReorder;

// settings.cc: native FC/FT confirmation core; full Mod Settings pages are restored separately.
extern const Feature kNativeSettings;
extern const Feature kNativeSettingsValues;
extern const Feature kNativeSettingsActions;
extern const Feature kNativeSettingsTextEntry;
extern const Feature kNativeSettingsPages;
extern const Feature kNativeSettingsSearch;

// screens.cc: shortcut helpers only, so no descriptor.
// Presses a button a screen shortcut opened a screen for, once the screen is up. True when one was pressed.
bool PressAwaitedButton();
// Input capture retires deferred screen requests without pressing a native button.
void CancelAwaitedButton();
// The shortcuts that open, step through, or search game screens. True when the key must not reach the game.
bool ScreenShortcuts();

// shop.cc
extern const Feature kGiftsAutoOpen;
extern const Feature kShopLimits;
#if _WIN32
extern const Feature kDonationLimit;
#endif
extern const Feature kSkipRevealSequence;
extern const Feature kArtifactExchange;

// toasts.cc
extern const Feature kToasts;

// tracked_objects.cc
extern const Feature kObjectTracker;

// ui_scale.cc
extern const Feature kUiScale;
// Steps the interface, ship or viewer scale by ui_scale_step (the viewer by a quarter of it), until the next
// settings reload.
void AdjustUiScale(bool up);
void AdjustUiShipScale(bool up);
void AdjustUiViewerScale(bool up);
// Exact Settings edits use the same loaded-model refresh as the ship-scale shortcut. Persistence remains with caller.
bool SetUiShipScale(float value);

// window.cc. Resizing and the cursor are Windows only.
#if _WIN32
extern const Feature kFreeResize;
extern const Feature kCursor;
#endif
// The monitor's physical-to-logical pixel ratio on Windows; 1 elsewhere. Cached until `refresh`.
float WindowDpiScale(bool refresh = false);

// sync/sync.cc
extern const Feature kSync;
