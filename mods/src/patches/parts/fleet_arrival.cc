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
#include "file.h"

#include <patches/fleet_notifications.h>
#include <patches/live_debug.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetPlayerData.h>
#include <prime/IncomingFleetParamsJson.h>
#include <prime/MiningObjectViewerWidget.h>
#include <prime/NavigationInteractionUIViewController.h>
#include <prime/StationWarningViewController.h>

#include <str_utils.h>

#include <spud/detour.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>

namespace {
constexpr bool kEnableNavigationHookOnEnable = false;
constexpr bool kEnableNavigationHookOnDisable = false;
constexpr bool kEnableNavigationHookAboutToShow = false;
constexpr bool kEnableNavigationHookDidHide = false;
constexpr bool kEnableStationWarningHooks = false;
constexpr bool kEnableIncomingFleetNotificationHook = false;
constexpr bool kEnableToastFleetObserverIncomingProducerHook = true;
constexpr int kNotificationProducerTypeIncomingFleet = 7;

void append_fleet_arrival_navhook_trace(const char* step,
                                        const char* phase,
                                        const void* controller,
                                        const void* sender = nullptr,
                                        const void* callback_context = nullptr)
{
  const auto trace_path = std::string(File::MakePath("community_patch_navhook_trace.log"));
  auto* trace_file = std::fopen(trace_path.c_str(), "ab");
  if (!trace_file) {
    return;
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto timestamp_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  std::fprintf(trace_file,
               "[%lld] fleet-arrival step=%s phase='%s' controller=%p sender=%p callbackContext=%p\n",
               static_cast<long long>(timestamp_ms),
               step ? step : "",
               phase ? phase : "",
               controller,
               sender,
               callback_context);
  std::fflush(trace_file);
  std::fclose(trace_file);
}
}

static FleetPlayerData* fleet_bar_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  auto helper      = IL2CppClassHelper{((Il2CppObject*)self)->klass};
  auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

static void FleetStateWidget_SetWidgetData_Hook(auto original, void* self)
{
  auto* fleet = fleet_bar_widget_context(self);
  fleet_notifications_observe_fleet_bar(fleet);

  original(self);
}

static void ToastFleetObserver_HandleMiningDepleted_Hook(auto original, void* self, int64_t fleetId)
{
  original(self, fleetId);
  fleet_notifications_observe_node_depleted(fleetId);
}

static void ToastFleetObserver_NotificationsChangedEventHandler_Hook(auto original, void* self, int producerType)
{
  auto producer_pointer = reinterpret_cast<void*>(static_cast<uintptr_t>(producerType));
  append_fleet_arrival_navhook_trace("toast-fleet-producer/before-original",
                                     "NotificationsChangedEventHandler",
                                     self,
                                     producer_pointer);
  if (producerType == kNotificationProducerTypeIncomingFleet) {
    live_debug_record_toast_fleet_producer("NotificationsChangedEventHandler", self, producerType);
    append_fleet_arrival_navhook_trace("toast-fleet-producer/before-notification",
                                       "NotificationsChangedEventHandler",
                                       self,
                                       producer_pointer);
    fleet_notifications_notify_incoming_attack_detected("toast-fleet-producer");
    append_fleet_arrival_navhook_trace("toast-fleet-producer/after-notification",
                                       "NotificationsChangedEventHandler",
                                       self,
                                       producer_pointer);
  }

  original(self, producerType);
  append_fleet_arrival_navhook_trace("toast-fleet-producer/after-original",
                                     "NotificationsChangedEventHandler",
                                     self,
                                     producer_pointer);
}

static void MiningObjectViewerWidget_UpdateTimerWidget_Hook(auto original, MiningObjectViewerWidget* self,
                                                            FleetPlayerData* selectedFleet)
{
  original(self, selectedFleet);

  auto* timerContext  = self ? self->_miningTimerWidgetContext : nullptr;
  auto remainingTicks = timerContext ? timerContext->RemainingTime.Ticks : -1;
  fleet_notifications_observe_mining_timer(selectedFleet, remainingTicks);
}

static std::string il2cpp_string_or_empty(Il2CppString* value)
{
  return value ? to_string(value) : std::string{};
}

static void record_incoming_fleet_materialized(std::string_view phase, IncomingFleetParamsJson* value)
{
  if (!LiveDebugChannelEnabled() || !value) {
    return;
  }

  auto* quick_scan_result = value->QuickScanResult;
  auto  quick_scan_target_id =
      quick_scan_result ? il2cpp_string_or_empty(quick_scan_result->TargetId) : std::string{};
  auto target_type = static_cast<int>(value->TargetType);
  auto target_fleet_id = static_cast<uint64_t>(value->TargetFleetId);
  auto quick_scan_target_fleet_id = quick_scan_result ? static_cast<uint64_t>(quick_scan_result->TargetFleetId) : 0;

  spdlog::debug(
      "[IncomingFleet] {} targetType={} targetFleetId={} quickScanTargetFleetId={} quickScanTargetId='{}'",
      phase, target_type, target_fleet_id, quick_scan_target_fleet_id, quick_scan_target_id);
  live_debug_record_incoming_fleet_materialized(phase, target_type, target_fleet_id,
                                                quick_scan_target_fleet_id, quick_scan_target_id);
}

static void record_station_warning_phase(std::string_view phase, StationWarningViewController* self)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  auto* context = self ? self->CanvasContext : nullptr;
  auto* quick_scan_result = context ? context->QuickScanResult : nullptr;
  auto  target_user_id = context ? il2cpp_string_or_empty(context->TargetUserId) : std::string{};
  auto  quick_scan_target_id =
      quick_scan_result ? il2cpp_string_or_empty(quick_scan_result->TargetId) : std::string{};
  auto target_type = context ? static_cast<int>(context->TargetType) : -1;
  auto target_fleet_id = context ? static_cast<uint64_t>(context->TargetFleetId) : 0;
  auto quick_scan_target_fleet_id = quick_scan_result ? static_cast<uint64_t>(quick_scan_result->TargetFleetId) : 0;

  spdlog::debug(
      "[StationWarning] {} hasContext={} targetType={} targetFleetId={} targetUserId='{}' quickScanTargetFleetId={} quickScanTargetId='{}'",
      phase, context != nullptr, target_type, target_fleet_id, target_user_id, quick_scan_target_fleet_id,
      quick_scan_target_id);
  live_debug_record_station_warning(phase, context != nullptr, target_type, target_fleet_id, target_user_id,
                                    quick_scan_target_fleet_id, quick_scan_target_id);
}

static void StationWarningViewController_OnDidBindCanvasContext_Hook(auto original,
                                                                     StationWarningViewController* self)
{
  original(self);
  record_station_warning_phase("OnDidBindCanvasContext", self);
}

static void StationWarningViewController_OnEnable_Hook(auto original, StationWarningViewController* self)
{
  original(self);
  record_station_warning_phase("OnEnable", self);
}

static void StationWarningViewController_OnDisable_Hook(auto original, StationWarningViewController* self)
{
  record_station_warning_phase("OnDisable", self);
  original(self);
}

static void StationWarningViewController_OnAnimationCompleted_Hook(auto original,
                                                                   StationWarningViewController* self)
{
  original(self);
  record_station_warning_phase("OnAnimationCompleted", self);
}

static void NavigationInteractionUIViewController_OnEnable_Hook(auto original,
                                                                NavigationInteractionUIViewController* self)
{
  original(self);
  live_debug_note_navigation_hook("OnEnable", self);
}

static void NavigationInteractionUIViewController_OnDisable_Hook(auto original,
                                                                 NavigationInteractionUIViewController* self)
{
  live_debug_note_navigation_hook("OnDisable", self);
  original(self);
}

static void NavigationInteractionUIViewController_AboutToShowCanvasEventHandler_Hook(
    auto original, NavigationInteractionUIViewController* self, void* visibility_handler, void* obj)
{
  append_fleet_arrival_navhook_trace("hook/about-to-show/enter-local",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/before-shared-enter",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  live_debug_trace_navigation_hook_step("hook/about-to-show/enter",
                                        "AboutToShowCanvasEventHandler",
                                        self,
                                        visibility_handler,
                                        obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/after-shared-enter",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/before-original",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  original(self, visibility_handler, obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/after-original-local",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/before-shared-after-original",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  live_debug_trace_navigation_hook_step("hook/about-to-show/after-original",
                                        "AboutToShowCanvasEventHandler",
                                        self,
                                        visibility_handler,
                                        obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/after-shared-after-original",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/before-queue",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  live_debug_note_navigation_hook("AboutToShowCanvasEventHandler", self, visibility_handler, obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/after-queue-local",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/before-shared-after-queue",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
  live_debug_trace_navigation_hook_step("hook/about-to-show/after-queue",
                                        "AboutToShowCanvasEventHandler",
                                        self,
                                        visibility_handler,
                                        obj);
  append_fleet_arrival_navhook_trace("hook/about-to-show/after-shared-after-queue",
                                     "AboutToShowCanvasEventHandler",
                                     self,
                                     visibility_handler,
                                     obj);
}

static void NavigationInteractionUIViewController_DidHideCanvasEventHandler_Hook(
    auto original, NavigationInteractionUIViewController* self, void* visibility_handler, void* obj)
{
  original(self, visibility_handler, obj);
  live_debug_note_navigation_hook("DidHideCanvasEventHandler", self, visibility_handler, obj);
}

static void Notification_SetIncomingFleetParamsJson_Hook(auto original, void* self, IncomingFleetParamsJson* value)
{
  append_fleet_arrival_navhook_trace("incoming-fleet-json/before-original",
                                     "Notification.set_IncomingFleetParamsJson",
                                     self,
                                     value);
  live_debug_record_incoming_fleet_materialized_pointer("Notification.set_IncomingFleetParamsJson:before", self, value);
  original(self, value);
  append_fleet_arrival_navhook_trace("incoming-fleet-json/after-original",
                                     "Notification.set_IncomingFleetParamsJson",
                                     self,
                                     value);
  live_debug_record_incoming_fleet_materialized_pointer("Notification.set_IncomingFleetParamsJson:after", self, value);
  if (value) {
    append_fleet_arrival_navhook_trace("incoming-fleet-json/before-notification",
                                       "Notification.set_IncomingFleetParamsJson",
                                       self,
                                       value);
    fleet_notifications_notify_incoming_attack_detected("incoming-fleet-json");
    append_fleet_arrival_navhook_trace("incoming-fleet-json/after-notification",
                                       "Notification.set_IncomingFleetParamsJson",
                                       self,
                                       value);
  }
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

  SPUD_STATIC_DETOUR(set_widget_data, FleetStateWidget_SetWidgetData_Hook);

  if (kEnableStationWarningHooks) {
    auto station_warning_view_controller =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "StationWarningViewController");
    if (!station_warning_view_controller.isValidHelper()) {
      spdlog::warn("[StationWarning] StationWarningViewController helper not found; direct banner probe disabled");
    } else {
      auto on_enable = station_warning_view_controller.GetMethod("OnEnable");
      if (on_enable == nullptr) {
        spdlog::warn("[StationWarning] StationWarningViewController.OnEnable not found; lifecycle probe incomplete");
      } else {
        SPUD_STATIC_DETOUR(on_enable, StationWarningViewController_OnEnable_Hook);
      }

      auto on_disable = station_warning_view_controller.GetMethod("OnDisable");
      if (on_disable == nullptr) {
        spdlog::warn("[StationWarning] StationWarningViewController.OnDisable not found; lifecycle probe incomplete");
      } else {
        SPUD_STATIC_DETOUR(on_disable, StationWarningViewController_OnDisable_Hook);
      }

      auto on_did_bind_canvas_context = station_warning_view_controller.GetMethod("OnDidBindCanvasContext");
      if (on_did_bind_canvas_context == nullptr) {
        spdlog::warn(
            "[StationWarning] StationWarningViewController.OnDidBindCanvasContext not found; direct banner probe disabled");
      } else {
        SPUD_STATIC_DETOUR(on_did_bind_canvas_context, StationWarningViewController_OnDidBindCanvasContext_Hook);
      }

      auto on_animation_completed = station_warning_view_controller.GetMethod("OnAnimationCompleted");
      if (on_animation_completed == nullptr) {
        spdlog::warn(
            "[StationWarning] StationWarningViewController.OnAnimationCompleted not found; lifecycle probe incomplete");
      } else {
        SPUD_STATIC_DETOUR(on_animation_completed, StationWarningViewController_OnAnimationCompleted_Hook);
      }
    }
  }

  if (kEnableIncomingFleetNotificationHook) {
    auto notification_class =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Notification");
    if (!notification_class.isValidHelper()) {
      spdlog::warn("[IncomingFleet] Notification helper not found; materialization probe disabled");
    } else {
      auto set_incoming_fleet_params_json =
          notification_class.GetMethod<void(void*, IncomingFleetParamsJson*)>("set_IncomingFleetParamsJson", 1);
      if (set_incoming_fleet_params_json == nullptr) {
        spdlog::warn("[IncomingFleet] Notification.set_IncomingFleetParamsJson not found; materialization probe disabled");
      } else {
        SPUD_STATIC_DETOUR(set_incoming_fleet_params_json, Notification_SetIncomingFleetParamsJson_Hook);
      }
    }
  }

  auto navigation_interaction_ui_view_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController");
  if (!navigation_interaction_ui_view_controller.isValidHelper()) {
    spdlog::warn(
        "[NavigationInteractionUI] NavigationInteractionUIViewController helper not found; experimental hooks disabled");
  } else {
    if (kEnableNavigationHookOnEnable) {
      auto on_enable = navigation_interaction_ui_view_controller.GetMethod("OnEnable");
      if (on_enable == nullptr) {
        spdlog::warn(
            "[NavigationInteractionUI] NavigationInteractionUIViewController.OnEnable not found; experimental hook disabled");
      } else {
        SPUD_STATIC_DETOUR(on_enable, NavigationInteractionUIViewController_OnEnable_Hook);
      }
    }

    if (kEnableNavigationHookOnDisable) {
      auto on_disable = navigation_interaction_ui_view_controller.GetMethod("OnDisable");
      if (on_disable == nullptr) {
        spdlog::warn(
            "[NavigationInteractionUI] NavigationInteractionUIViewController.OnDisable not found; experimental hook disabled");
      } else {
        SPUD_STATIC_DETOUR(on_disable, NavigationInteractionUIViewController_OnDisable_Hook);
      }
    }

    if (kEnableNavigationHookAboutToShow) {
      auto about_to_show_canvas_event_handler =
          navigation_interaction_ui_view_controller.GetMethod("AboutToShowCanvasEventHandler", 2);
      if (about_to_show_canvas_event_handler == nullptr) {
        spdlog::warn(
            "[NavigationInteractionUI] NavigationInteractionUIViewController.AboutToShowCanvasEventHandler not found; experimental hook disabled");
      } else {
        SPUD_STATIC_DETOUR(about_to_show_canvas_event_handler,
                           NavigationInteractionUIViewController_AboutToShowCanvasEventHandler_Hook);
      }
    }

    if (kEnableNavigationHookDidHide) {
      auto did_hide_canvas_event_handler =
          navigation_interaction_ui_view_controller.GetMethod("DidHideCanvasEventHandler", 2);
      if (did_hide_canvas_event_handler == nullptr) {
        spdlog::warn(
            "[NavigationInteractionUI] NavigationInteractionUIViewController.DidHideCanvasEventHandler not found; experimental hook disabled");
      } else {
        SPUD_STATIC_DETOUR(did_hide_canvas_event_handler,
                           NavigationInteractionUIViewController_DidHideCanvasEventHandler_Hook);
      }
    }
  }

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

  SPUD_STATIC_DETOUR(handle_mining_depleted, ToastFleetObserver_HandleMiningDepleted_Hook);

  if (kEnableToastFleetObserverIncomingProducerHook) {
    auto notifications_changed_event_handler = toast_fleet_observer.GetMethod("NotificationsChangedEventHandler", 1);
    if (notifications_changed_event_handler == nullptr) {
      spdlog::warn("[IncomingAttack] ToastFleetObserver.NotificationsChangedEventHandler not found; early incoming notifications disabled");
    } else {
      SPUD_STATIC_DETOUR(notifications_changed_event_handler,
                         ToastFleetObserver_NotificationsChangedEventHandler_Hook);
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
