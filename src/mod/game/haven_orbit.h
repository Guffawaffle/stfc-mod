#pragma once

#include "app.h"
#include "camera.h"
#include "haven.h"

#include <unity/application.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <cmath>
#include <optional>

namespace haven_orbit_binding
{
using haven_binding::Field;
template <il2cpp::Name N, class B> bool Write(B* object, float value)
{
  static auto* field = il2cpp::detail::ResolveField(B::Class::Raw(), N.View(), il2cpp::detail::SlotOf<float>(), false);
  if (!object || !field)
    return false;
  il2cpp::detail::WriteField(field, object, value);
  return true;
}
struct Starbase : MonoSingleton<Starbase> {
  using Class =
      il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlanetaryBase.PlanetaryStarbase", "PlanetaryStarbaseManager">;
};

inline bool InHaven()
{
  auto*      sections = Hub::Sections();
  const auto section =
      sections ? sections->TryCall<"get_CurrentSection", SectionID>() : il2cpp::CallResult<SectionID>{};
  return section && section.value == SectionID::Starbase_Planetary;
}
inline bool PointerAvailable(bool continuing)
{
  const auto observed = unity::EventSystem::Class::TryCall<"get_current", unity::EventSystem*>();
  auto       events   = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  const auto over     = events ? events.Get<unity::EventSystem>()->TryCall<"IsPointerOverGameObject", bool(int32_t)>(-1)
                               : il2cpp::CallResult<bool>{};
  if (!over || (over.value && !continuing))
    return false;
  const auto manager = Starbase::Class::TryCall<"get_Instance", Starbase*>();
  auto       root    = il2cpp::GcHandle::Strong(manager ? manager.value : nullptr);
  const auto placement =
      root ? root.Get<Starbase>()->TryCall<"get_HasActivePlacement", bool>() : il2cpp::CallResult<bool>{};
  return placement && !placement.value;
}
inline std::optional<float> Radius(AbstractOrbitFrameProvider* orbit)
{
  if (!orbit || !orbit->Is<PlanetaryOrbitFrameProvider>())
    return std::nullopt;
  const auto observed   = Field<"_constraint", OrbitConstraint*>(orbit);
  auto       constraint = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  const auto data       = constraint ? Field<"_radius", FloatConstraint*>(constraint.Get<OrbitConstraint>())
                                     : il2cpp::CallResult<FloatConstraint*>{};
  auto       root       = il2cpp::GcHandle::Strong(data ? data.value : nullptr);
  if (!root)
    return std::nullopt;
  auto*      range   = root.Get<FloatConstraint>();
  const auto enabled = Field<"_enabled", bool>(range);
  const auto minimum = Field<"_min", float>(range);
  const auto maximum = Field<"_max", float>(range);
  const auto radius  = Field<"_radius", float>(orbit);
  if (!enabled || !enabled.value || !minimum || !maximum || !radius || !std::isfinite(minimum.value)
      || !std::isfinite(maximum.value) || !std::isfinite(radius.value) || maximum.value <= 0
      || minimum.value != maximum.value || std::abs(radius.value - maximum.value) >= 0.01f)
    return std::nullopt;
  return radius.value;
}
inline std::optional<float> FixedElevation(AbstractOrbitFrameProvider* orbit)
{
  const auto observed   = Field<"_constraint", OrbitConstraint*>(orbit);
  auto       constraint = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  const auto data       = constraint ? Field<"_elevation", FloatConstraint*>(constraint.Get<OrbitConstraint>())
                                     : il2cpp::CallResult<FloatConstraint*>{};
  auto       root       = il2cpp::GcHandle::Strong(data ? data.value : nullptr);
  if (!root)
    return std::nullopt;
  auto*      range   = root.Get<FloatConstraint>();
  const auto enabled = Field<"_enabled", bool>(range);
  const auto minimum = Field<"_min", float>(range);
  const auto maximum = Field<"_max", float>(range);
  return enabled && enabled.value && minimum && maximum && std::isfinite(minimum.value) && std::isfinite(maximum.value)
                 && std::abs(minimum.value - maximum.value) < 0.0001f
             ? std::optional{minimum.value}
             : std::nullopt;
}
inline bool Available()
{
  return AbstractOrbitFrameProvider::Class::HasField<"_rotationAngle", float>()
         && AbstractOrbitFrameProvider::Class::HasField<"_elevationAngle", float>()
         && AbstractOrbitFrameProvider::Class::HasField<"_constraint", OrbitConstraint*>()
         && AbstractOrbitFrameProvider::Class::HasField<"_radius", float>()
         && AbstractOrbitFrameProvider::Class::HasField<"_orbitPivot", TargetController*>()
         && AbstractOrbitFrameProvider::Class::HasField<"_lookTarget", TargetController*>()
         && OrbitConstraint::Class::HasField<"_elevation", FloatConstraint*>()
         && OrbitConstraint::Class::HasField<"_radius", FloatConstraint*>()
         && FloatConstraint::Class::HasField<"_enabled", bool>() && FloatConstraint::Class::HasField<"_min", float>()
         && FloatConstraint::Class::HasField<"_max", float>()
         && BlendFrameProvider::Class::HasField<"_sourceFrameProvider", FrameProvider*>()
         && BlendFrameProvider::Class::HasField<"_targetFrameProvider", FrameProvider*>()
         && Starbase::Class::StaticMethod<Starbase*()>("get_Instance")
         && Starbase::Class::Method<bool()>("get_HasActivePlacement")
         && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current")
         && unity::EventSystem::Class::Method<bool(int32_t)>("IsPointerOverGameObject")
         && unity::Application::Class::StaticMethod<bool()>("get_isFocused") && unity::Time::FrameCountAvailable()
         && unity::Input::PointerAvailable();
}
} // namespace haven_orbit_binding
