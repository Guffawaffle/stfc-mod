#pragma once

#include <il2cpp/il2cpp_helper.h>

// Digit.PrimeServer.Models.BattleType
enum class BattleType {
  Fleet                          = 0,
  Base                           = 1,
  PassiveMarauder                = 2,
  NpcInstantiated                = 3,
  DockingPoint                   = 4,
  ActiveMarauder_MarauderInit    = 5,
  ActiveMarauder_PlayerInit      = 6,
  ArmadaBase                     = 7,
  ArmadaMarauder                 = 8,
  PveDockingPoint                = 9,
  ArmadaAsb                      = 10,
  ArmadaMta                      = 11,
  Hazard                         = 12,
  PveCuttingBeam                 = 13,
  PvpCuttingBeam                 = 14,
  PveChainShot                   = 15,
  PvpChainShot                   = 16
};

// Digit.PrimeServer.Models.BattleResultType
enum class BattleResultType {
  Defeat         = 0,
  Victory        = 1,
  PartialVictory = 2
};

// Digit.PrimeServer.Models.FleetDataType
enum class FleetDataType {
  DeployedFleet = 0,
  Starbase      = 1,
  Armada        = 2
};

// Digit.PrimeServer.Models.BattleResultFlags
namespace BattleResultFlags {
  inline constexpr int HiddenPlayer        = 2;
  inline constexpr int HiddenAlliance      = 4;
  inline constexpr int InitiatorSupported  = 8;
  inline constexpr int TargetSupported     = 16;
  inline constexpr int InitiatorDisrupted  = 32;
  inline constexpr int TargetDisrupted     = 64;
  inline constexpr int MultiTargetArmada   = 128;
}

// Digit.PrimeServer.Models.BattleResultHeader : IBattleResultHeader
//
// Il2CppDumper field layout:
//   _userProfileService     : UserProfileService      @ 0x10
//   _specService            : SpecService              @ 0x18
//   _deploymentService      : DeploymentService        @ 0x20
//   _allianceServerService  : AllianceServerService    @ 0x28
//   _journalHeader          : JournalHeader            @ 0x30
//   _isNew                  : bool                     @ 0x38
//   _playerFleetSummary     : JournalFleetSummary      @ 0x40
//   _enemyFleetSummary      : JournalFleetSummary      @ 0x48
//   _battleTimeContext      : TimerDataContext          @ 0x50
//   _userChestId            : long                     @ 0x58
//   _userChestMtaId         : long                     @ 0x60
//   _isOutgoingAssault      : bool                     @ 0x68
//   _targetAllianceId       : long                     @ 0x70
struct BattleResultHeader {
public:
  __declspec(property(get = __get_ID)) long ID;
  __declspec(property(get = __get_BattleType)) BattleType Battle_Type;
  __declspec(property(get = __get_BattleResultType)) BattleResultType ResultType;
  __declspec(property(get = __get_Flags)) int Flags;
  __declspec(property(get = __get_IsPlayerInitiator)) bool IsPlayerInitiator;
  __declspec(property(get = __get_IsPlayerWinner)) bool IsPlayerWinner;
  __declspec(property(get = __get_IsPartialWin)) bool IsPartialWin;
  __declspec(property(get = __get_IsPlayerStarbase)) bool IsPlayerStarbase;
  __declspec(property(get = __get_IsEnemyStarbase)) bool IsEnemyStarbase;
  __declspec(property(get = __get_IsArmadaBattle)) bool IsArmadaBattle;
  __declspec(property(get = __get_IsPlayerVersusPlayer)) bool IsPlayerVersusPlayer;
  __declspec(property(get = __get_PlayerShipHullId)) long PlayerShipHullId;
  __declspec(property(get = __get_EnemyShipHullId)) long EnemyShipHullId;
  __declspec(property(get = __get_SystemId)) long SystemId;

  // Reference-type property accessors (return Il2CppObject* for UserProfile, etc.)
  Il2CppObject* get_PlayerUserProfile()
  {
    static auto prop = get_class_helper().GetProperty("PlayerUserProfile");
    return prop.GetRaw<Il2CppObject>(this);
  }

  Il2CppObject* get_EnemyUserProfile()
  {
    static auto prop = get_class_helper().GetProperty("EnemyUserProfile");
    return prop.GetRaw<Il2CppObject>(this);
  }

  // Direct field access: _specService at offset 0x18
  Il2CppObject* get_SpecService()
  {
    return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(this) + 0x18);
  }

  // Direct field access: _journalHeader at offset 0x30
  Il2CppObject* get_JournalHeader()
  {
    return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(this) + 0x30);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleResultHeader");
    return class_helper;
  }

public:
  long __get_ID()
  {
    static auto prop = get_class_helper().GetProperty("ID");
    return *prop.Get<long>(this);
  }

  BattleType __get_BattleType()
  {
    static auto prop = get_class_helper().GetProperty("BattleType");
    return *prop.Get<BattleType>(this);
  }

  BattleResultType __get_BattleResultType()
  {
    static auto prop = get_class_helper().GetProperty("BattleResultType");
    return *prop.Get<BattleResultType>(this);
  }

  int __get_Flags()
  {
    static auto prop = get_class_helper().GetProperty("Flags");
    return *prop.Get<int>(this);
  }

  bool __get_IsPlayerInitiator()
  {
    static auto prop = get_class_helper().GetProperty("IsPlayerInitiator");
    return *prop.Get<bool>(this);
  }

  bool __get_IsPlayerWinner()
  {
    static auto prop = get_class_helper().GetProperty("IsPlayerWinner");
    return *prop.Get<bool>(this);
  }

  bool __get_IsPartialWin()
  {
    static auto prop = get_class_helper().GetProperty("IsPartialWin");
    return *prop.Get<bool>(this);
  }

  bool __get_IsPlayerStarbase()
  {
    static auto prop = get_class_helper().GetProperty("IsPlayerStarbase");
    return *prop.Get<bool>(this);
  }

  bool __get_IsEnemyStarbase()
  {
    static auto prop = get_class_helper().GetProperty("IsEnemyStarbase");
    return *prop.Get<bool>(this);
  }

  bool __get_IsArmadaBattle()
  {
    static auto prop = get_class_helper().GetProperty("IsArmadaBattle");
    return *prop.Get<bool>(this);
  }

  bool __get_IsPlayerVersusPlayer()
  {
    static auto prop = get_class_helper().GetProperty("IsPlayerVersusPlayer");
    return *prop.Get<bool>(this);
  }

  long __get_PlayerShipHullId()
  {
    static auto prop = get_class_helper().GetProperty("PlayerShipHullId");
    return *prop.Get<long>(this);
  }

  long __get_EnemyShipHullId()
  {
    static auto prop = get_class_helper().GetProperty("EnemyShipHullId");
    return *prop.Get<long>(this);
  }

  long __get_SystemId()
  {
    static auto prop = get_class_helper().GetProperty("SystemId");
    return *prop.Get<long>(this);
  }
};
