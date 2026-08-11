#include "patches/hook_registry.h"
#include "patches/repair_action_interlock_policy.h"
#include "prime/ActionData.h"
#include "prime/FleetPlayerData.h"
#include "prime/GenericButtonContext.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace
{
constexpr HookDescriptor kRepairActionStatusHook{
    "FleetPlayerData.GetActionStatus(ActionType)",
    "Preserve the last coherent Repair progress status while the client model converges.",
    {"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData", "GetActionStatus"},
    "Repair action status can briefly regress to a clickable Ready state while repair is still active.",
    HookSupportTier::Production};

constexpr HookDescriptor kRepairInstantButtonContextHook{
    "ActionElementWidget.GetInstantButtonContext()",
    "Hold the current Repair instant-button presentation across a bounded stale post-completion transition.",
    {"Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget", "GetInstantButtonContext"},
    "Repair can briefly present a stale instant action after completion.",
    HookSupportTier::Production};

constexpr HookDescriptor kRepairInstantButtonClickHook{
    "ActionElementWidget.OnInstantButtonClickCallback()",
    "Suppress stale Repair instant clicks only during the bounded post-completion hold.",
    {"Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget", "OnInstantButtonClickCallback"},
    "A stale post-completion Repair instant click can reach the game's paid or error path.",
    HookSupportTier::Production};

struct ActionElementWidget;

repair_action_interlock::State g_state;
std::mutex                     g_state_mutex;

uint64_t MonotonicMilliseconds()
{
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

IL2CppClassHelper& ActionElementWidgetHelper()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Actions", "ActionElementWidget");
  return helper;
}

ActionType ReadActionType(ActionElementWidget* widget)
{
  static auto property = ActionElementWidgetHelper().GetProperty("ActionType");
  const auto* value    = property.Get<ActionType>(widget);
  return value == nullptr ? ActionType::Invalid : *value;
}

FleetPlayerData* ReadFleetContext(ActionElementWidget* widget)
{
  static auto property = ActionElementWidgetHelper().GetProperty("Context");
  auto*       context  = property.GetRaw<void>(widget);
  if (context == nullptr) {
    return nullptr;
  }

  auto* klass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(context));
  if (klass == nullptr) {
    return nullptr;
  }

  const auto* class_name      = il2cpp_class_get_name(klass);
  const auto* class_namespace = il2cpp_class_get_namespace(klass);
  if (class_name == nullptr || class_namespace == nullptr || std::strcmp(class_name, "FleetPlayerData") != 0
      || std::strcmp(class_namespace, "Digit.PrimeServer.Models") != 0) {
    return nullptr;
  }
  return reinterpret_cast<FleetPlayerData*>(context);
}

GenericButtonContext* ReadCurrentInstantButtonContext(ActionElementWidget* widget)
{
  static auto field = ActionElementWidgetHelper().GetField("_instantButtonContext");
  if (widget == nullptr || !field.isValidHelper()) {
    return nullptr;
  }
  return *reinterpret_cast<GenericButtonContext**>(reinterpret_cast<char*>(widget) + field.offset());
}

bool ReadInstantAmount(GenericButtonContext* context, int64_t& amount)
{
  static auto button_helper     = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericButtonContext");
  static auto resource_property = button_helper.GetProperty("ResourceData");
  auto*       resource          = resource_property.GetRaw<void>(context);
  if (resource == nullptr) {
    return false;
  }

  static auto resource_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceData");
  static auto amount_property = resource_helper.GetProperty("Amount");
  const auto* value           = amount_property.Get<int64_t>(resource);
  if (value == nullptr) {
    return false;
  }

  amount = *value;
  return true;
}

int32_t FleetPlayerData_GetActionStatus_Hook(auto original, FleetPlayerData* fleet, ActionType action_type)
{
  const auto original_status = original(fleet, action_type);
  if (fleet == nullptr || action_type != ActionType::Repair) {
    return original_status;
  }

  const std::scoped_lock lock(g_state_mutex);
  return g_state.project_status(fleet->Id, original_status, static_cast<int32_t>(fleet->CurrentState));
}

GenericButtonContext* ActionElementWidget_GetInstantButtonContext_Hook(auto original, ActionElementWidget* widget)
{
  auto* context = original(widget);
  if (widget == nullptr || ReadActionType(widget) != ActionType::Repair) {
    return context;
  }

  auto* fleet = ReadFleetContext(widget);
  if (fleet == nullptr) {
    return context;
  }

  repair_action_interlock::InstantContextSnapshot snapshot{};
  snapshot.context_present = context != nullptr;
  if (context != nullptr) {
    snapshot.interactable = context->Interactable;
    snapshot.has_amount   = ReadInstantAmount(context, snapshot.amount);
  }

  repair_action_interlock::PresentationDecision decision{};
  {
    const std::scoped_lock lock(g_state_mutex);
    decision =
        g_state.observe_instant_context(fleet->Id, static_cast<int32_t>(fleet->CurrentState),
                                        static_cast<int32_t>(fleet->PreviousState), snapshot, MonotonicMilliseconds());
  }

  if (!decision.hold) {
    return context;
  }

  auto* live_context = ReadCurrentInstantButtonContext(widget);
  return live_context == nullptr ? context : live_context;
}

bool ShouldSuppressInstantClick(ActionElementWidget* widget)
{
  if (widget == nullptr || ReadActionType(widget) != ActionType::Repair) {
    return false;
  }

  auto* fleet = ReadFleetContext(widget);
  if (fleet == nullptr) {
    return false;
  }

  const std::scoped_lock lock(g_state_mutex);
  return g_state.should_suppress_instant_click(fleet->Id, static_cast<int32_t>(fleet->CurrentState),
                                               static_cast<int32_t>(fleet->PreviousState), MonotonicMilliseconds());
}

void ActionElementWidget_OnInstantButtonClickCallback_Hook(auto original, ActionElementWidget* widget)
{
  if (ShouldSuppressInstantClick(widget)) {
    spdlog::info("[RepairActionInterlock] Suppressed a stale post-completion Repair instant click");
    return;
  }
  original(widget);
}
} // namespace

void InstallRepairActionInterlockHooks()
{
  HookModuleHealth hooks("RepairActionInterlock");

  static auto fleet_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
  if (!fleet_helper.isValidHelper()) {
    hooks.record_missing_helper(kRepairActionStatusHook);
  } else if (auto method = fleet_helper.GetMethod("GetActionStatus", 1); method == nullptr) {
    hooks.record_missing_method(kRepairActionStatusHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairActionStatusHook, method, FleetPlayerData_GetActionStatus_Hook);
  }

  auto& widget_helper = ActionElementWidgetHelper();
  if (!widget_helper.isValidHelper()) {
    hooks.record_missing_helper(kRepairInstantButtonContextHook);
    hooks.record_missing_helper(kRepairInstantButtonClickHook);
  } else {
    if (auto method = widget_helper.GetMethod("GetInstantButtonContext", 0); method == nullptr) {
      hooks.record_missing_method(kRepairInstantButtonContextHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairInstantButtonContextHook, method,
                                       ActionElementWidget_GetInstantButtonContext_Hook);
    }

    if (auto method = widget_helper.GetMethod("OnInstantButtonClickCallback", 0); method == nullptr) {
      hooks.record_missing_method(kRepairInstantButtonClickHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kRepairInstantButtonClickHook, method,
                                       ActionElementWidget_OnInstantButtonClickCallback_Hook);
    }
  }

  hooks.log_summary();
}
