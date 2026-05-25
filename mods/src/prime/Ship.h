#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

struct Ship {
public:
  __declspec(property(get = __get_ID)) int64_t ID;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Ship");
    return class_helper;
  }

public:
  int64_t __get_ID()
  {
    static auto field = get_class_helper().GetProperty("ID");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }
};