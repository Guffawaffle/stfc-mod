/**
 * @file BattleResultHeader.h
 * @brief Battle result data structures and enumerations.
 *
 * Mirrors Digit.PrimeServer.Models.BattleResultHeader and the associated
 * enums (BattleType, BattleResultType, FleetDataType). Used by the battle-
 * log mod to extract combatant ship hull IDs and user profiles from combat
 * reports.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

/** @brief Categorises the type of battle (fleet vs. base, PvP vs. PvE, armada, etc.). */
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

/** @brief Outcome of a battle from the player's perspective. */
enum class BattleResultType {
  Defeat         = 0,
  Victory        = 1,
  PartialVictory = 2
};

/** @brief Identifies the type of fleet data (deployed fleet, starbase, or armada). */
enum class FleetDataType {
  DeployedFleet = 0,
  Starbase      = 1,
  Armada        = 2
};

/**
 * @brief Header data for a single battle result.
 *
 * Contains the player's and enemy's ship hull IDs, user profiles, and
 * a reference to the SpecService for resolving hull details. Accessed
 * from combat report screens and battle-log export functionality.
 */
struct BattleResultHeader {
public:
  __declspec(property(get = __get_PlayerShipHullId)) long PlayerShipHullId;
  __declspec(property(get = __get_EnemyShipHullId)) long EnemyShipHullId;

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

  Il2CppObject* get_SpecService()
  {
    return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(this) + 0x18);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleResultHeader");
    return class_helper;
  }

public:
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
};
