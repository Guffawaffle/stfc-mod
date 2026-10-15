#include "patches/mining_preview.h"

#include "config.h"
#include "errormsg.h"
#include "il2cpp/method_contract.h"
#include "il2cpp/runtime.h"
#include "prime/Hub.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanTargetWidget.h"
#include <chrono>
#include <cstring>
#include <optional>
#include <spdlog/spdlog.h>

namespace mining_preview
{
namespace
{
  using Clock = std::chrono::steady_clock;
  struct Api {
    FieldInfo *       viewerContext{}, *contextPoi{}, *nodeSlot{}, *slotFleet{}, *poiId{}, *poiType{};
    const MethodInfo *getPoi{}, *selectPoi{}, *canSelect{}, *fleetId{}, *rewardsClick{};
    bool              ready{};

    Api()
    {
      auto cls = [](const char* assembly, const char* space, const char* name) {
        return il2cpp_get_class_helper(assembly, space, name).get_cls();
      };
      auto field = [](Il2CppClass* type, const char* name, const char* expected) {
        auto* value = type ? il2cpp_class_get_field_from_name(type, name) : nullptr;
        return value && value->type && !(value->type->attrs & FIELD_ATTRIBUTE_STATIC)
                       && method_contract::Type(value->type, expected)
                   ? value
                   : nullptr;
      };
      auto* viewer  = cls("Assembly-CSharp", "Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget");
      auto* mining  = cls("Assembly-CSharp", "Digit.Prime.ObjectViewer", "MiningObjectViewerWidget");
      auto* context = cls("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIContext");
      auto* poi     = cls("Assembly-CSharp", "Digit.Prime.Navigation", "POI");
      auto* nav     = NavigationInteractionUIViewController::get_class_helper().get_cls();
      auto* slot    = cls("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MiningSlotData");
      auto* fleet   = cls("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
      auto* rewards = cls("Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget");
      viewerContext =
          field(viewer, "<CanvasContext>k__BackingField", "Digit.Prime.Navigation.NavigationInteractionUIContext");
      contextPoi = field(context, "Poi", "Digit.Prime.Navigation.POI");
      nodeSlot   = field(mining, "_nodeSlot", "Digit.PrimeServer.Models.MiningSlotData");
      slotFleet  = field(slot, "_fleetData", "Digit.PrimeServer.Models.FleetDeployedData");
      poiId      = field(poi, "<ID>k__BackingField", "System.Int64");
      poiType    = field(poi, "_interactableType", "Digit.Prime.Navigation.InteractableType");
      using method_contract::Resolve;
      getPoi       = Resolve(poi, "GetOfType", true, "Digit.Prime.Navigation.POI",
                             {"System.Int64", "Digit.Prime.Navigation.InteractableType"});
      selectPoi    = Resolve(nav, "SelectPOI", false, "System.Void", {"Digit.Prime.Navigation.POI"});
      canSelect    = Resolve(nav, "CanSelectPOI", false, "System.Boolean", {});
      fleetId      = Resolve(fleet, "get_ID", false, "System.Int64", {});
      rewardsClick = Resolve(rewards, "RewardsClicked", false, "System.Void", {});
      ready        = viewerContext && contextPoi && nodeSlot && slotFleet && poiId && poiType && getPoi && selectPoi
                     && canSelect && fleetId && rewardsClick;
      spdlog::info("[MiningPreview] shortcut cycle ready={}", ready);
    }
  };

  Api& Native()
  {
    static Api api;
    return api;
  }

  Il2CppObject* Object(void* owner, FieldInfo* field)
  {
    Il2CppObject* result{};
    if (owner && field)
      il2cpp_field_get_value(static_cast<Il2CppObject*>(owner), field, &result);
    return result;
  }

  template <typename T> T Value(void* owner, FieldInfo* field)
  {
    T result{};
    if (owner && field)
      il2cpp_field_get_value(static_cast<Il2CppObject*>(owner), field, &result);
    return result;
  }

  std::optional<std::int64_t> FleetId(Il2CppObject* fleet)
  {
    Il2CppObject* boxed{};
    if (!fleet || !Il2CppRuntime::TryInvoke(Native().fleetId, fleet, nullptr, &boxed) || !boxed)
      return {};
    auto* value = il2cpp_object_unbox(boxed);
    if (!value)
      return {};
    std::int64_t id{};
    std::memcpy(&id, value, sizeof(id));
    return id != 0 && id != -1 ? std::optional{id} : std::nullopt;
  }

  Il2CppObject* FindPoi(std::int64_t id, int type)
  {
    Il2CppObject* result{};
    void*         args[]{&id, &type};
    Il2CppRuntime::TryInvoke(Native().getPoi, nullptr, args, &result);
    return result;
  }

  template <typename T> bool Visible(T* widget)
  {
    auto* visibility = widget ? widget->_visibilityController : nullptr;
    // The existing wrappers read live metadata rather than fixed offsets.
    return visibility
           && (visibility->_state == VisibilityState::Visible || visibility->_state == VisibilityState::Show);
  }

  struct Session {
    Il2CppGCHandle    context{};
    std::int64_t      node{}, fleet{};
    int               nodeType{};
    bool              cargoRequested{}, sawShip{};
    Clock::time_point deadline{};
  } session;

  void Reset()
  {
    if (session.context)
      il2cpp_gchandle_free(session.context);
    session = {};
  }

  bool SystemMap()
  { return Hub::get_SectionManager()->CurrentSection == SectionID::Navigation_System; }

  Il2CppObject* Context()
  { return session.context ? il2cpp_gchandle_get_target(session.context) : nullptr; }

  bool Select(Il2CppObject* poi)
  {
    for (auto* nav : ObjectFinder<NavigationInteractionUIViewController>::GetAll()) {
      if (!nav || reinterpret_cast<Il2CppObject*>(nav->CanvasContext) != Context())
        continue;
      bool          allowed{};
      Il2CppObject* boxed{};
      if (!poi || !Il2CppRuntime::TryInvoke(Native().canSelect, nav, nullptr, &boxed)
          || !Il2CppRuntime::TryBoolean(boxed, allowed) || !allowed)
        return false;
      void* args[]{poi};
      return Il2CppRuntime::TryInvoke(Native().selectPoi, nav, args);
    }
    return false;
  }

  PreScanTargetWidget* Ship()
  {
    for (auto* widget : ObjectFinder<PreScanTargetWidget>::GetAll()) {
      if (Visible(widget) && Object(widget, Native().viewerContext) == Context()) {
        auto* data   = widget->_battleTargetData;
        auto* target = data ? data->TargetFleetDeployedData : nullptr;
        if (target && target->ID == session.fleet)
          return widget;
      }
    }
    return nullptr;
  }
} // namespace

void Update()
{
  if (!session.context)
    return;
  auto*      context = Context();
  auto*      poi     = Object(context, Native().contextPoi);
  const auto id      = Value<std::int64_t>(poi, Native().poiId);
  if (!Config::Get().hotkeys_enabled || Config::Get().use_scopely_hotkeys || !SystemMap() || !poi
      || (id != session.node && id != session.fleet)) {
    Reset();
    return;
  }
  if (Ship()) {
    session.sawShip = true;
  } else if (session.sawShip || Clock::now() > session.deadline) {
    Reset();
  }
}

bool Cycle()
{
  if (!SystemMap() || !Native().ready)
    return false;
  Update();
  if (session.context) {
    auto* ship = Ship();
    if (!ship)
      return true; // Wait for the selected ship's native popup to finish loading.
    auto* button  = ship->_rewardsButtonWidget;
    auto* rewards = button ? button->_rewardsController : nullptr;
    if (!rewards)
      return true;
    if (rewards->_state == VisibilityState::Visible || rewards->_state == VisibilityState::Show) {
      auto*      node     = FindPoi(session.node, session.nodeType);
      const bool selected = Select(node);
      Reset();
      return selected;
    }
    session.cargoRequested = true;
    Il2CppRuntime::TryInvoke(Native().rewardsClick, button, nullptr);
    return true;
  }
  for (auto* widget : ObjectFinder<MiningObjectViewerWidget>::GetAll()) {
    if (!Visible(widget) || widget->_occupiedState == NotOccupied)
      continue;
    auto*      context = Object(widget, Native().viewerContext);
    auto*      node    = Object(context, Native().contextPoi);
    const auto fleet   = FleetId(Object(Object(widget, Native().nodeSlot), Native().slotFleet));
    if (!node || !fleet)
      continue;
    const auto nodeId   = Value<std::int64_t>(node, Native().poiId);
    const auto nodeType = Value<int>(node, Native().poiType);
    auto*      poi      = FindPoi(*fleet, 5); // InteractableType.Fleet; never manufacture a POI.
    if (!poi || nodeId == 0 || nodeId == -1)
      continue;
    session = {il2cpp_gchandle_new_weakref(context, false), nodeId, *fleet, nodeType, false, false,
               Clock::now() + std::chrono::seconds(5)};
    if (session.context && Select(poi))
      return true;
    Reset();
  }
  return false;
}

bool SuppressAutomaticCargo(RewardsButtonWidget* widget)
{
  if (!session.context || session.cargoRequested || !widget)
    return false;
  auto* data   = widget->Context;
  auto* target = data ? data->TargetFleetDeployedData : nullptr;
  return target && target->ID == session.fleet;
}
} // namespace mining_preview
