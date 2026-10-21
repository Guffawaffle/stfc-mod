#pragma once

#include "prime/Color.h"
#include "prime/Vector2.h"
#include <cstdint>
#include <cstring>
#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>

namespace battle_log_ui
{
// All Unity layout work runs on ScreenManager's main-thread dispatcher.
// The shared class lookup dereferences missing assemblies. These optional UI
// classes can be absent after a client update, so check each lookup step.
inline Il2CppClass* Class(const char* assembly, const char* ns, const char* name)
{
  auto* domain = il2cpp_domain_get();
  auto* a      = domain ? il2cpp_domain_assembly_open(domain, assembly) : nullptr;
  auto* image  = a ? il2cpp_assembly_get_image(a) : nullptr;
  return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
}

inline bool TryInvoke(const MethodInfo* method, void* target, void** args, Il2CppObject** result = nullptr)
{
  if (!method)
    return false;
  Il2CppException* exception = nullptr;
  auto*            value     = il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception)
    return false;
  if (result)
    *result = value;
  return true;
}

// Unity has Type/string and generic overloads with the same argument count.
inline const MethodInfo* TypeMethod(Il2CppClass* cls, const char* name)
{
  // The shared filtered lookup cannot inspect is_generic and only searches
  // one class. Keep the overload check here so a generic overload cannot win.
  void* iter = nullptr;
  while (cls) {
    while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
      if (method->is_generic || std::strcmp(method->name, name) || method->parameters_count != 1)
        continue;
      auto* param = il2cpp_class_from_type(il2cpp_method_get_param(method, 0));
      if (param && !std::strcmp(il2cpp_class_get_namespace(param), "System")
          && !std::strcmp(il2cpp_class_get_name(param), "Type"))
        return method;
    }
    cls  = il2cpp_class_get_parent(cls);
    iter = nullptr;
  }
  return nullptr;
}

inline Il2CppObject* Get(Il2CppObject* object, const char* name)
{
  // Callers pass getter method names. Use the shared property helper for its
  // virtual dispatch and exception handling, while accepting absent objects.
  if (!object || std::strncmp(name, "get_", 4))
    return nullptr;
  return IL2CppClassHelper(object->klass).GetProperty(name + 4).GetRaw<Il2CppObject>(object);
}

template <typename T> bool Value(Il2CppObject* object, const char* name, T& value)
{
  if (!object)
    return false;
  Il2CppObject* boxed = nullptr;
  if (!std::strncmp(name, "get_", 4))
    boxed = Get(object, name);
  else if (!TryInvoke(IL2CppClassHelper(object->klass).GetMethodInfo(name, 0), object, nullptr, &boxed))
    return false;
  uint32_t alignment = 0;
  if (!boxed || !il2cpp_class_is_valuetype(boxed->klass)
      || il2cpp_class_value_size(boxed->klass, &alignment) != sizeof(T))
    return false;
  std::memcpy(&value, il2cpp_object_unbox(boxed), sizeof(T));
  return true;
}

inline bool Set(Il2CppObject* object, const char* name, void* value)
{
  void* args[] = {value};
  return object && TryInvoke(IL2CppClassHelper(object->klass).GetMethodInfo(name, 1), object, args);
}

inline Il2CppObject* Field(Il2CppObject* object, const char* name)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  if (!field || il2cpp_type_is_byref(il2cpp_field_get_type(field)))
    return nullptr;
  auto* cls = il2cpp_class_from_type(il2cpp_field_get_type(field));
  if (!cls || il2cpp_class_is_valuetype(cls))
    return nullptr;
  Il2CppObject* result = nullptr;
  il2cpp_field_get_value(object, field, &result);
  return result;
}

inline Il2CppObject* WithType(const MethodInfo* method, Il2CppObject* target, Il2CppClass* type)
{
  if (!type)
    return nullptr;
  void*         args[] = {il2cpp_type_get_object(il2cpp_class_get_type(type))};
  Il2CppObject* result = nullptr;
  TryInvoke(method, target, args, &result);
  return result;
}

inline Il2CppClass* UnityObject()
{
  static auto* cls = Class("UnityEngine.CoreModule", "UnityEngine", "Object");
  return cls;
}

inline bool Alive(Il2CppObject* object)
{
  if (!object)
    return false;
  void*         args[] = {object};
  Il2CppObject* result = nullptr;
  static auto*  method = IL2CppClassHelper(UnityObject()).GetMethodInfo("op_Implicit", 1);
  if (!TryInvoke(method, nullptr, args, &result) || !result || !result->klass)
    return false;
  const auto* type = il2cpp_class_get_type(result->klass);
  if (!type || type->byref || type->type != IL2CPP_TYPE_BOOLEAN)
    return false;
  auto* alive = static_cast<bool*>(il2cpp_object_unbox(result));
  return alive && *alive;
}

struct Root {
  Il2CppGCHandle handle{};
  Il2CppObject*  get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
  void reset(Il2CppObject* value = nullptr)
  {
    if (handle)
      il2cpp_gchandle_free(handle);
    handle = value ? il2cpp_gchandle_new(value, false) : 0;
  }
};

inline Il2CppObject* Find(Il2CppClass* type)
{
  static auto* method = TypeMethod(UnityObject(), "FindObjectOfType");
  return WithType(method, nullptr, type);
}

inline Il2CppObject* Component(Il2CppObject* object, Il2CppClass* type)
{ return object ? WithType(TypeMethod(object->klass, "GetComponent"), object, type) : nullptr; }

inline Il2CppObject* NewObject(const char* name, Il2CppObject* parent, Root& root)
{
  static auto* go = Class("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto* rt = Class("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  if (!go || !rt)
    return nullptr;
  auto* object = il2cpp_object_new(go);
  root.reset(object);
  void* args[] = {il2cpp_string_new(name)};
  if (!TryInvoke(IL2CppClassHelper(go).GetMethodInfo(".ctor", 1), object, args))
    return nullptr;
  bool active = false;
  if (!Set(object, "SetActive", &active))
    return nullptr;
  auto* transform          = WithType(TypeMethod(go, "AddComponent"), object, rt);
  bool  worldPositionStays = false;
  void* parentArgs[]       = {parent, &worldPositionStays};
  if (!transform
      || !TryInvoke(IL2CppClassHelper(transform->klass).GetMethodInfo("SetParent", 2), transform, parentArgs))
    return nullptr;
  return transform;
}

template <typename T> bool FieldValue(Il2CppObject* object, const char* name, Il2CppTypeEnum expected, T& value)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  auto* type  = field ? il2cpp_field_get_type(field) : nullptr;
  if (!type || type->byref || (il2cpp_field_get_flags(field) & 0x10)) // static field
    return false;
  auto* cls = il2cpp_class_from_type(type);
  if (cls && il2cpp_class_is_enum(cls))
    type = il2cpp_class_enum_basetype(cls);
  if (!type || type->type != expected)
    return false;
  // The API accepts boxed structs and applies their runtime field offsets.
  il2cpp_field_get_value(object, field, &value);
  return true;
}

} // namespace battle_log_ui
