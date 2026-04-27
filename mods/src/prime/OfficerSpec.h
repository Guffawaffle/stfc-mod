#pragma once

#include "IdRefs.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct OfficerSpec {
public:
  __declspec(property(get = __get_Id)) int64_t Id;
  __declspec(property(get = __get_IdRefs)) IdRefs* IdRefsValue;
  __declspec(property(get = __get_CaptainManeuverId)) int64_t CaptainManeuverId;
  __declspec(property(get = __get_OfficerAbilityId)) int64_t OfficerAbilityId;
  __declspec(property(get = __get_BelowDecksAbilityId)) int64_t BelowDecksAbilityId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "OfficerSpec");
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

  int64_t __get_CaptainManeuverId()
  {
    static auto prop = get_class_helper().GetProperty("CaptainManeuverId");
    return *prop.Get<int64_t>(this);
  }

  int64_t __get_OfficerAbilityId()
  {
    static auto prop = get_class_helper().GetProperty("OfficerAbilityId");
    return *prop.Get<int64_t>(this);
  }

  int64_t __get_BelowDecksAbilityId()
  {
    static auto prop = get_class_helper().GetProperty("BelowDecksAbilityId");
    return *prop.Get<int64_t>(this);
  }
};