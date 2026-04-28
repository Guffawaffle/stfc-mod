#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct IdRefs {
public:
  __declspec(property(get = __get_LocaId)) int64_t LocaId;
  __declspec(property(get = __get_LocaStringId)) Il2CppString* LocaStringId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IdRefs");
    return class_helper;
  }

public:
  int64_t __get_LocaId()
  {
    static auto prop = get_class_helper().GetProperty("LocaId");
    return *prop.Get<int64_t>(this);
  }

  Il2CppString* __get_LocaStringId()
  {
    static auto prop = get_class_helper().GetProperty("LocaStringId");
    return prop.GetRaw<Il2CppString>(this);
  }
};