#pragma once

#include "CallbackContainer.h"
#include "MonoSingleton.h"
#include "Vector3.h"
#include "FleetPlayerData.h"
#include "FleetDeployedData.h"
#include "IEnumerator.h"

#include <il2cpp/il2cpp_helper.h>

struct DeploymentManger : MonoSingleton<DeploymentManger> {
  friend struct MonoSingleton<DeploymentManger>;

public:
  void SetTowRequest(uint64_t towedFleetId, uint64_t towingFleetId)
  {
    static auto SetTowRequestMethod =
        get_class_helper().GetMethod<void(DeploymentManger*, uint64_t, uint64_t, void*)>("SetTowRequest");
    static auto SetTowRequestWarn = true;

    if (SetTowRequestMethod) {
      auto ptr = CallbackContainer::Create();
      SetTowRequestMethod(this, towedFleetId, towingFleetId, ptr);
    } else if (SetTowRequestWarn) {
      SetTowRequestWarn = false;
      ErrorMsg::MissingMethod("DeploymentManager", "SetTowRequest");
    }
  }

  IEnumerator* PlanCourse(FleetPlayerData* selectedFleet, void* targetAddress, Vector3 targetPosition,
                          FleetDeployedData* targetDeployedFleet, void* starbaseData, void* allianceStarbaseData,
                          void* outpost = nullptr)
  {
    static auto* method = method_contract::Resolve(
        get_class_helper().get_cls(), "PlanCourse", false, "System.Collections.IEnumerator",
        {"Digit.PrimeServer.Models.FleetPlayerData", "Digit.PrimeServer.Models.NodeAddress", "UnityEngine.Vector3",
         "Digit.PrimeServer.Models.FleetDeployedData", "Digit.PrimeServer.Models.StarbaseData",
         "Digit.PrimeServer.Models.AllianceStarbaseData", "Digit.PrimeServer.Models.Outpost"});
    void* args[]{selectedFleet, targetAddress, &targetPosition, targetDeployedFleet, starbaseData, allianceStarbaseData,
                 outpost};
    Il2CppObject* result = nullptr;
    if (Il2CppRuntime::TryInvoke(method, this, args, &result))
      return reinterpret_cast<IEnumerator*>(result);

    spdlog::warn("DeploymentManager: unable to invoke PlanCourse");
    return nullptr;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "", "DeploymentManager");
    return class_helper;
  }
};
