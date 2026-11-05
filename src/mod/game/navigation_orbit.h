#pragma once

// Checked access used by the system-only orbit. Discovery never invokes Unity.
#include "game/navigation.h"

#include <unity/application.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <cmath>

namespace navigation_orbit_binding
{
using Vector3 = unity::Vector3;
struct Poi : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "POI">;
};
struct NavigationCamera {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationCamera">;
};

inline bool Finite(Vector3 value)
{ return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

template <il2cpp::Name N, class T, class B> il2cpp::CallResult<T> Field(B* object)
{
  static auto* field = il2cpp::detail::ResolveField(B::Class::Raw(), N.View(), il2cpp::detail::SlotOf<T>(), false);
  return object && field ? il2cpp::CallResult<T>{true, il2cpp::detail::ReadField<T>(field, object)}
                         : il2cpp::CallResult<T>{};
}
template <il2cpp::Name N, class T, class B> bool Write(B* object, T value)
{
  static auto* field = il2cpp::detail::ResolveField(B::Class::Raw(), N.View(), il2cpp::detail::SlotOf<T>(), false);
  if (!object || !field)
    return false;
  il2cpp::detail::WriteField(field, object, value);
  return true;
}
inline bool InSystem()
{
  auto*      sections = Hub::Sections();
  const auto observed =
      sections ? sections->TryCall<"get_CurrentSection", SectionID>() : il2cpp::CallResult<SectionID>{};
  return observed && observed.value == SectionID::Navigation_System;
}
inline il2cpp::CallResult<Vector3> Vector(const MethodInfo* method, Il2CppObject* object)
{
  const auto observed = unity::TryReadVector3(method, object);
  return observed && Finite(observed.value) ? observed : il2cpp::CallResult<Vector3>{};
}
template <il2cpp::Name N> const MethodInfo* VectorGetter()
{
  static const auto* method =
      il2cpp::detail::MethodCall<Vector3()>::Resolve(unity::Transform::Class::Raw(), N.View(), false);
  return method;
}
template <il2cpp::Name N> bool SetVector(unity::Transform* object, Vector3 value)
{
  static const auto* method = [] {
    const auto* candidate =
        il2cpp::detail::MethodCall<void(Vector3)>::Resolve(unity::Transform::Class::Raw(), N.View(), false);
    return candidate && candidate->invoker_method && candidate->parameters
                   && il2cpp::detail::ValueClassMatches(candidate->parameters[0], unity::Vector3Class(),
                                                        sizeof(Vector3))
               ? candidate
               : nullptr;
  }();
  if (!object || !Finite(value) || !method)
    return false;
  void* args[]{&value};
  return il2cpp::detail::TryInvokeBoxed(method, object, args).succeeded;
}
inline il2cpp::CallResult<unity::Transform*> Transform(Il2CppObject* object)
{
  static const auto* method =
      il2cpp::detail::MethodCall<unity::Transform*()>::Resolve(unity::Component::Class::Raw(), "get_transform", false);
  if (!object || !method)
    return {};
  const auto observed = il2cpp::detail::TryInvokeBoxed(method, object, nullptr);
  if (!observed || !observed.value)
    return {};
  auto* result = reinterpret_cast<il2cpp::Object*>(observed.value)->As<unity::Transform>();
  return result ? il2cpp::CallResult<unity::Transform*>{true, result} : il2cpp::CallResult<unity::Transform*>{};
}
inline bool SetView(unity::Transform* transform, Vector3 angles, float distance)
{
  if (!transform || !Finite(angles) || !std::isfinite(distance) || distance <= 0
      || !SetVector<"set_localEulerAngles">(transform, angles))
    return false;
  const auto forward = Vector(VectorGetter<"get_forward">(), transform);
  return forward && SetVector<"set_localPosition">(transform, forward.value * -distance);
}
inline bool Reanchor(NavigationZoom* zoom)
{
  const auto         camera   = Field<"_sceneCamera", unity::Camera*>(zoom);
  const auto         location = Field<"_zoomLocation", unity::Vector2>(zoom);
  static const auto* method   = [] {
    const auto* candidate = il2cpp::detail::MethodCall<Vector3(unity::Camera*, Vector3)>::Resolve(
        MathUtils::Class::Raw(), "GetMouseWorldPos", true);
    return candidate && candidate->invoker_method
                   && il2cpp::detail::ValueClassMatches(candidate->return_type, unity::Vector3Class(), sizeof(Vector3))
                   && candidate->parameters
                   && il2cpp::detail::ValueClassMatches(candidate->parameters[1], unity::Vector3Class(),
                                                        sizeof(Vector3))
               ? candidate
               : nullptr;
  }();
  if (!camera || !camera.value || !location || !method)
    return false;
  Vector3 screen{location.value.x, location.value.y, 0};
  if (!Finite(screen))
    return false;
  auto       camera_root = il2cpp::GcHandle::Strong(camera.value);
  void*      args[]{camera_root.Get(), &screen};
  const auto observed = il2cpp::detail::TryInvokeBoxed(method, nullptr, args);
  auto       box      = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  if (!box || !box.Get()->klass
      || !il2cpp::detail::ValueClassMatches(&box.Get()->klass->byval_arg, unity::Vector3Class(), sizeof(Vector3)))
    return false;
  const auto world = il2cpp::detail::FromBoxed<Vector3>(box.Get());
  return Finite(world) && Write<"_worldPoint">(zoom, world);
}
inline il2cpp::CallResult<bool> PointerAvailable(bool continuing)
{
  const auto move     = NavigationCamera::Class::TryCall<"CanMoveCamera", bool>();
  auto*      tutorial = Hub::Tutorial();
  if (!move)
    return {};
  if (!move.value || !tutorial || !tutorial->IsSceneInputAllowed() || !ShortcutsManager::CanUseShortcuts()
      || unity::Key::IsInputFocused())
    return {true, false};
  const auto current = unity::EventSystem::Class::TryCall<"get_current", unity::EventSystem*>();
  auto       events  = il2cpp::GcHandle::Strong(current ? current.value : nullptr);
  if (!events)
    return {true, false};
  const auto over = events.Get<unity::EventSystem>()->TryCall<"IsPointerOverGameObject", bool(int32_t)>(-1);
  return over ? il2cpp::CallResult<bool>{true, !over.value || continuing} : il2cpp::CallResult<bool>{};
}
inline bool Available()
{
  return NavigationZoom::Class::HasField<"_sceneCamera", unity::Camera*>()
         && NavigationZoom::Class::HasField<"_actualDistance", float>()
         && NavigationZoom::Class::HasField<"_depth", NodeDepth>()
         && NavigationZoom::Class::HasField<"_zoomLocation", unity::Vector2>()
         && NavigationZoom::Class::HasField<"_worldPoint", Vector3>()
         && NavigationZoom::Class::HasField<"<NormalizedZoom>k__BackingField", float>()
         && NavigationPan::Class::HasField<"_sceneCamera", unity::Camera*>()
         && NavigationPan::Class::HasField<"_nodeDepth", NodeDepth>()
         && NavigationPan::Class::HasField<"_systemPosition", Vector3>()
         && NavigationPan::Class::HasField<"_viewRadius", float>()
         && NavigationPan::Class::HasField<"_dragDelta", unity::Vector2>()
         && NavigationPan::Class::HasField<"_trackingPOI", Poi*>()
         && NavigationPan::Class::HasField<"_minPanSpeed", float>() && VectorGetter<"get_localEulerAngles">()
         && VectorGetter<"get_forward">() && VectorGetter<"get_position">()
         && unity::Transform::Class::Method<void(Vector3)>("set_localEulerAngles")
         && unity::Transform::Class::Method<void(Vector3)>("set_localPosition")
         && unity::Camera::Class::Method<float()>("get_farClipPlane")
         && unity::Camera::Class::Method<void(float)>("set_farClipPlane")
         && unity::Camera::Class::Method<float()>("get_fieldOfView")
         && unity::Camera::Class::Method<int32_t()>("get_pixelHeight")
         && unity::Component::Class::Method<unity::Transform*()>("get_transform")
         && NavigationCamera::Class::StaticMethod<bool()>("CanMoveCamera")
         && MathUtils::Class::StaticMethod<Vector3(unity::Camera*, Vector3)>("GetMouseWorldPos")
         && unity::EventSystem::Class::StaticMethod<unity::EventSystem*()>("get_current")
         && unity::EventSystem::Class::Method<bool(int32_t)>("IsPointerOverGameObject")
         && unity::Application::Class::StaticMethod<bool()>("get_isFocused") && unity::Time::FrameCountAvailable()
         && unity::Input::PointerAvailable();
}
} // namespace navigation_orbit_binding
