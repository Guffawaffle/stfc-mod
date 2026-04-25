#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

struct QuickScanFleetData {
public:
  __declspec(property(get = __get_TargetId)) Il2CppString* TargetId;
  __declspec(property(get = __get_TargetFleetId)) int64_t TargetFleetId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "QuickScanFleetData");
    return class_helper;
  }

public:
  Il2CppString* __get_TargetId()
  {
    static auto prop = get_class_helper().GetProperty("TargetId");
    return prop.GetRaw<Il2CppString>(this);
  }

  int64_t __get_TargetFleetId()
  {
    static auto prop = get_class_helper().GetProperty("TargetFleetId");
    return *prop.Get<int64_t>(this);
  }
};