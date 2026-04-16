/**
 * @file viewer_mgmt.cc
 * @brief Object viewer visibility management and rewards panel toggling.
 *
 * Manages the lifecycle of object viewer overlays (alliance starbase, armada,
 * celestial, embassy, housing, mining, missions, pre-scan target). Provides
 * Escape-to-dismiss, rewards screen dismissal, and the multi-frame
 * cargo/rewards info panel toggle (ActionView key).
 */
#include "errormsg.h"
#include "config.h"

#include "patches/viewer_mgmt.h"

// Object Viewers
#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include "prime/AnimatedRewardsScreenViewController.h"

/** Frame counter for multi-frame info panel display. */
static int show_info_pending = 0;

// ─── Viewer Visibility Queries & Hide ───────────────────────────────────────────────

// NOTE: If you change this loop functionality, also change DoHideViewersOfType template
template <typename T>
inline bool CanHideViewersOfType()
{
  for (auto widget : ObjectFinder<T>::GetAll()) {
    const auto visible = widget && widget->_visibilityController != NULL
                         && (widget->_visibilityController->_state == VisibilityState::Visible
                             || widget->_visibilityController->_state == VisibilityState::Show);
    if (visible) {
      return true;
    }
  }

  return false;
}

bool CanHideViewers()
{
  return (CanHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || CanHideViewersOfType<ArmadaObjectViewerWidget>()
          || CanHideViewersOfType<CelestialObjectViewerWidget>() || CanHideViewersOfType<EmbassyObjectViewer>()
          || CanHideViewersOfType<HousingObjectViewerWidget>() || CanHideViewersOfType<MiningObjectViewerWidget>()
          || CanHideViewersOfType<MissionsObjectViewerWidget>() || CanHideViewersOfType<PreScanTargetWidget>()
          || CanHideViewersOfType<HousingObjectViewerWidget>());
}

// NOTE: If you change this loop functionality, also change CanHideViewersOfType template
template <typename T>
inline bool DidHideViewersOfType()
{
  const auto objects = ObjectFinder<T>::GetAll();
  auto       didHide = false;
  for (auto widget : objects) {
    if (!widget) {
      continue;
    }
    auto visbility_controller = widget->_visibilityController;
    if (!visbility_controller) {
      continue;
    }
    const auto visible = (visbility_controller->_state == VisibilityState::Visible
                          || visbility_controller->_state == VisibilityState::Show);
    if (visible) {
      widget->HideAllViewers();
      didHide = true;
    }
  }

  return didHide;
}

bool DidHideViewers()
{
  return DidHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || DidHideViewersOfType<ArmadaObjectViewerWidget>()
         || DidHideViewersOfType<CelestialObjectViewerWidget>() || DidHideViewersOfType<EmbassyObjectViewer>()
         || DidHideViewersOfType<HousingObjectViewerWidget>() || DidHideViewersOfType<MiningObjectViewerWidget>()
         || DidHideViewersOfType<MissionsObjectViewerWidget>() || DidHideViewersOfType<PreScanTargetWidget>()
         || DidHideViewersOfType<HousingObjectViewerWidget>();
}

// ─── Rewards Screen Dismiss ──────────────────────────────────────────────────────────

bool TryDismissRewardsScreen()
{
  if (auto reward_controller = ObjectFinder<AnimatedRewardsScreenViewController>::Get(); reward_controller) {
    if (reward_controller->IsActive()) {
      reward_controller->GoBackToLastSection();
      return true;
    }
  }
  return false;
}

// ─── ActionView / Info Pending ────────────────────────────────────────────────────────

void HandleActionView()
{
  auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

  for (auto& pre_scan_widget : all_pre_scan_widgets) {
    if (pre_scan_widget
        && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
            || pre_scan_widget->_visibilityController->_state == VisibilityState::Show)) {
      auto rewardsWidget = pre_scan_widget->_rewardsButtonWidget;
      if (rewardsWidget->_rewardsController->_state != VisibilityState::Visible
          && rewardsWidget->_rewardsController->_state != VisibilityState::Show) {
        show_info_pending = 5;
      } else {
        rewardsWidget->_rewardsController->Hide();
      }
    }
  }
}

void TickInfoPending()
{
  if (show_info_pending <= 0) {
    return;
  }

  auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

  for (auto& pre_scan_widget : all_pre_scan_widgets) {
    const auto pre_scan_visible = pre_scan_widget
                                  && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
                                      || pre_scan_widget->_visibilityController->_state == VisibilityState::Show);
    if (pre_scan_visible) {
      auto       rewardsWidget          = pre_scan_widget->_rewardsButtonWidget;
      const auto rewards_widget_visible = rewardsWidget->_rewardsController->_state == VisibilityState::Visible
                                          || rewardsWidget->_rewardsController->_state == VisibilityState::Show;
      if (!rewards_widget_visible) {
        rewardsWidget->_rewardsController->Show(true);
      }
    }
  }
  show_info_pending -= 1;
}

void SetInfoPending(int frames)
{
  show_info_pending = frames;
}
