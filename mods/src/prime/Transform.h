/**
 * @file Transform.h
 * @brief Unity Transform wrapper.
 *
 * Mirrors UnityEngine.Transform. Exposes the localScale property for
 * reading and writing via IL2CPP reflection.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "GameObject.h"
#include "Vector3.h"

/** @brief Wrapper for Unity's Transform component (position, rotation, scale). */
struct Transform {
  __declspec(property(get = __get_childCount)) int childCount;
  __declspec(property(get = __get_gameObject)) GameObject* gameObject;
  __declspec(property(get = __get_LocalScale, put = __set_LocalScale)) Vector3* localScale;

  int __get_childCount()
  {
    static auto field = get_class_helper().GetProperty("childCount");
    return *field.Get<int>(this);
  }

  GameObject* __get_gameObject()
  {
    static auto field = get_class_helper().GetProperty("gameObject");
    return field.GetRaw<GameObject>(this);
  }

  Transform* GetChild(int index)
  {
    static auto method = get_class_helper().GetMethod<Transform*(Transform*, int)>("GetChild");
    return method(this, index);
  }

  Vector3* __get_LocalScale()
  {
    static auto field = get_class_helper().GetProperty("localScale");
    return field.Get<Vector3>(this);
  }

  void __set_LocalScale(Vector3* v)
  {
    static auto prop = get_class_helper().GetProperty("localScale");
    return prop.SetRaw((void*)this, *v);
  }


private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    return class_helper;
  }
};
