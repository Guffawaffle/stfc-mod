#pragma once

#include "HullSpec.h"
#include "NodeAddress.h"
#include "UserProfile.h"
#include <il2cpp/il2cpp_helper.h>

enum class DeployedFleetType {
  Nonexistent,
  Player,
  Marauder,
  NpcInstantiated,
  Sentinel,
  Alliance,
  Challenge,
};

struct FleetDeployedData {
public:
  __declspec(property(get = __get_Address)) NodeAddress*        Address;
  __declspec(property(get = __get_CurrentlyBattling)) bool      CurrentlyBattling;
  __declspec(property(get = __get_CurrentState)) int            CurrentState;
  __declspec(property(get = __get_ID)) long                     ID;
  __declspec(property(get = __get_IsDestroyed)) bool            IsDestroyed;
  __declspec(property(get = __get_Hull)) HullSpec*              Hull;
  __declspec(property(get = __get_FleetType)) DeployedFleetType FleetType;
  __declspec(property(get = __get_NeedsHostileHighlight)) bool  NeedsHostileHighlight;
  __declspec(property(get = __get_PreviousState)) int           PreviousState;
  __declspec(property(get = __get_User)) UserProfile*           User;
  __declspec(property(get = __get_UserId)) Il2CppString*        UserId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
    return class_helper;
  }

public:
  NodeAddress* __get_Address()
  {
    static auto prop = get_class_helper().GetProperty("Address");
    return prop.GetRaw<NodeAddress>(this);
  }

  bool __get_CurrentlyBattling()
  {
    static auto prop  = get_class_helper().GetProperty("CurrentlyBattling");
    auto*       value = prop.Get<bool>(this);
    return value ? *value : false;
  }

  int __get_CurrentState()
  {
    static auto prop  = get_class_helper().GetProperty("CurrentState");
    auto*       value = prop.Get<int>(this);
    return value ? *value : -1;
  }

  long __get_ID()
  {
    static auto field = get_class_helper().GetProperty("ID");
    return *field.Get<long>(this);
  }

  bool __get_IsDestroyed()
  {
    static auto prop  = get_class_helper().GetProperty("IsDestroyed");
    auto*       value = prop.Get<bool>(this);
    return value ? *value : false;
  }

  HullSpec* __get_Hull()
  {
    static auto field = get_class_helper().GetProperty("Hull");
    return field.GetRaw<HullSpec>(this);
  }

  DeployedFleetType __get_FleetType()
  {
    static auto field = get_class_helper().GetProperty("FleetType");
    return *field.Get<DeployedFleetType>(this);
  }

  bool __get_NeedsHostileHighlight()
  {
    static auto prop  = get_class_helper().GetProperty("NeedsHostileHighlight");
    auto*       value = prop.Get<bool>(this);
    return value ? *value : false;
  }

  int __get_PreviousState()
  {
    static auto prop  = get_class_helper().GetProperty("PreviousState");
    auto*       value = prop.Get<int>(this);
    return value ? *value : -1;
  }

  UserProfile* __get_User()
  {
    static auto prop = get_class_helper().GetProperty("User");
    return prop.GetRaw<UserProfile>(this);
  }

  Il2CppString* __get_UserId()
  {
    static auto prop = get_class_helper().GetProperty("UserId");
    return prop.GetRaw<Il2CppString>(this);
  }
};
