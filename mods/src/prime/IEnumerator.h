#pragma once

#include "il2cpp/method_contract.h"
#include "il2cpp/runtime.h"

class IEnumerator
{
public:
  // Resolve the returned object's method: compiler-generated class suffixes change
  // between clients, and different overloads can return different iterator classes.
  // A successful false result means completion; lookup/invocation failure is separate.
  static bool TryMoveNext(IEnumerator* iterator, bool& has_next)
  {
    if (!iterator)
      return false;
    auto* object = reinterpret_cast<Il2CppObject*>(iterator);
    auto* method = method_contract::Resolve(object->klass, "MoveNext", false, "System.Boolean", {});
    Il2CppObject* result = nullptr;
    return Il2CppRuntime::TryInvoke(method, object, nullptr, &result)
           && Il2CppRuntime::TryBoolean(result, has_next);
  }
};
