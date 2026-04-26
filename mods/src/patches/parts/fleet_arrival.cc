/**
 * @file fleet_arrival.cc
 * @brief Fleet notification hooks.
 *
 * This layer only captures game signals and forwards them to fleet_notifications.
 * Incoming attacks are sourced from ToastFleetObserver.QueueNotifications so the
 * feature has a single targeted path instead of several inference fallbacks.
 */
#include "config.h"
#include "errormsg.h"

#include <patches/fleet_notifications.h>
#include <patches/live_debug.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetPlayerData.h>
#include <prime/IList.h>
#include <prime/MiningObjectViewerWidget.h>
#include <prime/Notification.h>

#include <str_utils.h>

#include <spud/detour.h>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

namespace {
constexpr int kNotificationProducerTypeIncomingFleet = 7;
constexpr ptrdiff_t kIncomingFleetParamsTargetTypeOffset = 0x18;
constexpr ptrdiff_t kIncomingFleetParamsQuickScanResultOffset = 0x20;
constexpr ptrdiff_t kIncomingFleetParamsParamsObjectOffset = 0x28;
constexpr ptrdiff_t kIncomingFleetParamsParamsCaseOffset = 0x30;
constexpr ptrdiff_t kQuickScanFleetDataFleetTypeOffset = 0x18;
constexpr ptrdiff_t kQuickScanFleetDataTargetIdOffset = 0x20;
constexpr ptrdiff_t kQuickScanFleetDataTargetFleetIdOffset = 0x28;

struct QuickScanData {
  int fleet_type = 0;
  std::string target_id;
  uint64_t target_fleet_id = 0;
};

std::string read_string_field(Il2CppObject* object, ptrdiff_t offset)
{
  auto* value = object ? *reinterpret_cast<Il2CppString**>(reinterpret_cast<char*>(object) + offset) : nullptr;
  return value ? to_string(value) : std::string{};
}

QuickScanData read_quick_scan_data(Il2CppObject* quickScanResult)
{
  QuickScanData data;
  if (!quickScanResult) {
    return data;
  }

  data.fleet_type = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(quickScanResult) +
                                                kQuickScanFleetDataFleetTypeOffset);
  data.target_id = read_string_field(quickScanResult, kQuickScanFleetDataTargetIdOffset);
  data.target_fleet_id = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(reinterpret_cast<char*>(quickScanResult) +
                                                                           kQuickScanFleetDataTargetFleetIdOffset));
  return data;
}

FleetPlayerData* fleet_bar_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  auto helper      = IL2CppClassHelper{((Il2CppObject*)self)->klass};
  auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

void FleetStateWidget_SetWidgetData_Hook(auto original, void* self)
{
  auto* fleet = fleet_bar_widget_context(self);
  fleet_notifications_observe_fleet_bar(fleet);

  original(self);
}

void ToastFleetObserver_HandleMiningDepleted_Hook(auto original, void* self, int64_t fleetId)
{
  original(self, fleetId);
  fleet_notifications_observe_node_depleted(fleetId);
}

void ToastFleetObserver_QueueNotifications_Hook(auto original, void* self, IList* notifications)
{
  auto notification_count = notifications ? notifications->Count : 0;

  for (int index = 0; notifications && index < notification_count; ++index) {
    auto* notification = reinterpret_cast<Notification*>(notifications->Get(index));
    if (!notification || notification->ProducerType != kNotificationProducerTypeIncomingFleet) {
      continue;
    }

    auto* incoming_params = notification->IncomingFleetParams;
    auto target_type = incoming_params ? *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(incoming_params) +
                                                                     kIncomingFleetParamsTargetTypeOffset)
                                       : -1;
    auto* quick_scan_result = incoming_params
                                  ? *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(incoming_params) +
                                                                      kIncomingFleetParamsQuickScanResultOffset)
                                  : nullptr;
    auto quick_scan = read_quick_scan_data(quick_scan_result);
    auto params_case = incoming_params ? *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(incoming_params) +
                                                                     kIncomingFleetParamsParamsCaseOffset)
                                       : 0;
    auto* params_object = incoming_params
                              ? *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(incoming_params) +
                                                                  kIncomingFleetParamsParamsObjectOffset)
                              : nullptr;
    uint64_t target_fleet_id = 0;
    if (params_case == 4 && params_object) {
      target_fleet_id = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(reinterpret_cast<char*>(params_object) + 0x10));
    }

    spdlog::debug("[IncomingAttack] queue index={} count={} targetType={} paramsCase={} targetFleetId={} quickScanFleetType={} quickScanTargetFleetId={} quickScanTargetId='{}'",
                  index,
                  notification_count,
                  target_type,
                  params_case,
                  target_fleet_id,
                  quick_scan.fleet_type,
                  quick_scan.target_fleet_id,
                  quick_scan.target_id);
    live_debug_record_incoming_fleet_materialized("ToastFleetObserver.QueueNotifications",
                                                  target_type,
                                                  target_fleet_id,
                                                  quick_scan.fleet_type,
                                                  quick_scan.target_fleet_id,
                                                  quick_scan.target_id);
    ToastFleetQueueNotificationsSignal signal;
    signal.source = "toast-fleet-queue";
    signal.target_fleet_id = target_fleet_id;
    signal.target_type = target_type;
    signal.attacker_fleet_type = quick_scan.fleet_type;
    signal.attacker_identity = quick_scan.target_id;
    fleet_notifications_notify_incoming_attack_target(signal);
  }

  original(self, notifications);
}

void MiningObjectViewerWidget_UpdateTimerWidget_Hook(auto original, MiningObjectViewerWidget* self,
                                                     FleetPlayerData* selectedFleet)
{
  original(self, selectedFleet);

  auto* timerContext  = self ? self->_miningTimerWidgetContext : nullptr;
  auto remainingTicks = timerContext ? timerContext->RemainingTime.Ticks : -1;
  fleet_notifications_observe_mining_timer(selectedFleet, remainingTicks);
}
} // namespace

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

  SPUD_STATIC_DETOUR(set_widget_data, FleetStateWidget_SetWidgetData_Hook);

  auto toast_fleet_observer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver");
  if (!toast_fleet_observer.isValidHelper()) {
    spdlog::warn("[Fleet] ToastFleetObserver helper not found; fleet observer hooks disabled");
  } else {
    auto handle_mining_depleted = toast_fleet_observer.GetMethod("HandleMiningDepleted");
    if (handle_mining_depleted == nullptr) {
      spdlog::warn("[Fleet] ToastFleetObserver.HandleMiningDepleted not found; node depleted notifications disabled");
    } else {
      SPUD_STATIC_DETOUR(handle_mining_depleted, ToastFleetObserver_HandleMiningDepleted_Hook);
    }

    auto queue_notifications = toast_fleet_observer.GetMethod("QueueNotifications", 1);
    if (queue_notifications == nullptr) {
      spdlog::warn("[IncomingAttack] ToastFleetObserver.QueueNotifications not found; incoming attack notifications disabled");
    } else {
      SPUD_STATIC_DETOUR(queue_notifications, ToastFleetObserver_QueueNotifications_Hook);
    }
  }

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

  SPUD_STATIC_DETOUR(update_timer_widget, MiningObjectViewerWidget_UpdateTimerWidget_Hook);
}
