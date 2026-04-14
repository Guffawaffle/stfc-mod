#pragma once

#include <il2cpp/il2cpp_helper.h>

enum class HullType {
  Any          = -1,
  Destroyer    = 0,
  Survey       = 1,
  Explorer     = 2,
  Battleship   = 3,
  Defense      = 4,
  ArmadaTarget = 5
};

// Digit.PrimeServer.Models.Faction
enum class Faction {
  None      = -1,
  Undefined = 0,
  Federation = 1,
  Klingon    = 2,
  Romulan    = 3
};

// Digit.PrimeServer.Models.Rarity
enum class Rarity {
  Base      = 0,
  Common    = 1,
  Uncommon  = 2,
  Rare      = 3,
  Epic      = 4,
  Legendary = 5
};

// Digit.PrimeServer.Models.FleetArmadaCategory
enum class FleetArmadaCategory {
  Regular        = 0,
  Solo           = 1,
  SoloBoss       = 2,
  SquadArmada    = 3,
  SoloOutpost    = 4,
  CrossAlliance  = 5
};

// Digit.PrimeServer.Models.HullSpec (protobuf IMessage)
//
// Il2CppDumper field layout:
//   id_            : long                @ 0x58
//   idStr_         : string              @ 0x60
//   name_          : string              @ 0x68
//   type_          : HullType            @ 0x70
//   faction_       : Faction             @ 0x74  [obsolete]
//   grade_         : int                 @ 0x78
//   rarity_        : Rarity              @ 0xE8
//   factionId_     : long                @ 0xF0
//   category_      : FleetArmadaCategory @ 0xF8
//   sortOrder_     : int                 @ 0xD4
//   initiative_    : int                 @ 0xD0
//   tierMax_       : int                 @ 0xD8
//   craftingId_    : string              @ 0xE0
//   generation_    : int                 @ 0x12C
struct HullSpec {
public:
  __declspec(property(get = __get_Id)) long Id;
  __declspec(property(get = __get_IdStr)) Il2CppString* IdStr;
  __declspec(property(get = __get_Name)) Il2CppString* Name;
  __declspec(property(get = __get_Type)) HullType Type;
  __declspec(property(get = __get_Faction)) Faction HullFaction;
  __declspec(property(get = __get_Grade)) int Grade;
  __declspec(property(get = __get_Rarity)) Rarity HullRarity;
  __declspec(property(get = __get_FactionId)) long FactionId;
  __declspec(property(get = __get_Category)) FleetArmadaCategory Category;
  __declspec(property(get = __get_SortOrder)) int SortOrder;
  __declspec(property(get = __get_TierMax)) int TierMax;

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

  Il2CppString* __get_IdStr()
  {
    static auto field = get_class_helper().GetField("idStr_").offset();
    return *(Il2CppString**)((char*)this + field);
  }

  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetField("name_").offset();
    return *(Il2CppString**)((char*)this + field);
  }

  HullType __get_Type()
  {
    static auto field = get_class_helper().GetProperty("Type");
    return *field.Get<HullType>(this);
  }

  Faction __get_Faction()
  {
    static auto field = get_class_helper().GetField("faction_").offset();
    return *(Faction*)((char*)this + field);
  }

  int __get_Grade()
  {
    static auto field = get_class_helper().GetField("grade_").offset();
    return *(int*)((char*)this + field);
  }

  Rarity __get_Rarity()
  {
    static auto field = get_class_helper().GetField("rarity_").offset();
    return *(Rarity*)((char*)this + field);
  }

  long __get_FactionId()
  {
    static auto field = get_class_helper().GetField("factionId_").offset();
    return *(long*)((char*)this + field);
  }

  FleetArmadaCategory __get_Category()
  {
    static auto field = get_class_helper().GetField("category_").offset();
    return *(FleetArmadaCategory*)((char*)this + field);
  }

  int __get_SortOrder()
  {
    static auto field = get_class_helper().GetField("sortOrder_").offset();
    return *(int*)((char*)this + field);
  }

  int __get_TierMax()
  {
    static auto field = get_class_helper().GetField("tierMax_").offset();
    return *(int*)((char*)this + field);
  }
};
