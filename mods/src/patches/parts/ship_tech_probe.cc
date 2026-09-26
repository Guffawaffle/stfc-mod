#ifdef _MODDBG
#include "il2cpp/method_contract.h"
#include "prime/ShipTileWidget.h"

#include <spud/detour.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdlib>

namespace
{
constexpr int64_t kForbiddenSlot = 3502081615;
constexpr int64_t kChaosSlot = 953301906;
constexpr unsigned kBindLimit = 512;

struct Methods {
  const MethodInfo* context = nullptr;
  const MethodInfo* ship = nullptr;
  const MethodInfo* id = nullptr;
  const MethodInfo* hull = nullptr;
  const MethodInfo* has_slots = nullptr;
  const MethodInfo* equipped = nullptr;
  const MethodInfo* state = nullptr;
} methods;

Il2CppObject* Invoke(const MethodInfo* method, void* instance, void** args = nullptr)
{
  if (!instance) return nullptr;
  Il2CppException* exception = nullptr;
  auto* result = il2cpp_runtime_invoke(method, instance, args, &exception);
  if (exception) {
    spdlog::warn("[ShipTechProbe] accessor={} threw; sample unavailable", method->name);
    return nullptr;
  }
  return result;
}

template <typename T> T Value(const MethodInfo* method, void* instance, T unknown)
{
  auto* result = Invoke(method, instance);
  return result ? *static_cast<T*>(il2cpp_object_unbox(result)) : unknown;
}

struct Slot {
  int state = -1;
  int equipped = -1;
  int64_t id = 0;
};

Slot ReadSlot(Il2CppObject* ship, int64_t slot_id)
{
  Slot result;
  void* state_args[]{&slot_id};
  if (auto* state = Invoke(methods.state, ship, state_args))
    result.state = *static_cast<int32_t*>(il2cpp_object_unbox(state));
  void* equipped_args[]{&result.id, &slot_id};
  if (auto* equipped = Invoke(methods.equipped, ship, equipped_args))
    result.equipped = *static_cast<bool*>(il2cpp_object_unbox(equipped)) ? 1 : 0;
  return result;
}

void SetWidgetData_Hook(auto original, ShipTileWidget* widget)
{
  original(widget);
  static unsigned samples = 0;
  if (samples >= kBindLimit) return;
  ++samples;
  auto* context = Invoke(methods.context, widget);
  auto* ship = Invoke(methods.ship, context);
  auto* slots = Invoke(methods.has_slots, ship);
  const int has_slots = slots ? (*static_cast<bool*>(il2cpp_object_unbox(slots)) ? 1 : 0) : -1;
  const auto ft = ReadSlot(ship, kForbiddenSlot);
  const auto ct = ReadSlot(ship, kChaosSlot);
  spdlog::info("[ShipTechProbe] bind={} widget={} context={} ship={} id={} hull={} slots={} "
               "FT(state={},equipped={},id={}) CT(state={},equipped={},id={})",
               samples, static_cast<void*>(widget), static_cast<void*>(context), static_cast<void*>(ship),
               Value<int64_t>(methods.id, ship, -1), Value<int64_t>(methods.hull, ship, -1),
               has_slots,
               ft.state, ft.equipped, ft.id, ct.state, ct.equipped, ct.id);
  if (samples == kBindLimit)
    spdlog::info("[ShipTechProbe] capture limit reached; no further samples this session");
}
} // namespace

void InstallShipTechProbe()
{
  auto widget = ShipTileWidget::get_class_helper();
  auto fleet = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
  auto ship = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Ship");
  auto base_ship = ship.GetParent("BaseShip");
  auto parent = widget.GetParent("Widget`1");
  auto* property = parent.get_cls() ? il2cpp_class_get_property_from_name(parent.get_cls(), "Context") : nullptr;
  methods.context = property ? property->get : nullptr;
  methods.ship = method_contract::Resolve(fleet.get_cls(), "get_Ship", false, "Digit.PrimeServer.Models.Ship", {});
  methods.id = method_contract::Resolve(base_ship.get_cls(), "get_Id", false, "System.Int64", {});
  methods.hull = method_contract::Resolve(ship.get_cls(), "get_HullId", false, "System.Int64", {});
  methods.has_slots = method_contract::Resolve(ship.get_cls(), "get_HasForbiddenTechSlots", false, "System.Boolean", {});
  methods.state = method_contract::Resolve(ship.get_cls(), "GetForbiddenTechSlotState", false,
                                          "Digit.PrimeServer.Models.ForbiddenTechSlotState", {"System.Int64"});
  methods.equipped = ship.GetMethodInfo("TryGetEquippedForbiddenTechId", 2);
  // This accessor has an out Int64; the ordinary non-byref resolver intentionally rejects it.
  const auto* equipped = methods.equipped;
  const bool equipped_valid = equipped && equipped->methodPointer && !(equipped->flags & METHOD_ATTRIBUTE_STATIC)
      && method_contract::Type(equipped->return_type, "System.Boolean") && equipped->parameters_count == 2
      && equipped->parameters[0]->byref && equipped->parameters[0]->type == IL2CPP_TYPE_I8
      && method_contract::Type(equipped->parameters[1], "System.Int64");
  auto* target = method_contract::Resolve(widget.get_cls(), "SetWidgetData", false, "System.Void", {});
  if (!target || !methods.context || !methods.context->methodPointer || !methods.ship || !methods.id || !methods.hull || !methods.has_slots
      || !methods.state || !equipped_valid) {
    spdlog::critical("[ShipTechProbe] required managed accessor unavailable: hook={} context={} ship={} id={} hull={} "
                     "slots={} state={} equipped={}", bool(target), bool(methods.context), bool(methods.ship),
                     bool(methods.id), bool(methods.hull), bool(methods.has_slots), bool(methods.state), equipped_valid);
    std::abort();
  }
  SPUD_STATIC_DETOUR(method_contract::Pointer(target), SetWidgetData_Hook);
  spdlog::info("[ShipTechProbe] installed ShipTileWidget.SetWidgetData; limit={} binds; unknown=-1", kBindLimit);
}
#endif
