/**
 * @file il2cpp_helper.cc
 * @brief Implementation helpers for IL2CPP managed-object access.
 */
#include "il2cpp_helper.h"

/**
 * @brief Resolve a GC handle to its managed object pointer.
 * @param handle IL2CPP garbage-collector handle.
 * @return Pointer to the managed Il2CppObject, or nullptr if the handle is invalid.
 */
Il2CppObject* get_target(Il2CppGCHandle handle)
{
  return il2cpp_gchandle_get_target(handle);
}
