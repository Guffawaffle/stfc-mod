#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

#include "QuickScanFleetData.h"

enum class NotificationIncomingAttackTargetType {
  None         = 0,
  Fleet        = 1,
  DockingPoint = 2,
  Station      = 3,
};

struct NotificationIncomingFleetParams {
public:
  __declspec(property(get = __get_TargetType)) NotificationIncomingAttackTargetType TargetType;
  __declspec(property(get = __get_QuickScanResult)) QuickScanFleetData* QuickScanResult;
  __declspec(property(get = __get_TargetUserId)) Il2CppString* TargetUserId;
  __declspec(property(get = __get_TargetFleetId)) int64_t TargetFleetId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = []() {
      auto notification_helper =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Notification");
      if (!notification_helper.isValidHelper()) {
        return IL2CppClassHelper{nullptr};
      }

      auto types_helper = notification_helper.GetNestedType("Types");
      if (!types_helper.isValidHelper()) {
        return IL2CppClassHelper{nullptr};
      }

      return types_helper.GetNestedType("IncomingFleetParams");
    }();
    return class_helper;
  }

public:
  NotificationIncomingAttackTargetType __get_TargetType()
  {
    static auto prop = get_class_helper().GetProperty("TargetType");
    return *prop.Get<NotificationIncomingAttackTargetType>(this);
  }

  QuickScanFleetData* __get_QuickScanResult()
  {
    static auto prop = get_class_helper().GetProperty("QuickScanResult");
    return prop.GetRaw<QuickScanFleetData>(this);
  }

  Il2CppString* __get_TargetUserId()
  {
    static auto prop = get_class_helper().GetProperty("TargetUserId");
    return prop.GetRaw<Il2CppString>(this);
  }

  int64_t __get_TargetFleetId()
  {
    static auto prop = get_class_helper().GetProperty("TargetFleetId");
    return *prop.Get<int64_t>(this);
  }
};