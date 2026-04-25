#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

#include "QuickScanFleetData.h"
#include "NotificationIncomingFleetParams.h"

struct IncomingFleetParamsJson {
public:
  __declspec(property(get = __get_QuickScanResult)) QuickScanFleetData* QuickScanResult;
  __declspec(property(get = __get_TargetType)) NotificationIncomingAttackTargetType TargetType;
  __declspec(property(get = __get_TargetFleetId)) int64_t TargetFleetId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IncomingFleetParamsJson");
    return class_helper;
  }

public:
  QuickScanFleetData* __get_QuickScanResult()
  {
    static auto prop = get_class_helper().GetProperty("QuickScanResult");
    return prop.GetRaw<QuickScanFleetData>(this);
  }

  NotificationIncomingAttackTargetType __get_TargetType()
  {
    static auto prop = get_class_helper().GetProperty("TargetType");
    return *prop.Get<NotificationIncomingAttackTargetType>(this);
  }

  int64_t __get_TargetFleetId()
  {
    static auto prop = get_class_helper().GetProperty("TargetFleetId");
    return *prop.Get<int64_t>(this);
  }
};