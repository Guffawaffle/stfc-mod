#pragma once

// Native mining and HUD contracts used by the downstream PLAY OPC indicators.
#include "fleet.h"

#include <unity/ui.h>

namespace fleet_opc_binding
{
struct Progress : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ProgressData">;
  il2cpp::CallResult<double> Current()
  { return TryCall<"get_CurrentValue", double>(); }
  il2cpp::CallResult<double> Protected()
  { return TryCall<"get_MinValue", double>(); }
};
struct Hold : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CargoHoldData">;
  il2cpp::CallResult<Progress*> Unprotected()
  { return TryCall<"get_UnprotectedCargoProgress", Progress*>(); }
};
struct Mining : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IMiningSlot">;
  il2cpp::CallResult<float> Speed()
  { return TryCall<"get_MiningSpeed", float>(); }
  il2cpp::CallResult<double> Mined()
  { return TryCall<"get_AmountMined", double>(); }
  il2cpp::CallResult<int64_t> Amount()
  { return TryCall<"get_Amount", int64_t>(); }
};
struct Timer : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "TimerWidget">;
};
struct StateWidget : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget">;
};
struct FlagWidget : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FleetbarFlagWidget">;
};

inline il2cpp::CallResult<Hold*> Cargo(FleetPlayerData* fleet)
{ return fleet ? fleet->TryCall<"get_CargoHoldData", Hold*>() : il2cpp::CallResult<Hold*>{}; }
inline il2cpp::CallResult<Mining*> MiningData(FleetPlayerData* fleet)
{ return fleet ? fleet->TryCall<"get_MiningData", Mining*>() : il2cpp::CallResult<Mining*>{}; }
inline il2cpp::CallResult<FleetPlayerData*> Fleet(il2cpp::Object* widget, bool local_view)
{
  return !widget      ? il2cpp::CallResult<FleetPlayerData*>{}
         : local_view ? widget->TryCall<"get_fleet", FleetPlayerData*>()
                      : widget->TryCall<"get_Context", FleetPlayerData*>();
}
inline unity::Transform* Transform(il2cpp::Object* object)
{
  auto* component = object ? object->As<unity::Component>() : nullptr;
  return component ? component->transform() : nullptr;
}
inline FleetLocalViewController* LocalView(il2cpp::Object* object)
{
  auto* transform = Transform(object);
  return transform ? transform->FindInParents<FleetLocalViewController>() : nullptr;
}
inline FleetBarViewController* Bar(il2cpp::Object* object)
{
  auto* transform = Transform(object);
  return transform ? transform->FindInParents<FleetBarViewController>() : nullptr;
}
inline unity::Transform* Body(il2cpp::Object* object)
{
  auto* view      = LocalView(object);
  auto* transform = Transform(view);
  return transform ? transform->Find("BodyContainer") : nullptr;
}
inline unity::Transform* TimerAnchor(FleetLocalViewController* panel)
{
  auto* timer = panel ? panel->Field<"_timerWidget", Timer*>() : nullptr;
  return timer ? timer->transform() : nullptr;
}
inline unity::GameObject* ChildObject(unity::Transform* parent, std::string_view name)
{
  auto* child = parent ? parent->Find(name) : nullptr;
  return child ? child->gameObject() : nullptr;
}
inline void Name(il2cpp::Object* object, std::string_view name)
{
  if (object)
    (void)object->TryCall<"set_name", void>(il2cpp::NewString(name));
}
} // namespace fleet_opc_binding
