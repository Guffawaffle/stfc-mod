#include "ship_identity.h"

#include "il2cpp/method_contract.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

namespace
{
struct Methods {
  const MethodInfo* ship = nullptr;
  const MethodInfo* id   = nullptr;
};

const Methods& GetMethods()
{
  static const Methods methods = [] {
    auto fleet = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models",
                                         "FleetPlayerData");
    auto ship = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Ship");
    return Methods{method_contract::Resolve(fleet.get_cls(), "get_Ship", false, "Digit.PrimeServer.Models.Ship", {}),
                   method_contract::Resolve(ship.GetParent("BaseShip").get_cls(), "get_Id", false, "System.Int64",
                                            {})};
  }();
  return methods;
}
} // namespace

namespace ship_identity
{
bool Available()
{
  const auto& methods = GetMethods();
  return methods.ship && methods.id;
}

std::optional<int64_t> InstanceId(FleetPlayerData* fleet)
{
  const auto& methods = GetMethods();
  if (!fleet || !methods.ship || !methods.id)
    return std::nullopt;

  Il2CppException* exception = nullptr;
  auto* ship = il2cpp_runtime_invoke(methods.ship, fleet, nullptr, &exception);
  if (exception || !ship)
    return std::nullopt;

  exception = nullptr;
  auto* boxed_id = il2cpp_runtime_invoke(methods.id, ship, nullptr, &exception);
  if (exception || !boxed_id)
    return std::nullopt;

  const auto id = *static_cast<int64_t*>(il2cpp_object_unbox(boxed_id));
  return id > 0 ? std::optional{id} : std::nullopt;
}
} // namespace ship_identity
