#pragma once

#include "IdRefs.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct ForbiddenTechSpec {
public:
  __declspec(property(get = __get_Id)) int64_t Id;
  __declspec(property(get = __get_IdRefs)) IdRefs* IdRefsValue;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ForbiddenTechSpec");
    return class_helper;
  }

public:
  int64_t __get_Id()
  {
    static auto prop = get_class_helper().GetProperty("Id");
    return *prop.Get<int64_t>(this);
  }

  IdRefs* __get_IdRefs()
  {
    static auto prop = get_class_helper().GetProperty("IdRefs");
    return prop.GetRaw<IdRefs>(this);
  }
};