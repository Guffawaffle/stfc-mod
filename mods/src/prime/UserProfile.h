#pragma once

#include <il2cpp/il2cpp_helper.h>

// Digit.PrimeServer.Models.UserProfile (sealed, protobuf IMessage)
//
// Il2CppDumper field layout:
//   _locaId                  : long          @ 0x10
//   _isMuted                 : bool          @ 0x18
//   _avatarOverrideID        : long          @ 0x20
//   _previousMilitaryMight   : MilMightCont  @ 0x28
//   _initialUpdateComplete   : bool          @ 0x30
//   _frame                   : Cosmetic      @ 0x38
//   _avatar                  : Cosmetic      @ 0x40
//   _playerTitleItem         : Cosmetic      @ 0x48
//   _breadcrumbData          : Dict          @ 0x50
//   _entityRef               : EntityRef     @ 0x58
//   <ProfileType>            : enum          @ 0x60
//   _alliance                : AllianceInfo  @ 0x88
//   _unknownFields           : ...           @ 0x98
//   _dirtyFlagsObservable    : ...           @ 0xA0
//   userId_                  : string        @ 0xA8
//   name_                    : string        @ 0xB0
//   level_                   : int           @ 0xB8
//   levelLastAwarded_        : int           @ 0xBC
//   xp_                      : ulong         @ 0xC0
//   allianceId_              : long          @ 0xC8
//   ftueStage_               : int           @ 0xD0
//   militaryMight_           : ulong         @ 0xD8
//   battleRating_            : ulong         @ 0xE0
//   cosmetics_               : MapField      @ 0xE8
//   gameworldId_             : int           @ 0xF0
//   isMentor_                : bool          @ 0xF4
//   maxWarpRange_            : ulong         @ 0xF8
struct UserProfile {
public:
  __declspec(property(get = __get_LocaId)) long LocaId;
  __declspec(property(get = __get_UserId)) Il2CppString* UserId;
  __declspec(property(get = __get_Name)) Il2CppString* Name;
  __declspec(property(get = __get_Level)) int Level;
  __declspec(property(get = __get_AllianceId)) long AllianceId;
  __declspec(property(get = __get_MilitaryMight)) uint64_t MilitaryMight;

  bool get_IsNPC()
  {
    static auto prop = get_class_helper().GetProperty("IsNPC");
    return *prop.Get<bool>(this);
  }

  bool get_IsPlayer()
  {
    static auto prop = get_class_helper().GetProperty("IsPlayer");
    return *prop.Get<bool>(this);
  }

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

  Il2CppString* __get_UserId()
  {
    static auto field = get_class_helper().GetField("userId_").offset();
    return *(Il2CppString**)((char*)this + field);
  }

  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetField("name_").offset();
    return *(Il2CppString**)((char*)this + field);
  }

  int __get_Level()
  {
    static auto field = get_class_helper().GetField("level_").offset();
    return *(int*)((char*)this + field);
  }

  long __get_AllianceId()
  {
    static auto field = get_class_helper().GetField("allianceId_").offset();
    return *(long*)((char*)this + field);
  }

  uint64_t __get_MilitaryMight()
  {
    static auto field = get_class_helper().GetField("militaryMight_").offset();
    return *(uint64_t*)((char*)this + field);
  }
};
