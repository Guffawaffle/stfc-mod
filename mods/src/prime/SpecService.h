#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "HullSpec.h"

// Digit.PrimeServer.Services.SpecService : GSService
//
// Il2CppDumper field layout (selected):
//   _resourceService       : ResourcesService   @ 0x78
//   _userProfileService    : UserProfileService  @ 0x80
//   _dataContainer         : StaticSyncData...   @ 0x88
//
// Key method: GetHull(long hullId) → HullSpec*
//   NOTE: The game uses "GetHull" not "GetHullSpec".
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
