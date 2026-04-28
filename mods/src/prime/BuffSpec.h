#pragma once

#include "IdRefs.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct BuffSpec {
public:
  __declspec(property(get = __get_BuffId)) int64_t BuffId;
  __declspec(property(get = __get_IdRefs)) IdRefs* IdRefsValue;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BuffSpec");
    return class_helper;
  }

public:
  int64_t __get_BuffId()
  {
    static auto prop = get_class_helper().GetProperty("BuffId");
    return *prop.Get<int64_t>(this);
  }

  IdRefs* __get_IdRefs()
  {
    static auto prop = get_class_helper().GetProperty("IdRefs");
    return prop.GetRaw<IdRefs>(this);
  }
};