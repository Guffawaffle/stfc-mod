#pragma once

#include "game/navigation_orbit.h"

#include <unity/scene.h>

namespace navigation_environment_binding
{
struct Flat : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderable">;
};
struct Loader : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.Rendering", "FlatRenderableLoader">;
};
inline il2cpp::CallResult<il2cpp::Array<il2cpp::Object*>*> Loaders()
{
  auto*      type     = Loader::Class::TypeObject();
  const auto observed = type ? unity::UnityObject::Class::TryCall<"FindObjectsOfType", il2cpp::ArrayObject*>(type)
                             : il2cpp::CallResult<il2cpp::ArrayObject*>{};
  return observed ? il2cpp::TryReferenceArray<il2cpp::Object*>(observed.value)
                  : il2cpp::CallResult<il2cpp::Array<il2cpp::Object*>*>{};
}
inline il2cpp::CallResult<il2cpp::Array<Flat*>*> Pool(Loader* loader)
{
  const auto observed = navigation_orbit_binding::Field<"_pool", il2cpp::Array<Flat*>*>(loader);
  return observed ? il2cpp::TryReferenceArray<Flat*>(observed.value) : il2cpp::CallResult<il2cpp::Array<Flat*>*>{};
}
inline il2cpp::CallResult<unity::MeshRenderer*> Renderer(Flat* flat)
{ return navigation_orbit_binding::Field<"MeshRenderer", unity::MeshRenderer*>(flat); }
inline bool Available()
{
  return Loader::Class::HasField<"_pool", il2cpp::Array<Flat*>*>()
         && Flat::Class::HasField<"MeshRenderer", unity::MeshRenderer*>()
         && Loader::Class::Method<Flat*()>("get_Background")
         && unity::UnityObject::Class::StaticMethod<il2cpp::ArrayObject*(Il2CppReflectionType*)>("FindObjectsOfType")
         && unity::Mesh::Class::Raw() && unity::MeshFilter::Class::Raw() && unity::MeshRenderer::Class::Raw()
         && unity::Shader::Class::StaticMethod<unity::Shader*(Il2CppString*)>("Find")
         && unity::Texture2D::Class::Method<void(int32_t, int32_t, int32_t, bool)>(".ctor")
         && unity::Mesh::Class::Method<void()>(".ctor") && unity::Material::Class::Method<void(unity::Shader*)>(".ctor")
         && unity::Time::FrameCountAvailable();
}
} // namespace navigation_environment_binding
