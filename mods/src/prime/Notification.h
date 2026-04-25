#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

#include "NotificationIncomingFleetParams.h"

struct Notification {
public:
  __declspec(property(get = __get_ProducerType)) int ProducerType;
  __declspec(property(get = __get_IncomingFleetParams)) NotificationIncomingFleetParams* IncomingFleetParams;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Notification");
    return class_helper;
  }

public:
  int __get_ProducerType()
  {
    static auto prop = get_class_helper().GetProperty("ProducerType");
    return *prop.Get<int>(this);
  }

  NotificationIncomingFleetParams* __get_IncomingFleetParams()
  {
    static auto prop = get_class_helper().GetProperty("IncomingFleetParams");
    return prop.GetRaw<NotificationIncomingFleetParams>(this);
  }
};