/**
 * @file NodeAddress.h
 * @brief Basic system-location address wrapper.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct NodeAddress {
public:
  __declspec(property(get = __get_Galaxy)) int64_t   Galaxy;
  __declspec(property(get = __get_System)) int64_t   System;
  __declspec(property(get = __get_Planet)) int64_t   Planet;
  __declspec(property(get = __get_Instance)) int32_t Instance;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "NodeAddress");
    return class_helper;
  }

public:
  int64_t __get_Galaxy()
  {
    static auto prop  = get_class_helper().GetProperty("Galaxy");
    auto*       value = prop.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int64_t __get_System()
  {
    static auto prop  = get_class_helper().GetProperty("System");
    auto*       value = prop.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int64_t __get_Planet()
  {
    static auto prop  = get_class_helper().GetProperty("Planet");
    auto*       value = prop.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int32_t __get_Instance()
  {
    static auto prop  = get_class_helper().GetProperty("Instance");
    auto*       value = prop.Get<int32_t>(this);
    return value ? *value : 0;
  }
};
