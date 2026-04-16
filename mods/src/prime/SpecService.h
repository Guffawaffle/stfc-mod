/**
 * @file SpecService.h
 * @brief Game data specification lookup service.
 *
 * Mirrors Digit.PrimeServer.Services.SpecService. Acts as a registry
 * for static game data specifications (ship hulls, etc.). Mod code uses
 * this to resolve numeric IDs into rich spec objects.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "HullSpec.h"

/**
 * @brief Service for looking up static game data specifications.
 *
 * Currently exposes GetHull() to resolve a hull ID to its HullSpec.
 * The service instance is typically obtained from the game's service registry.
 */
struct SpecService {
public:
  HullSpec* GetHull(long hullId)
  {
    static auto method =
        get_class_helper().GetMethod<HullSpec*(SpecService*, long)>("GetHull");
    return method(this, hullId);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SpecService");
    return class_helper;
  }
};
