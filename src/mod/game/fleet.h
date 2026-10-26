#pragma once

// Player fleets and ships: fleet data and managers, forbidden tech, ship abilities, the fleet bar, the ship
// management screens and dock, and the way into the player's own Haven.

#include "app.h"
#include "server.h"
#include "ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/tracker.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// These retained-course action guards preserve signed scalar identity without changing existing bindings'
// usual size-compatible integer contract. Lookup and invocation remain the shared IL2CPP implementation.
namespace planned_warp_binding
{
template <il2cpp::Name Name, class Value, class Binding> il2cpp::CallResult<Value> Scalar(Binding* object)
{
  static const auto* method = il2cpp::detail::MethodCall<Value()>::Resolve(Binding::Class::Raw(), Name.View(), false);
  const auto*        actual = il2cpp::detail::ExactScalarGetter(method, object, il2cpp::detail::Primitive<Value>());
  return actual ? il2cpp::detail::TryInvoke<Value>(actual, object) : il2cpp::CallResult<Value>{};
}
template <il2cpp::Name Name, class Value, class Binding> il2cpp::CallResult<Value*> Reference(Binding* object)
{
  static const auto* method = il2cpp::detail::MethodCall<Value*()>::Resolve(Binding::Class::Raw(), Name.View(), false);
  const auto*        actual = il2cpp::detail::ReferenceGetter(method, object, il2cpp::detail::SlotOf<Value*>());
  return actual ? il2cpp::detail::TryInvoke<Value*>(actual, object) : il2cpp::CallResult<Value*>{};
}
template <il2cpp::Name Name, class Value, class Binding> il2cpp::CallResult<Value*> StaticReference()
{
  static const auto* method = il2cpp::detail::MethodCall<Value*()>::Resolve(Binding::Class::Raw(), Name.View(), true);
  const auto*        actual = il2cpp::detail::ReferenceGetter(method, nullptr, il2cpp::detail::SlotOf<Value*>());
  return actual ? il2cpp::detail::TryInvoke<Value*>(actual, nullptr) : il2cpp::CallResult<Value*>{};
}
} // namespace planned_warp_binding

enum class DeployedFleetType : int32_t {
  Nonexistent     = 0,
  Player          = 1,
  Marauder        = 2,
  NpcInstantiated = 3,
  Sentinel        = 4,
  Alliance        = 5,
  Challenge       = 6,
};

enum class DeployedFleetState : int32_t {
  Idle      = 0,
  Moving    = 1,
  Warping   = 2,
  Battling  = 3,
  Recall    = 4,
  Docked    = 5,
  PreBattle = 6,
};

enum class FleetRemovalReason : int32_t {
  None               = 0,
  Destroyed          = 1,
  ExitedActiveSystem = 2,
  Recalled           = 3,
  Hazard             = 4,
  Summoned           = 5,
};

// A location in the galaxy, a protobuf message.
struct NodeAddress : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "NodeAddress">;

  int64_t Galaxy()
  { return Get<"Galaxy", int64_t>(); }
  int64_t System()
  { return Get<"System", int64_t>(); }
  int64_t Planet()
  { return Get<"Planet", int64_t>(); }
  int32_t Instance()
  { return Get<"Instance", int32_t>(); }
  il2cpp::CallResult<int64_t> TrySystem()
  { return planned_warp_binding::Scalar<"get_System", int64_t>(this); }
  il2cpp::CallResult<int32_t> TryInstance()
  { return planned_warp_binding::Scalar<"get_Instance", int32_t>(this); }
  il2cpp::CallResult<int32_t> TryNodeDepth()
  { return planned_warp_binding::Scalar<"get_NodeDepth", int32_t>(this); }
};

// IDeployedFleet: the server's model behind a deployed fleet. Its class varies (DeployedFleet, DeployedFleetJSON),
// so members resolve on the runtime class.
struct DeployedFleetModel : il2cpp::Object {
  int64_t FleetId()
  { return Get<"FleetId", int64_t>(); }
};

struct FleetDeployedData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData">;

  HullSpec* Hull()
  { return Get<"Hull", HullSpec*>(); }
  // Nonexistent when the property is missing.
  DeployedFleetType FleetType()
  { return Get<"FleetType", DeployedFleetType>(); }
  int64_t Id()
  { return Get<"ID", int64_t>(); }
  il2cpp::CallResult<int64_t> TryId()
  { return TryCall<"get_ID", int64_t>(); }
  il2cpp::CallResult<bool> TryIsLocalPlayer()
  { return TryCall<"get_IsLocalPlayer", bool>(); }
  DeployedFleetState CurrentState()
  { return Get<"CurrentState", DeployedFleetState>(); }
  FleetRemovalReason RemovalReason()
  { return Get<"RemovalReason", FleetRemovalReason>(); }
  bool IsDestroyed()
  { return Get<"IsDestroyed", bool>(); }
  bool CurrentlyBattling()
  { return Get<"CurrentlyBattling", bool>(); }
  bool IsPlanningRecallCourse()
  { return Get<"IsPlanningRecallCourse", bool>(); }
  // True for the player's own fleets.
  bool localPlayerFleet()
  { return Field<"_localPlayerFleet", bool>(); }
  NodeAddress* address()
  { return Field<"_address", NodeAddress*>(); }
  DeployedFleetModel* deploymentFleet()
  { return Field<"_deploymentFleet", DeployedFleetModel*>(); }
};

struct BattleTargetData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData">;

  FleetDeployedData* TargetFleetDeployedData()
  { return Field<"TargetFleetDeployedData", FleetDeployedData*>(); }
};

enum class FleetState : int32_t {
  Unknown                = 0,
  IdleInSpace            = 1,
  Docked                 = 2,
  Mining                 = 4,
  Destroyed              = 8,
  TieringUp              = 16,
  CanReplaceOfficers     = 18,
  Repairing              = 32,
  CannotLaunch           = 56,
  Battling               = 64,
  WarpCharging           = 128,
  Warping                = 256,
  CanRemove              = 384,
  Impulsing              = 512,
  CanActivateAbility     = 513,
  CanDisco               = 515,
  Capturing              = 1024,
  AutoHunting            = 2048,
  CannotMove             = 2552,
  CanManage              = 2947,
  CanBeTargetedByAbility = 3589,
  CanEngage              = 3591,
  Outposting             = 4096,
  CanRecall              = 5637,
  Deployed               = 8133,
  CanLocate              = 8135,
};

// A ship name as the game shows it, in the player's language, for a hull's localisation id; empty for none.
inline std::string LocalizedShipName(int64_t loca_id)
{
  if (loca_id <= 0)
    return {};
  auto* context = il2cpp::New<LocaleTextContext>(il2cpp::NewString("ship_name_" + std::to_string(loca_id)),
                                                 il2cpp::NewString("ships"));
  return context ? il2cpp::ToUtf8(Localizer::Localise(context)) : std::string();
}

// The declared interface of FleetPlayerData.Ship. A fleet ID or hull ID is not
// the identity of the individual ship model held here.
struct IShipModel : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IShipModel">;
};

// The hull a fleet flies.
struct BaseShip : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BaseShip">;

  int64_t Id()
  { return Get<"Id", int64_t>(); }
};

// An item slot on a ship, such as its forbidden tech slot.
struct EntitySlot : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "EntitySlot">;

  // The slot specs of a ship's forbidden tech and chaos tech slots, as the game's ship tech indicators read them.
  static constexpr int64_t kForbiddenTechSlot = 3502081615;
  static constexpr int64_t kChaosTechSlot     = 953301906;

  int64_t SlotSpecId()
  { return Get<"SlotSpecId", int64_t>(); }
  // Empty when nothing is fitted.
  il2cpp::Nullable<int64_t> SlotItemId()
  { return Get<"SlotItemId", il2cpp::Nullable<int64_t>>(); }
  // Named after the property, so the type needs `::` here.
  ::ForbiddenTechSlotParams* ForbiddenTechSlotParams()
  { return Get<"ForbiddenTechSlotParams", ::ForbiddenTechSlotParams*>(); }
};

struct ForbiddenTechManager : MonoSingleton<ForbiddenTechManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ForbiddenTechs", "ForbiddenTechManager">;

  // CachedService<FleetService>: a struct around the service, empty until something asks for it.
  struct CachedService {
    ::FleetService* cached_service;
  };

  bool TryGetActiveForbiddenTechSlot(int64_t ship_id, il2cpp::List<EntitySlot*>*& slots)
  { return Call<"TryGetActiveForbiddenTechSlot", bool>(ship_id, std::ref(slots)); }
  bool TryGetEntitySlotFromForbiddenTech(int64_t tech_id, EntitySlot*& slot)
  { return Call<"TryGetEntitySlotFromForbiddenTech", bool>(tech_id, std::ref(slot)); }

  // Until the cache holds the service, the cache's Service getter looks it up and keeps it. Named after the service,
  // so the type needs `::` here.
  ::FleetService* FleetService()
  {
    auto* service = Field<"_fleetServiceCache", CachedService>().cached_service;
    return service ? service : CallOnField<"_fleetServiceCache", CachedService, "get_Service", ::FleetService*>();
  }
};

struct FleetPlayerData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData">;

  int32_t Index()
  { return Get<"Index", int32_t>(); }
  HullSpec* Hull()
  { return Get<"Hull", HullSpec*>(); }
  FleetState CurrentState()
  { return Get<"CurrentState", FleetState>(); }
  int64_t Id()
  { return Get<"Id", int64_t>(); }
  il2cpp::CallResult<int64_t> TryId()
  { return planned_warp_binding::Scalar<"get_Id", int64_t>(this); }
  il2cpp::CallResult<NodeAddress*> TryAddress()
  { return planned_warp_binding::Reference<"get_Address", NodeAddress>(this); }
  int64_t Level()
  { return Get<"Level", int64_t>(); }
  bool HasShip()
  { return Get<"HasShip", bool>(); }
  int64_t HullId()
  { return Get<"HullId", int64_t>(); }
  int64_t GetLocaId()
  { return Call<"GetLocaId", int64_t>(); }

  static bool ShipIdentityAvailable()
  {
    static const bool available =
        Class::HasProperty<"Ship", IShipModel*>() && BaseShip::Class::HasProperty<"Id", int64_t>();
    return available;
  }
  std::optional<int64_t> ShipInstanceId()
  {
    if (!ShipIdentityAvailable())
      return std::nullopt;
    const auto observed = TryCall<"get_Ship", IShipModel*>();
    auto*      ship     = observed && observed.value ? observed.value->As<BaseShip>() : nullptr;
    if (!ship)
      return std::nullopt;
    const auto id = ship->TryCall<"get_Id", int64_t>();
    return id && id.value > 0 ? std::optional{id.value} : std::nullopt;
  }

  // A forbidden tech or chaos tech item is fitted to the ship.
  bool HasAppliedTech()
  {
    auto*                      ship    = Get<"Ship", BaseShip*>();
    auto*                      manager = ForbiddenTechManager::Instance();
    il2cpp::List<EntitySlot*>* slots   = nullptr;
    if (!ship || !manager || !manager->TryGetActiveForbiddenTechSlot(ship->Id(), slots))
      return false;
    static constexpr std::array kTechSlots{EntitySlot::kForbiddenTechSlot, EntitySlot::kChaosTechSlot};
    return std::ranges::any_of(il2cpp::Items(slots), [](EntitySlot* slot) {
      return slot && std::ranges::contains(kTechSlots, slot->SlotSpecId()) && slot->SlotItemId().has_value;
    });
  }

  // The ship's name as the game shows it, in the player's language; empty without a hull.
  std::string DisplayName()
  { return LocalizedShipName(Hull() ? GetLocaId() : 0); }
};

// Digit.PrimeServer.Models.ActionType is a struct around a flag value, not an enum, so it crosses the
// managed boundary as a 4-byte struct.
struct ActionType {
  int32_t flagValue;

  friend bool operator==(ActionType, ActionType) = default;

  static const ActionType Manage;
  static const ActionType Repair;
  static const ActionType Recall;
};

inline constexpr ActionType ActionType::Manage{-1997567611};
inline constexpr ActionType ActionType::Repair{-1850668115};
inline constexpr ActionType ActionType::Recall{-1851055311};

enum class ActionBehaviour : int32_t {
  Default             = 1,
  Instant             = 2,
  Speedup             = 4,
  Collect             = 8,
  AskHelp             = 16,
  RequestConfirmation = 32,
};

// Digit.PrimeServer.Models.TimerDataContext, read on its runtime class: the mod only needs the remaining time, and
// the class has moved between client builds.
struct TimerDataContext : il2cpp::Object {
  unity::TimeSpan RemainingTime()
  { return Get<"RemainingTime", unity::TimeSpan>(); }
};

struct ShipActionPanelLocalViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipActionPanelLocalViewController">;

  il2cpp::Array<int64_t>* abilityIDs()
  { return Field<"_abilityIDs", il2cpp::Array<int64_t>*>(); }
  il2cpp::Array<GenericButtonWidget*>* buttons()
  { return Field<"_buttons", il2cpp::Array<GenericButtonWidget*>*>(); }
  TimerDataContext* GetRemainingCooldown(int64_t abilityId)
  { return Call<"GetRemainingCooldown", TimerDataContext*>(abilityId); }
  bool HasShipAnActiveAbilityWithActiveEffect()
  { return Call<"HasShipAnActiveAbilityWithActiveEffect", bool>(); }
};

struct FleetLocalViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController">;

  ShipActionPanelLocalViewController* shipActionPanelLocalViewController()
  { return Field<"_shipActionPanelLocalViewController", ShipActionPanelLocalViewController*>(); }
  FleetPlayerData* fleet()
  { return Get<"fleet", FleetPlayerData*>(); }
  void CancelButtonClicked()
  { Call<"CancelButtonClicked">(); }

  // `target` is the IActionData the action is on, such as a FleetPlayerData. `callback` is an
  // Action<bool, ActionBehaviour>, or null.
  bool RequestAction(il2cpp::Object* target, ActionType type, int64_t index, ActionBehaviour behaviourMask,
                     void* callback = nullptr)
  { return Call<"RequestAction", bool>(target, type, index, behaviourMask, callback); }
};

struct FleetBarContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FleetBarContext">;

  il2cpp::Array<FleetPlayerData*>* FleetData()
  { return Get<"FleetData", il2cpp::Array<FleetPlayerData*>*>(); }
};

struct FleetBarViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController">;

  FleetLocalViewController* fleetPanelController()
  { return Field<"_fleetPanelController", FleetLocalViewController*>(); }
  FleetBarContext* CanvasContext()
  { return Get<"CanvasContext", FleetBarContext*>(); }

  // The int overload; the other takes the list element Component.
  void RequestSelect(int32_t index, bool simulated = false)
  { Call<"RequestSelect">(index, simulated); }
  bool IsIndexSelected(int32_t index)
  { return Call<"IsIndexSelected", bool>(index); }

  // The fleet whose panel is open in the bar, or null.
  FleetPlayerData* SelectedFleet()
  {
    auto* panel = fleetPanelController();
    return panel ? panel->fleet() : nullptr;
  }

  // The fleet panel of the newest fleet bar, or null. Its fleet() is the selected fleet, or null.
  static FleetLocalViewController* NewestPanel()
  {
    auto* bar = unity::Newest<FleetBarViewController>();
    return bar ? bar->fleetPanelController() : nullptr;
  }
};

struct FleetsManager : MonoSingleton<FleetsManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.FleetManagement", "FleetsManager">;

  // CachedService<FleetService>: a struct around the service, empty until something asks for it.
  struct CachedService {
    il2cpp::Object* cached_service;
  };

  // The fleet service is up. Until the cache holds it, the cache's HasService looks it up and keeps it.
  bool HasFleetService()
  {
    return Field<"_fleetServiceCache", CachedService>().cached_service
           || CallOnField<"_fleetServiceCache", CachedService, "get_HasService", bool>();
  }
  FleetPlayerData* GetFleetPlayerData(int32_t index)
  { return Call<"GetFleetPlayerData", FleetPlayerData*>(index); }
  il2cpp::CallResult<int32_t> TryPlayerFleetIndex(int64_t fleet_id)
  { return TryCall<"GetPlayerFleetIndex", int32_t(int64_t)>(fleet_id); }
  FleetPlayerData* GetSelectedFleetData()
  { return Call<"GetSelectedFleetData", FleetPlayerData*>(); }
  void RequestViewFleet(FleetPlayerData* fleetData, bool showSystemInfo = false)
  { Call<"RequestViewFleet">(fleetData, showSystemInfo); }
};

// Native planned courses also represent deployed fleets without PlayerFleet. FleetID is the identity observation.
struct PathFinderResultData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "PathFinderResultData">;
  il2cpp::CallResult<int32_t> TryResult()
  { return planned_warp_binding::Scalar<"get_Result", int32_t>(this); }
};
struct CourseNode : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseNode">;
  il2cpp::CallResult<NodeAddress*> TryAddress()
  { return planned_warp_binding::Reference<"get_Address", NodeAddress>(this); }
  il2cpp::CallResult<int64_t> TryId()
  { return planned_warp_binding::Scalar<"get_ID", int64_t>(this); }
  il2cpp::CallResult<unity::Vector3> TrySystemPosition()
  { return TryPosition<"get_SystemPosition">(); }
  il2cpp::CallResult<unity::Vector3> TryGalaxyPosition()
  { return TryPosition<"get_GalaxyPosition">(); }

private:
  template <il2cpp::Name Name> il2cpp::CallResult<unity::Vector3> TryPosition()
  {
    static const auto* method = il2cpp::detail::MethodCall<unity::Vector3()>::Resolve(Class::Raw(), Name.View(), false);
    return unity::TryReadVector3(method, this);
  }
};
struct ITreeNode : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ITreeNode">;
  il2cpp::CallResult<int64_t> TryId()
  { return planned_warp_binding::Scalar<"get_ID", int64_t>(this); }
  il2cpp::CallResult<int32_t> TryType()
  { return planned_warp_binding::Scalar<"get_Type", int32_t>(this); }
};
struct CourseData : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseData">;
  FleetPlayerData* PlayerFleet()
  { return Get<"PlayerFleet", FleetPlayerData*>(); }
  il2cpp::CallResult<int64_t> TryFleetId()
  { return planned_warp_binding::Scalar<"get_FleetID", int64_t>(this); }
  il2cpp::CallResult<int32_t> TryType()
  { return planned_warp_binding::Scalar<"get_Type", int32_t>(this); }
  il2cpp::CallResult<int32_t> TryTargetAction()
  { return planned_warp_binding::Scalar<"get_TargetAction", int32_t>(this); }
  il2cpp::CallResult<bool> TryInUse()
  { return planned_warp_binding::Scalar<"get_IsInUse", bool>(this); }
  il2cpp::CallResult<bool> TryWarp()
  { return planned_warp_binding::Scalar<"get_IsWarpCourse", bool>(this); }
  il2cpp::CallResult<bool> TryRecall()
  { return planned_warp_binding::Scalar<"get_IsRecallCourse", bool>(this); }
  il2cpp::CallResult<bool> TryWormhole()
  { return planned_warp_binding::Scalar<"get_IsWormholeCourse", bool>(this); }
  il2cpp::CallResult<bool> TryToll()
  { return planned_warp_binding::Scalar<"get_HasToll", bool>(this); }
  il2cpp::CallResult<PathFinderResultData*> TryPath()
  { return planned_warp_binding::Reference<"get_PathFinderResult", PathFinderResultData>(this); }
  il2cpp::CallResult<CourseNode*> TryTarget()
  { return planned_warp_binding::Reference<"get_TargetNode", CourseNode>(this); }
  il2cpp::CallResult<CourseNode*> TryStart()
  { return planned_warp_binding::Reference<"get_StartNode", CourseNode>(this); }
};

struct DeploymentManager : MonoSingleton<DeploymentManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "", "DeploymentManager">;

  bool HasInstantWarpAbility(FleetPlayerData* selectedFleet)
  { return Call<"HasInstantWarpAbility", bool>(selectedFleet); }
  il2cpp::CallResult<CourseData*> TryPlannedCourse(int64_t fleet_id)
  {
    static const auto* method = [] {
      const auto* candidate =
          il2cpp::detail::MethodCall<CourseData*(int64_t)>::Resolve(Class::Raw(), "GetPlannedCourse", false);
      if (candidate
          && (!candidate->methodPointer
              || (candidate->has_full_generic_sharing_signature ? !candidate->invoker_method
                                                                : !candidate->virtualMethodPointer)
              || (candidate->flags & METHOD_ATTRIBUTE_VIRTUAL)
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[0], IL2CPP_TYPE_I8))) {
        il2cpp::Report("[IL2CPP] DeploymentManager.GetPlannedCourse requires a signed fleet identity");
        return static_cast<const MethodInfo*>(nullptr);
      }
      return candidate;
    }();
    return il2cpp::detail::TryInvoke<CourseData*>(method, this, fleet_id);
  }
};

// Discovery's Shift+ship course is scheduled by Unity. These exact client273
// contracts decline on drift; no native iterator body is advanced by the mod.
namespace discovery_tow_binding
{
struct Routine : il2cpp::Object {
  using Class = il2cpp::Class<"mscorlib", "System.Collections", "IEnumerator">;
};
struct Coroutine : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Coroutine">;
};
struct Mono : il2cpp::Object {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "MonoBehaviour">;
};
struct Starbase : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "StarbaseData">;
};
struct AllianceStarbase : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "AllianceStarbaseData">;
};
struct Outpost : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Outpost">;
};
struct Error : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "GSError">;
};
struct ErrorDelegate : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "OnError">;
};
struct RetryDelegate : il2cpp::Object {
  using Class = il2cpp::Class<"Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "OnRetry">;
};
struct Reference : il2cpp::Object {
  struct Class {
    static Il2CppClass* Raw()
    { return il2cpp::ObjectClass(); }
  };
};
struct RecallCallbacks : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager/<>c">;
};
struct PlanIterator : Routine {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services",
                              "DeploymentService/<PlanCourse>d__136">;
};

inline bool ClosedInt32(const Il2CppType* type, Il2CppClass* definition)
{
  const auto* generic =
      type && !type->byref && type->type == IL2CPP_TYPE_GENERICINST ? type->data.generic_class : nullptr;
  const auto* args = generic ? generic->context.class_inst : nullptr;
  return generic && generic->type && definition && il2cpp_class_from_type(generic->type) == definition && args
         && args->type_argc == 1 && args->type_argv
         && il2cpp::detail::ExactScalarMatches(args->type_argv[0], IL2CPP_TYPE_I4)
         && il2cpp_class_from_type(args->type_argv[0]) == il2cpp::PrimitiveClass<int32_t>();
}
struct Api {
  il2cpp::Method    plan, start, stop, stop_iterator, tow, success, error;
  const MethodInfo* constructor = nullptr;
  Il2CppClass*      callbacks   = nullptr;
  FieldInfo *       state = nullptr, *fleet = nullptr, *destination = nullptr;
  bool              ready = false;
};
inline const Api& Native()
{
  static const Api api = [] {
    Api result;
    result.plan =
        DeploymentManager::Class::Method<Routine*(FleetPlayerData*, NodeAddress*, unity::Vector3, FleetDeployedData*,
                                                  Starbase*, AllianceStarbase*, Outpost*)>("PlanCourse");
    result.start         = Mono::Class::Method<Coroutine*(Routine*)>("StartCoroutine");
    result.stop          = Mono::Class::Method<void(Coroutine*)>("StopCoroutine");
    result.stop_iterator = Mono::Class::Method<void(Routine*)>("StopCoroutine");
    result.tow           = DeploymentManager::Class::Method<void(int64_t, int64_t, Reference*)>("SetTowRequest");
    result.success       = RecallCallbacks::Class::Method<void(int32_t)>("<OnShipRecallAction>b__91_0");
    result.error         = RecallCallbacks::Class::Method<void(Error*)>("<OnShipRecallAction>b__91_1");
    result.state         = il2cpp::detail::ResolveField(PlanIterator::Class::Raw(), "<>1__state",
                                                        il2cpp::detail::SlotOf<int32_t>(), false);
    result.fleet         = il2cpp::detail::ResolveField(PlanIterator::Class::Raw(), "fleet",
                                                        il2cpp::detail::SlotOf<FleetPlayerData*>(), false);
    result.destination   = il2cpp::detail::ResolveField(PlanIterator::Class::Raw(), "targetAddress",
                                                        il2cpp::detail::SlotOf<NodeAddress*>(), false);
    using Definition = il2cpp::Class<"Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "CallbackContainer`1">;
    using Success    = il2cpp::Class<"Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "OnSuccess`1">;
    const auto* tow  = result.tow.Info();
    if (tow && il2cpp::detail::ExactScalarMatches(tow->parameters[0], IL2CPP_TYPE_I8)
        && il2cpp::detail::ExactScalarMatches(tow->parameters[1], IL2CPP_TYPE_I8)
        && ClosedInt32(tow->parameters[2], Definition::Raw())) {
      result.callbacks = il2cpp_class_from_type(tow->parameters[2]);
      result.constructor =
          il2cpp::detail::MethodCall<void(Reference*, ErrorDelegate*, int32_t, RetryDelegate*)>::Resolve(
              result.callbacks, ".ctor", false);
    }
    const auto* ctor = result.constructor;
    result.ready = result.plan && result.start && result.stop && result.stop_iterator && result.tow && result.success
                   && result.error && result.state && result.fleet && result.destination
                   && il2cpp::detail::ExactScalarMatches(result.state->type, IL2CPP_TYPE_I4) && ctor
                   && il2cpp::detail::ValueClassMatches(result.plan.Info()->parameters[2], unity::Vector3Class(),
                                                        sizeof(unity::Vector3))
                   && ClosedInt32(ctor->parameters[0], Success::Raw())
                   && il2cpp::detail::ResolveField(RecallCallbacks::Class::Raw(), "<>9",
                                                   il2cpp::detail::SlotOf<RecallCallbacks*>(), true)
                   && NodeAddress::Class::Method<NodeAddress*()>("Clone")
                   && NodeAddress::Class::Method<bool(NodeAddress*)>("Equals")
                   && PathFinderResultData::Class::Method<bool()>("get_Valid")
                   && DeploymentManager::Class::Method<bool(FleetPlayerData*)>("CanTow")
                   && DeploymentManager::Class::Method<bool(FleetPlayerData*, FleetPlayerData*)>("CanBeTowed");
    return result;
  }();
  return api;
}
inline il2cpp::CallResult<Routine*> Plan(DeploymentManager* owner, FleetPlayerData* selected, NodeAddress* destination)
{
  return Native().ready && owner && selected && destination
             ? il2cpp::detail::TryInvoke<Routine*>(
                   Native().plan.Info(), owner, selected, destination, unity::Vector3{},
                   static_cast<FleetDeployedData*>(nullptr), static_cast<Starbase*>(nullptr),
                   static_cast<AllianceStarbase*>(nullptr), static_cast<Outpost*>(nullptr))
             : il2cpp::CallResult<Routine*>{};
}
inline il2cpp::CallResult<int32_t> State(Routine* iterator)
{
  return Native().ready && iterator && iterator->Is<PlanIterator>()
             ? il2cpp::CallResult<int32_t>{true, il2cpp::detail::ReadField<int32_t>(Native().state, iterator)}
             : il2cpp::CallResult<int32_t>{};
}
inline bool FreshIterator(Routine* iterator, FleetPlayerData* fleet, NodeAddress* destination)
{
  const auto state = State(iterator);
  return state && state.value == 0 && il2cpp::detail::ReadField<FleetPlayerData*>(Native().fleet, iterator) == fleet
         && il2cpp::detail::ReadField<NodeAddress*>(Native().destination, iterator) == destination;
}
inline il2cpp::CallResult<Coroutine*> Start(DeploymentManager* owner, Routine* iterator)
{
  return Native().ready && owner && owner->Is<Mono>() && iterator
             ? il2cpp::detail::TryInvoke<Coroutine*>(Native().start.Info(), owner, iterator)
             : il2cpp::CallResult<Coroutine*>{};
}
inline bool Stop(DeploymentManager* owner, Routine* iterator, Coroutine* coroutine)
{
  if (!Native().ready || !owner || !owner->Is<Mono>())
    return false;
  if (coroutine)
    return il2cpp::detail::TryInvoke<void>(Native().stop.Info(), owner, coroutine).succeeded;
  else if (iterator)
    return il2cpp::detail::TryInvoke<void>(Native().stop_iterator.Info(), owner, iterator).succeeded;
  return true;
}
inline il2cpp::GcHandle Callbacks()
{
  const auto& api = Native();
  if (!api.ready)
    return {};
  auto owner = il2cpp::GcHandle::Strong(RecallCallbacks::Class::Field<"<>9", RecallCallbacks*>());
  if (!owner || !owner.Get<il2cpp::Object>()->Is<RecallCallbacks>())
    return {};
  // These are the game's actual native recall no-op handlers (both ret in the
  // retained273 body), bound to its real managed singleton. No self-pointers,
  // fake delegate fields, metadata mutation or managed-reference shim.
  auto success = il2cpp::GcHandle::Strong(il2cpp::NewClosedDelegate(
      il2cpp_class_from_type(api.constructor->parameters[0]), owner.Get<RecallCallbacks>(), api.success));
  auto error   = il2cpp::GcHandle::Strong(il2cpp::NewClosedDelegate(
      il2cpp_class_from_type(api.constructor->parameters[1]), owner.Get<RecallCallbacks>(), api.error));
  if (!success || !error)
    return {};
  auto value = il2cpp::GcHandle::Strong(il2cpp_object_new(api.callbacks));
  if (!value)
    return {};
  if (!il2cpp::detail::TryInvoke<void>(api.constructor, value.Get(), success.Get<Reference>(),
                                       error.Get<ErrorDelegate>(), int32_t{2}, static_cast<RetryDelegate*>(nullptr)))
    return {};
  const auto observed_success = value.Get<il2cpp::Object>()->TryCall<"get_OnSuccess", Reference*>();
  const auto observed_error   = value.Get<il2cpp::Object>()->TryCall<"get_OnError", ErrorDelegate*>();
  return observed_success && observed_error && observed_success.value == success.Get()
                 && observed_error.value == error.Get()
             ? std::move(value)
             : il2cpp::GcHandle{};
}
inline il2cpp::CallResult<void> Submit(DeploymentManager* owner, int64_t selected, int64_t discovery,
                                       Reference* callbacks)
{
  return Native().ready && owner && selected > 0 && discovery > 0 && selected != discovery && callbacks
                 && callbacks->klass == Native().callbacks
             ? il2cpp::detail::TryInvoke<void>(Native().tow.Info(), owner, selected, discovery, callbacks)
             : il2cpp::CallResult<void>{};
}
} // namespace discovery_tow_binding

struct FleetMeshSelector : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "FleetMeshSelector">;

  unity::GameObject* LoadedObject()
  { return Get<"LoadedObject", unity::GameObject*>(); }
};

// Places a ship model in system view; the hook scales it by ui_scale_ship.
using SystemViewShip = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "SystemViewShip">;

struct ShipManagementViewContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementViewContext">;

  il2cpp::List<FleetPlayerData*>* SortedIdleShips()
  { return Field<"SortedIdleShips", il2cpp::List<FleetPlayerData*>*>(); }
  FleetPlayerData* CurrentFleet()
  { return Get<"CurrentFleet", FleetPlayerData*>(); }
  int32_t CurrentIndex()
  { return Get<"CurrentIndex", int32_t>(); }
  void SetCurrentIndex(int32_t index)
  { Set<"CurrentIndex">(index); }
  void SetLastMovementWasToIncrement(bool increment)
  { Set<"LastMovementWasToIncrement">(increment); }
  int32_t ListCount()
  { return Get<"ListCount", int32_t>(); }
  void Sort()
  { Call<"Sort">(); }
};

struct ShipManagementViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementViewController">;

  // m_context lives on the generic ViewController`1 base; field lookup walks the parents.
  ShipManagementViewContext* Context()
  { return Field<"m_context", ShipManagementViewContext*>(); }
};

struct ShipManagementScreenStateViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementScreenStateViewController">;

  unity::Button* officerIconButton()
  { return Field<"_officerIconButton", unity::Button*>(); }
  unity::Button* swapShipButton()
  { return Field<"_swapShipButton", unity::Button*>(); }
};

struct AssignShipsWidget : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "AssignShipsWidget">;

  InputFieldWidget* inputField()
  { return Field<"_inputField", InputFieldWidget*>(); }
  ButtonAndContextWrapper* assignButton()
  { return Field<"_assignButton", ButtonAndContextWrapper*>(); }
  SelectableList* selectableList()
  { return Field<"_selectableList", SelectableList*>(); }
  ShipManagementViewContext* Context()
  { return Get<"Context", ShipManagementViewContext*>(); }
  bool SortingChanged()
  { return TryCall<"HandleSortingChanged", void(int32_t)>(0).succeeded; }
};

struct ShipSelectionViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipSelectionViewController">;

  ShipManagementViewContext* Context()
  { return Field<"m_context", ShipManagementViewContext*>(); }
};

struct ShipTileWidget : Widget<FleetPlayerData> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipTileWidget">;

  unity::Transform* Transform()
  {
    auto* component = As<unity::Component>();
    return component ? component->transform() : nullptr;
  }
  ShipManagementViewContext* PinContext()
  {
    auto* transform = Transform();
    auto* owner     = transform ? transform->FindInParents<ShipSelectionViewController>() : nullptr;
    auto* context   = owner ? owner->Context() : nullptr;
    return context && context->Is<ShipManagementViewContext>() ? context : nullptr;
  }
};

// Older clients lack this class; there the binding logs it once and ViewOwnHaven does nothing.
struct PlanetaryBaseManager : MonoSingleton<PlanetaryBaseManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlanetaryBase", "PlanetaryBaseManager">;

  static void ViewOwnHaven()
  {
    auto* manager  = Instance();
    auto* sections = Hub::Sections();
    // Preserve the game's Iconian Gateway prerequisite and its locked-building message.
    if (!manager || !sections || !manager->Call<"IsPlanetaryBaseCenterReady", bool>(true))
      return;
    // PlanetarySectionContext is reserved for visiting another player's Haven. Passing no context makes the
    // PlanetarySectionDirector initialize the complete local Haven state from the service cache.
    sections->TriggerSectionChange(SectionID::Starbase_Planetary, nullptr, false, false, true);
  }
};

struct ActivatedAbilityManager : MonoSingleton<ActivatedAbilityManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ActivatedAbilities", "ActivatedAbilityManager">;

  // ActivatedAbilityType.RevealHostileVger.
  static constexpr int32_t kRevealHostileVger = 86002;

  int32_t GetActivatedAbilityType(int64_t abilityId)
  { return Call<"GetActivatedAbilityType", int32_t>(abilityId); }
  bool CanUseAbility(FleetPlayerData* fleet, int64_t abilityId)
  { return Call<"CanUseAbility", bool>(fleet, abilityId); }
};
