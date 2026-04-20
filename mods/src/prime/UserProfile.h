/**
 * @file UserProfile.h
 * @brief Player profile data.
 *
 * Mirrors Digit.PrimeServer.Models.UserProfile. Provides the player's
 * location ID and display name, used in battle reports and UI displays.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

/**
 * @brief Profile data for a game player.
 *
 * Contains the player's server-side location ID and their display name.
 * Typically retrieved from BattleResultHeader or other combat data.
 */
struct UserProfile {
public:
  __declspec(property(get = __get_LocaId)) long LocaId;
  __declspec(property(get = __get_Name)) Il2CppString* Name;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "UserProfile");
    return class_helper;
  }

public:
  long __get_LocaId()
  {
    static auto field = get_class_helper().GetField("_locaId").offset();
    return *(long*)((char*)this + field);
  }

  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetField("name_").offset();
    return *(Il2CppString**)((char*)this + field);
  }
};
