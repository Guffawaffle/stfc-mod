/**
 * @file fleet_arrival.cc
 * @brief Fleet arrival detection driven by the bottom fleet bar state widgets.
 *
 * Hooks FleetStateWidget::SetWidgetData and watches FleetPlayerData state
 * transitions. The most useful arrival signal is Warping -> Impulsing, which
 * means the ship has dropped out of warp and entered the destination system.
 */
#include "config.h"
#include "errormsg.h"

#include <patches/fleet_notifications.h>
#include <hook/hook.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetPlayerData.h>
#include <prime/MiningObjectViewerWidget.h>

#include <spdlog/spdlog.h>

static FleetPlayerData* fleet_bar_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  auto helper      = IL2CppClassHelper{((Il2CppObject*)self)->klass};
  auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

typedef void (*FleetStateWidget_SetWidgetData_fn)(void*);
static FleetStateWidget_SetWidgetData_fn FleetStateWidget_SetWidgetData_original = nullptr;

typedef void (*ToastFleetObserver_HandleMiningDepleted_fn)(void*, int64_t);
static ToastFleetObserver_HandleMiningDepleted_fn ToastFleetObserver_HandleMiningDepleted_original = nullptr;

typedef void (*MiningObjectViewerWidget_UpdateTimerWidget_fn)(MiningObjectViewerWidget*, FleetPlayerData*);
static MiningObjectViewerWidget_UpdateTimerWidget_fn MiningObjectViewerWidget_UpdateTimerWidget_original = nullptr;

static void FleetStateWidget_SetWidgetData_Hook(void* self)
{
  auto* fleet = fleet_bar_widget_context(self);
  fleet_notifications_observe_fleet_bar(fleet);

  FleetStateWidget_SetWidgetData_original(self);
}

static void ToastFleetObserver_HandleMiningDepleted_Hook(void* self, int64_t fleetId)
{
  ToastFleetObserver_HandleMiningDepleted_original(self, fleetId);
  fleet_notifications_observe_node_depleted(fleetId);
}

static void MiningObjectViewerWidget_UpdateTimerWidget_Hook(MiningObjectViewerWidget* self, FleetPlayerData* selectedFleet)
{
  MiningObjectViewerWidget_UpdateTimerWidget_original(self, selectedFleet);

  auto* timerContext  = self ? self->_miningTimerWidgetContext : nullptr;
  auto remainingTicks = timerContext ? timerContext->RemainingTime.Ticks : -1;
  fleet_notifications_observe_mining_timer(selectedFleet, remainingTicks);
}

void InstallFleetArrivalHooks()
{
  fleet_notifications_init();

  auto fleet_state_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
  if (!fleet_state_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.HUD", "FleetStateWidget");
    return;
  }

  auto set_widget_data = fleet_state_widget.GetMethod("SetWidgetData");
  if (set_widget_data == nullptr) {
    ErrorMsg::MissingMethod("FleetStateWidget", "SetWidgetData");
    return;
  }

  mh_install(set_widget_data, (void*)FleetStateWidget_SetWidgetData_Hook, (void**)&FleetStateWidget_SetWidgetData_original);

  auto toast_fleet_observer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver");
  if (!toast_fleet_observer.isValidHelper()) {
    spdlog::warn("[Fleet] ToastFleetObserver helper not found; node depleted notifications disabled");
    return;
  }

  auto handle_mining_depleted = toast_fleet_observer.GetMethod("HandleMiningDepleted");
  if (handle_mining_depleted == nullptr) {
    spdlog::warn("[Fleet] ToastFleetObserver.HandleMiningDepleted not found; node depleted notifications disabled");
    return;
  }

  mh_install(handle_mining_depleted, (void*)ToastFleetObserver_HandleMiningDepleted_Hook,
             (void**)&ToastFleetObserver_HandleMiningDepleted_original);

  auto mining_object_viewer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "MiningObjectViewerWidget");
  if (!mining_object_viewer.isValidHelper()) {
    spdlog::warn("[MiningViewer] MiningObjectViewerWidget helper not found; mining ETA disabled");
    return;
  }

  auto update_timer_widget = mining_object_viewer.GetMethod<void(MiningObjectViewerWidget*, FleetPlayerData*)>(
      "UpdateTimerWidget", 1);
  if (update_timer_widget == nullptr) {
    spdlog::warn("[MiningViewer] MiningObjectViewerWidget.UpdateTimerWidget not found; mining ETA disabled");
    return;
  }

  mh_install(update_timer_widget, (void*)MiningObjectViewerWidget_UpdateTimerWidget_Hook,
             (void**)&MiningObjectViewerWidget_UpdateTimerWidget_original);
}
