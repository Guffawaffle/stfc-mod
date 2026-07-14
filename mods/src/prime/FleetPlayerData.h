#pragma once

#include "BattleTargetData.h"
#include "HullSpec.h"
#include "MiningSlot.h"
#include "Ship.h"

#include <cstdint>

enum class FleetState {
  Unknown      = 0,
  IdleInSpace  = 1,
  Docked       = 2,
  Mining       = 4,
  Destroyed    = 8,
  TieringUp    = 16,
  CanReplaceOfficers = 18,
  Repairing    = 32,
  CannotLaunch = 56,
  Battling     = 64,
  WarpCharging = 128,
  Warping      = 256,
  CanRemove    = 384,
  Impulsing    = 512,
  Capturing    = 1024,
  AutoHunting  = 2048,
  CannotMove   = 2552,
  CanManage    = 2947,
  CanBeTargetedByAbility = 3589,
  CanEngage    = 3591,
  Outposting   = 4096,
  CanRecall    = 5637,
  Deployed     = 8133,
  CanLocate    = 8135
};
    
struct FleetPlayerData {
public:
  __declspec(property(get = __get_CurrentState)) FleetState CurrentState;
  __declspec(property(get = __get_PreviousState)) FleetState PreviousState;
  __declspec(property(get = __get_Id)) uint64_t Id;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_Ship)) ::Ship* Ship;
  __declspec(property(get = __get_MiningData)) MiningSlot* MiningData;
  __declspec(property(get = __get_CargoResourceFillLevel)) float CargoResourceFillLevel;
  __declspec(property(get = __get_Address)) void* Address;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
    return class_helper;
  }

public:
  HullSpec* __get_Hull()
  {
    static auto field = get_class_helper().GetProperty("Hull");
    return field.GetRaw<HullSpec>(this);
  }

  ::Ship* __get_Ship()
  {
    static auto field = get_class_helper().GetProperty("Ship");
    return field.GetRaw<::Ship>(this);
  }

  MiningSlot* __get_MiningData()
  {
    static auto field = get_class_helper().GetProperty("MiningData");
    return field.GetRaw<MiningSlot>(this);
  }

  float __get_CargoResourceFillLevel()
  {
    static auto field = get_class_helper().GetProperty("CargoResourceFillLevel");
    auto* value = field.Get<float>(this);
    return value ? *value : -1.0f;
  }

  void* __get_Address()
  {
    static auto field = get_class_helper().GetProperty("Address");
    return field.GetRaw<void>(this);
  }
  FleetState __get_CurrentState()
  {
    static auto field = get_class_helper().GetProperty("CurrentState");
    return *field.Get<FleetState>(this);
  }
  FleetState __get_PreviousState()
  {
    static auto field = get_class_helper().GetProperty("PreviousState");
    return *field.Get<FleetState>(this);
  }

  uint64_t __get_Id()
  {
    static auto field = get_class_helper().GetProperty("Id");
    return *field.Get<uint64_t>(this);
  }
};
