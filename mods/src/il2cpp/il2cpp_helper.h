/**
 * @file il2cpp_helper.h
 * @brief High-level C++ wrappers around the IL2CPP reflection API.
 *
 * Provides helper classes that simplify common IL2CPP operations such as
 * looking up classes, calling methods, and reading/writing fields and
 * properties on managed objects. These wrappers are the primary interface
 * used by the prime/ game-object headers to interact with the C# runtime.
 *
 * Key types:
 *  - IL2CppPropertyHelper  -- read/write managed properties via reflection.
 *  - IL2CppFieldHelper      -- resolve field offsets for direct memory access.
 *  - IL2CppStaticFieldHelper-- read static fields.
 *  - IL2CppClassHelper      -- the main workhorse: method lookup, object
 *                              allocation, nested types, parent traversal.
 *  - ObjectFinder<T>        -- locate tracked managed objects by class.
 */
#pragma once

#include "il2cpp-functions.h"
#include "patches/object_tracker_core.h"

#include <il2cpp-api-types.h>
#include <il2cpp-class-internals.h>
#include <il2cpp-config.h>
#include <il2cpp-object-internals.h>
#include <utils/Il2CppHashMap.h>

#include <vector>

#if !_WIN32
#include <syslog.h>
#include <unistd.h>
#endif

/**
 * @brief Wrapper for reading and writing a managed C# property via IL2CPP reflection.
 *
 * Holds a class pointer and PropertyInfo obtained from il2cpp_class_get_property_from_name().
 * Property access goes through virtual dispatch so overridden getters/setters are honoured.
 *
 * @note GetRaw() returns the raw Il2CppObject* (boxed for value types).
 *       Get() additionally unboxes the result, suitable for value types like int/float.
 */
class IL2CppPropertyHelper
{
public:
  IL2CppPropertyHelper(Il2CppClass* cls, const PropertyInfo* propInfo)
  {
    this->cls      = cls;
    this->propInfo = propInfo;
  }

  bool isValidHelper()
  {
#if DEBUG
    return true;
#else
    return this->cls != nullptr && propInfo != nullptr;
#endif
  }

  /**
   * @brief Set the property value via its managed setter (virtual dispatch).
   * @tparam T The value type to pass.
   * @param _this Pointer to the managed object instance.
   * @param v Value to set.
   */
  template <typename T> void SetRaw(void* _this, T& v)
  {
    if (!this->propInfo) {
      return;
    }

    auto set_method         = il2cpp_property_get_set_method((PropertyInfo*)this->propInfo);
    auto set_method_virtual = il2cpp_object_get_virtual_method((Il2CppObject*)_this, set_method);

    Il2CppException* exception = nullptr;
    void*            params[1] = {&v};

    il2cpp_runtime_invoke(set_method_virtual, _this, params, &exception);
  }

  /**
   * @brief Invoke the property getter via virtual dispatch.
   * @tparam T Cast the returned pointer to this type.
   * @param _this Pointer to the managed object instance.
   * @return Pointer to the result (still boxed for value types), or nullptr on failure.
   */
  template <typename T = void> T* GetRaw(void* _this)
  {
    if (!this->propInfo) {
      return nullptr;
    }

    auto get_method         = il2cpp_property_get_get_method((PropertyInfo*)this->propInfo);
    auto get_method_virtual = il2cpp_object_get_virtual_method((Il2CppObject*)_this, get_method);

    Il2CppException* exception = nullptr;
    auto             result    = il2cpp_runtime_invoke(get_method_virtual, _this, nullptr, &exception);

    if (exception) {
      return nullptr;
    }

    return (T*)(result);
  }

  /**
   * @brief Get and unbox a property value (for value types like int, float, enum).
   * @tparam T The unboxed value type.
   * @param _this Pointer to the managed object instance.
   * @return Pointer to the unboxed value, or nullptr on failure.
   */
  template <typename T> T* Get(void* _this)
  {
    auto r = GetRaw<Il2CppObject>(_this);
    return !r ? nullptr : (T*)(il2cpp_object_unbox(r));
  }

  /**
   * @brief Get a property from an already-boxed object, unboxing both self and result.
   * @tparam T The unboxed value type.
   * @param _this Pointer to the boxed managed object.
   * @return Pointer to the unboxed value, or nullptr on failure.
   */
  template <typename T> T* GetUnboxedSelf(void* _this)
  {
    auto r = GetRaw<Il2CppObject>(il2cpp_object_unbox((Il2CppObject*)_this));
    return !r ? nullptr : (T*)(il2cpp_object_unbox(r));
  }

private:
  Il2CppClass*        cls;
  const PropertyInfo* propInfo;
};

/**
 * @brief Wrapper for accessing a managed instance field by its byte offset.
 *
 * Used by the prime/ headers to read/write fields directly in managed object
 * memory via offset arithmetic (e.g. `*(T*)((ptrdiff_t)this + field.offset())`).
 */
class IL2CppFieldHelper
{
public:
  IL2CppFieldHelper(Il2CppClass* cls, FieldInfo* fieldInfo)
  {
    this->cls       = cls;
    this->fieldInfo = fieldInfo;
  }

  bool isValidHelper()
  {
#if DEBUG
    return true;
#else
    return this->cls != nullptr && fieldInfo != nullptr;
#endif
  }

  inline ptrdiff_t offset() const
  {
    return this->fieldInfo->offset;
  }

private:
  Il2CppClass* cls;
  FieldInfo*   fieldInfo;
};

/**
 * @brief Wrapper for reading managed static fields.
 *
 * Static fields live in class-level storage rather than on an instance, so
 * they are accessed via il2cpp_field_static_get_value() instead of offsets.
 */
class IL2CppStaticFieldHelper
{
public:
  IL2CppStaticFieldHelper(Il2CppClass* cls, FieldInfo* fieldInfo)
  {
    this->cls       = cls;
    this->fieldInfo = fieldInfo;
  }

  template <typename T> inline T Get() const
  {
    T v;
    il2cpp_field_static_get_value(this->fieldInfo, &v);
    return v;
  }

private:
  Il2CppClass* cls;
  FieldInfo*   fieldInfo;
};

/**
 * @brief Central helper for IL2CPP class reflection operations.
 *
 * Wraps an Il2CppClass* and provides methods for:
 *  - Allocating managed objects (New).
 *  - Looking up methods by name and argument count, returning native function pointers.
 *  - Virtual method resolution.
 *  - Custom method searches with parameter-type filters (GetMethodSpecial).
 *  - Property, field, and static field access (returns the corresponding helper).
 *  - Parent class and nested type traversal.
 *
 * Most prime/ struct headers obtain one of these via il2cpp_get_class_helper()
 * and cache it in a static local.
 */
class IL2CppClassHelper
{
public:
  IL2CppClassHelper(Il2CppClass* cls)
  {
    this->cls = cls;
  }

  /**
   * @brief Allocate a new managed object of this class.
   * @tparam T Native type to cast the result to.
   * @return Pointer to the newly allocated (uninitialized) managed object.
   */
  template <typename T> T* New()
  {
    return (T*)il2cpp_object_new(this->cls);
  }

  /**
   * @brief Get the System.Type object for this class.
   * @return Pointer to the managed System.Type (Il2CppReflectionType*).
   */
  void* GetType()
  {
    auto obj = il2cpp_type_get_object(&this->cls->byval_arg);
    return obj;
  }

  bool isValidHelper()
  {
#if DEBUG
    return true;
#else
    return this->cls != nullptr;
#endif
  }

  /**
   * @brief Look up a method by name and optional argument count.
   * @tparam T Cast the native function pointer to this type.
   * @param name Method name in the C# class.
   * @param arg_count Expected parameter count, or -1 to match any.
   * @return Native function pointer, or nullptr if not found.
   */
  template <typename T = void> T* GetMethod(const char* name, int arg_count = -1)
  {
    if (!this->cls) {
      return nullptr;
    }

    auto fn = il2cpp_class_get_method_from_name(this->cls, name, arg_count);
    if (fn != nullptr) {
      return (T*)fn->methodPointer;
    }

    return nullptr;
  }

  /**
   * @brief Look up a method and resolve it through virtual dispatch.
   * @tparam T Cast the native function pointer to this type.
   * @param name Method name.
   * @param arg_count Expected parameter count, or -1 to match any.
   * @return Native function pointer to the virtual implementation.
   * @note Uses `this` as the Il2CppObject for vtable lookup, which means
   *       the IL2CppClassHelper instance itself must point at a valid object.
   */
  template <typename T = void> T* GetVirtualMethod(const char* name, int arg_count = -1)
  {
    if (!this->cls) {
      return nullptr;
    }

    auto fn = il2cpp_class_get_method_from_name(this->cls, name, arg_count);

    auto get_method_virtual = il2cpp_object_get_virtual_method((Il2CppObject*)this, fn);

    return (T*)get_method_virtual->methodPointer;
  }

  /**
   * @brief Helper for calling methods through il2cpp_runtime_invoke().
   *
   * Unlike the direct function-pointer approach, this goes through the
   * managed invocation path, which handles boxing/unboxing and exceptions.
   *
   * @tparam R Return type (value type; will be unboxed).
   * @tparam Args Parameter types.
   */
  template <typename R, typename... Args> class InvokerMethod
  {

  public:
    InvokerMethod(const MethodInfo* fn)
        : fn(fn)
    {
    }

    R Invoke(void* _this, Args... args)
    {
      Il2CppException* exception = nullptr;
      void*            params    = {};
      auto             result    = il2cpp_runtime_invoke(this->fn, _this, &params, &exception);

      return *(R*)il2cpp_object_unbox(result);
    }

    const MethodInfo* fn;
  };

  template <typename R, typename... Args>
  const InvokerMethod<R, Args...> GetInvokeMethod(const char* name, int arg_count = -1)
  {
    if (!this->cls) {
      return nullptr;
    }

    auto fn = il2cpp_class_get_method_from_name(this->cls, name, arg_count);

    return InvokerMethod<R, Args...>(fn);
  }

  /**
   * @brief Find a method using a custom parameter-type filter, returning its MethodInfo.
   *
   * Iterates all methods on the class and applies @p arg_filter to let the
   * caller disambiguate overloads by inspecting parameter Il2CppTypes.
   *
   * @param name Method name.
   * @param arg_filter Predicate receiving (param_count, params[]). Return true to accept.
   * @return The first matching MethodInfo*, or nullptr.
   */
  const MethodInfo*
  GetMethodInfoSpecial(const char*                                                    name,
                       std::function<bool(int param_count, const Il2CppType** param)> arg_filter = nullptr)
  {
    if (!this->cls) {
      return nullptr;
    }

    auto  flags = 0;
    void* iter  = NULL;
    while (const MethodInfo* method = il2cpp_class_get_methods(this->cls, &iter)) {
      if (method->name[0] == name[0] && !strcmp(name, method->name) && ((method->flags & flags) == flags)) {
        if (!arg_filter || arg_filter(method->parameters_count, method->parameters)) {
          return method;
        }
      }
    }
    return nullptr;
  }

  /**
   * @brief Like GetMethodInfoSpecial but returns a cast native function pointer.
   * @tparam T Function pointer type to cast to.
   * @param name Method name.
   * @param arg_filter Optional predicate for parameter disambiguation.
   * @return Native function pointer, or nullptr.
   */
  template <typename T = void>
  T* GetMethodSpecial(const char*                                                    name,
                      std::function<bool(int param_count, const Il2CppType** param)> arg_filter = nullptr)
  {
    if (!this->cls) {
      return nullptr;
    }
    auto info = GetMethodInfoSpecial(name, arg_filter);
    if (info) {
      return (T*)info->methodPointer;
    }
    return nullptr;
  }

  template <typename T = void> T* GetMethodSpecial2(Il2CppObject* obj, const char* name)
  {
    if (!this->cls) {
      return nullptr;
    }

    for (auto i = 0; i < obj->klass->method_count; ++i) {
      auto method = obj->klass->klass->methods[i];
      if (method->name[0] == name[0] && !strcmp(name, method->name)) {
        return (T*)method->methodPointer;
      }
    }
    return nullptr;
  }

  const MethodInfo* GetMethodInfo(const char* name, int arg_count = -1)
  {
    if (!this->cls) {
      return nullptr;
    }

    return il2cpp_class_get_method_from_name(this->cls, name, arg_count);
  }

  inline IL2CppPropertyHelper GetProperty(const char* name)
  {
    return IL2CppPropertyHelper{this->cls, il2cpp_class_get_property_from_name(this->cls, name)};
  }

  inline IL2CppFieldHelper GetField(const char* name)
  {
    return IL2CppFieldHelper{this->cls, il2cpp_class_get_field_from_name(this->cls, name)};
  }

  inline IL2CppStaticFieldHelper GetStaticField(const char* name)
  {
    return IL2CppStaticFieldHelper{this->cls, il2cpp_class_get_field_from_name(this->cls, name)};
  }

  /**
   * @brief Walk the class hierarchy upward to find a parent class by name.
   * @param name Simple class name to search for.
   * @return Helper wrapping the parent class, or a null-class helper if not found.
   */
  inline IL2CppClassHelper GetParent(const char* name)
  {
    Il2CppClass* pcls = this->cls;
    if (pcls) {
      do {
        if (pcls->name[0] == name[0] && !strcmp(name, pcls->name)) {
          return IL2CppClassHelper{pcls};
        }

        pcls = il2cpp_class_get_parent(pcls);
      } while (pcls);
    }
    return IL2CppClassHelper{nullptr};
  }

  /**
   * @brief Find a nested type (inner class) by name.
   * @param name Simple name of the nested type.
   * @return Helper wrapping the nested type, or a null-class helper if not found.
   */
  inline IL2CppClassHelper GetNestedType(const char* name)
  {
    for (int i = 0; i < this->cls->nested_type_count; ++i) {
      auto type = this->cls->nestedTypes[i];
      if (strcmp(type->name, name) == 0) {
        return type;
      }
    }

    return nullptr;
  }

  Il2CppClass* get_cls()
  {
    return this->cls;
  }

private:
  Il2CppClass* cls;
};

/**
 * @brief Convenience macro that resolves to il2cpp_get_class_helper_impl().
 */
#define il2cpp_get_class_helper(assembly, namespacez, name) il2cpp_get_class_helper_impl(assembly, namespacez, name)

/**
 * @brief Resolve a managed class by assembly, namespace, and name.
 *
 * Opens the assembly image via the IL2CPP domain and looks up the class.
 * The result is typically cached in a static local by each prime/ header.
 *
 * @param assembly  Assembly name (e.g. "Assembly-CSharp").
 * @param namespacez C# namespace (e.g. "Digit.Client.Core").
 * @param name      Simple class name (e.g. "Hub").
 * @return IL2CppClassHelper wrapping the resolved Il2CppClass*.
 */
inline IL2CppClassHelper il2cpp_get_class_helper_impl(const char* assembly, const char* namespacez, const char* name)
{
  auto domain    = il2cpp_domain_get();
  auto assemblyT = il2cpp_domain_assembly_open(domain, assembly);
  auto image     = il2cpp_assembly_get_image(assemblyT);

  auto cls = il2cpp_class_from_name(image, namespacez, name);

  return IL2CppClassHelper{cls};
}

/**
 * @brief Access an element of an Il2CppArray by index with a typed cast.
 * @tparam T Element type.
 * @param array Pointer to the managed array.
 * @param index Zero-based element index.
 * @return Pointer to the element cast to T*.
 */
template <typename T> inline T* il2cpp_get_array_element(Il2CppArray* array, size_t index)
{
  Il2CppArraySize* n = (Il2CppArraySize*)(array);
  return (T*)n->vector[index];
}

/** @brief Global tracked-object core, keyed by Il2CppClass*. */
extern ObjectTrackerCore<Il2CppClass*, uintptr_t> tracked_objects;

/**
 * @brief Look up live managed objects of a given type from the global tracked_objects map.
 *
 * The game's object-tracking hooks populate tracked_objects whenever a managed
 * object of interest is constructed or destroyed. ObjectFinder provides typed
 * access into that map.
 *
 * @tparam T A prime/ struct type that exposes a static get_class_helper() method.
 */
template <typename T> class ObjectFinder
{
public:
  static T* Get()
  {
    return reinterpret_cast<T*>(tracked_objects.latest_for_class(T::get_class_helper().get_cls()));
  }

  static std::vector<T*> GetAll()
  {
    auto objects = tracked_objects.objects_for_class(T::get_class_helper().get_cls());

    std::vector<T*> typed_objects;
    typed_objects.reserve(objects.size());
    for (const auto object : objects) {
      typed_objects.push_back(reinterpret_cast<T*>(object));
    }
    return typed_objects;
  }
};

/**
 * @brief Typed wrapper around il2cpp_resolve_icall() for internal calls.
 * @tparam T Function pointer type.
 * @param name Fully qualified internal call name (e.g. "UnityEngine.Screen::get_width").
 * @return Native function pointer cast to T*, or nullptr if unresolved.
 */
template <typename T> T* il2cpp_resolve_icall_typed(const char* name)
{
  return (T*)il2cpp_resolve_icall(name);
}
