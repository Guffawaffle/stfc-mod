#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>
#include <limits>

// FleetPlayerData.MiningData is IMiningSlot, implemented by both MiningSlot and
// FleetMiningSlot. Property helpers dispatch interface getters on the live object.
struct IMiningSlot {
public:
  __declspec(property(get = __get_ResourceId)) int64_t  ResourceId;
  __declspec(property(get = __get_MiningSpeed)) float   MiningSpeed;
  __declspec(property(get = __get_AmountMined)) double  AmountMined;
  __declspec(property(get = __get_Amount)) int64_t      Amount;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IMiningSlot");
    return class_helper;
  }

public:
  int64_t __get_ResourceId()
  {
    static auto property = get_class_helper().GetProperty("ResourceId");
    auto*       value    = property.Get<int64_t>(this);
    return value ? *value : 0;
  }

  float __get_MiningSpeed()
  {
    static auto property = get_class_helper().GetProperty("MiningSpeed");
    auto*       value    = property.Get<float>(this);
    return value ? *value : 0;
  }

  double __get_AmountMined()
  {
    static auto property = get_class_helper().GetProperty("AmountMined");
    auto*       value    = property.Get<double>(this);
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
  }

  int64_t __get_Amount()
  {
    static auto property = get_class_helper().GetProperty("Amount");
    auto*       value    = property.Get<int64_t>(this);
    return value ? *value : -1;
  }
};
