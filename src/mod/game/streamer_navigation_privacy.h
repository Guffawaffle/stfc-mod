#pragma once

// Authored presentation fields for the identity and location masks. No profile, location or display text is read.
#include "fleet.h"
#include "ui.h"
#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace streamer_navigation_binding
{
using Targets = std::vector<il2cpp::GcHandle>;

inline bool AppendPrivacyTarget(Targets& targets, unity::GameObject* object)
{
  const auto alive = unity::TryAlive(object);
  if (!alive || !alive.value)
    return alive.succeeded;
  if (std::ranges::any_of(targets, [object](const auto& target) { return target.Get() == object; }))
    return true;
  auto root = il2cpp::GcHandle::Strong(object);
  if (!root)
    return false;
  targets.push_back(std::move(root));
  return true;
}

inline bool AppendPrivacyTarget(Targets& targets, il2cpp::Object* component)
{
  if (!component)
    return true;
  auto root = il2cpp::GcHandle::Strong(component);
  if (!root)
    return false;
  const auto alive = unity::TryAlive(root.Get());
  if (!alive || !alive.value)
    return alive.succeeded; // Serialized optional components may retain destroyed managed wrappers.
  // GraphicLocalizer resolves its Graphic through GetComponent on this same GameObject.
  const auto object = root.Get<il2cpp::Object>()->TryCall<"get_gameObject", unity::GameObject*>();
  return object && object.value && AppendPrivacyTarget(targets, object.value);
}

template <typename FieldType, il2cpp::Name... Names> struct PrivacyFields {
  template <typename Class> static bool Available()
  { return (Class::template HasField<Names, FieldType*>() && ...); }

  template <typename Owner> static bool Collect(Owner* owner, Targets& targets)
  {
    bool complete = true;
    ((complete = AppendPrivacyTarget(targets, owner->template Field<Names, FieldType*>()) && complete), ...);
    return complete;
  }
};

template <il2cpp::Name... Names> using Components = PrivacyFields<unity::Component, Names...>;
template <il2cpp::Name... Names> using Objects    = PrivacyFields<unity::GameObject, Names...>;

template <il2cpp::Name Namespace, il2cpp::Name Type, typename... Fields> struct PrivacyView : il2cpp::Object {
  using Class                                    = il2cpp::Class<"Assembly-CSharp", Namespace, Type>;
  static constexpr std::string_view surface_name = Type.View();

  static bool Available()
  { return (Fields::template Available<Class>() && ...); }

  bool Collect(Targets& targets)
  {
    bool complete = true;
    ((complete = Fields::Collect(this, targets) && complete), ...);
    return complete;
  }
};

using UserProfileWidget = PrivacyView<"Digit.Prime.PlayerProfile", "UserProfileWidget",
                                      Components<"_name", "_serverName", "_avatar", "_profileFrameAndAvatar",
                                                 "_serverRivalWidget", "_playerTitleWidget">>;
using PlayerProfileAllianceAreaWidget = PrivacyView<"Digit.Prime.PlayerProfile", "PlayerProfileAllianceAreaWidget",
                                                    Components<"_allianceName", "_allianceTag", "_allianceIcon">>;

struct NavigationFleetWidget
    : PrivacyView<"Digit.Prime.Navigation", "NavigationFleetWidget",
                  Components<"_fleetName", "_fleetNameUIText", "_tagNameTextLocalizer", "_tagNameUIText">> {
  static bool Available()
  {
    return PrivacyView::Available() && Class::HasProperty<"Context", FleetDeployedData*>()
           && FleetDeployedData::Class::HasProperty<"FleetType", DeployedFleetType>();
  }

  bool Collect(Targets& targets)
  {
    const auto context = TryCall<"get_Context", FleetDeployedData*>();
    if (context && context.value) {
      const auto type = context.value->TryFleetType();
      if (type && type.value != DeployedFleetType::Player && type.value != DeployedFleetType::Alliance)
        return true; // Preserve current NPC and faction labels.
    }
    // An unreadable or still-binding context can carry the previous player's label.
    return PrivacyView::Collect(targets);
  }
};

using NavigationStarbaseWidget = PrivacyView<"Digit.Prime.Navigation", "NavigationStarbaseWidget",
                                             Components<"_playerName", "_playerNameUIText", "_allianceName">>;
using NavigationGalaxyFleetWidget =
    PrivacyView<"Digit.Prime.Navigation", "NavigationGalaxyFleetWidget", Components<"_playerName">>;
using NavigationStarEntityWidget =
    PrivacyView<"Digit.Prime.Navigation", "NavigationStarEntityWidget",
                Components<"_starName", "_homeRegionalSystemTextLocalizer", "_regionalSystemTextLocalizer">,
                Objects<"_homeIcon", "_isRegionalSpaceSystemCallout", "_isHomeRegionalSystemCallout">>;

struct FactionInformation : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FactionInformation">;
  static constexpr std::string_view surface_name = "FactionInformation";

  static bool Available()
  { return Class::HasField<"_systemName", il2cpp::Object*>(); }

  bool Collect(Targets& targets)
  {
    auto* observed = Field<"_systemName", il2cpp::Object*>();
    if (!observed)
      return true;
    auto list = il2cpp::GcHandle::Strong(observed);
    if (!list)
      return false;
    const auto count = list.Get<il2cpp::Object>()->TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return false;
    bool complete = true;
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item = list.Get<il2cpp::Object>()->TryCall<"get_Item", TextLocalizer*(int32_t)>(index);
      complete        = item && AppendPrivacyTarget(targets, item.value) && complete;
    }
    return complete;
  }
};

using HUDHomeLocatorAndFleetCommanderViewController =
    PrivacyView<"Digit.Prime.HUD", "HUDHomeLocatorAndFleetCommanderViewController", Components<"_homePointer">>;
using ViewLocationWidget = PrivacyView<"Digit.Prime.Navigation.UI", "ViewLocationWidget", Components<"_name">>;
using BookmarkWidget     = PrivacyView<"Digit.Client.UI", "BookmarkWidget", Components<"_coordinateText">>;
using BookmarkListWidget =
    PrivacyView<"Digit.Prime.Bookmarks", "BookmarkListWidget", Components<"_name", "_systemName", "_coordinates">>;
using BookmarkDetailsViewController = PrivacyView<"Digit.Prime.Bookmarks", "BookmarkDetailsViewController",
                                                  Components<"_systemName", "_coordinates", "_nameInputField">>;
} // namespace streamer_navigation_binding
