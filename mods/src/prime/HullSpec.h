/**
 * @file HullSpec.h
 * @brief Ship hull specification data.
 *
 * Mirrors Digit.PrimeServer.Models.HullSpec. Each ship type in the game
 * has a HullSpec describing its ID, display name, and hull classification
 * (destroyer, survey, explorer, etc.).
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

/** @brief Ship hull classification. */
enum class HullType {
  Any          = -1,
  Destroyer    = 0,
  Survey       = 1,
  Explorer     = 2,
  Battleship   = 3,
  Defense      = 4,
  ArmadaTarget = 5
};

/**
 * @brief Specification data for a single ship hull type.
 *
 * Provides the hull's numeric ID (matches server data), localised display
 * name, and classification type. Obtained via SpecService::GetHull().
 */
struct HullSpec {
public:
  __declspec(property(get = __get_Id)) long Id;
  __declspec(property(get = __get_Name)) Il2CppString* Name;
  __declspec(property(get = __get_Type)) HullType Type;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "HullSpec");
    return class_helper;
  }

public:
  long __get_Id()
  {
    static auto field = get_class_helper().GetField("id_").offset();
    return *(long*)((char*)this + field);
  }

  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetField("name_").offset();
    return *(Il2CppString**)((char*)this + field);
  }

  HullType __get_Type()
  {
    static auto prop = get_class_helper().GetProperty("Type");
    return *prop.Get<HullType>(this);
  }
};
