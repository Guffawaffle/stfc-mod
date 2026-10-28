#pragma once

// System and galaxy navigation: camera zoom and pan, LOD, sections, bookmarks, course prompts.

#include "app.h"
#include "fleet.h"
#include "ui.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>

enum class ZoomLevels : int32_t {
  None   = 0,
  Near   = 1,
  Middle = 2,
  Far    = 3,
};

struct NavigationLOD : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationLOD">;

  // Writes the backing field directly; the managed setter is private.
  void SetTargetLevel(ZoomLevels level)
  { SetField<"<TargetLevel>k__BackingField">(level); }
  void UpdateLOD(ZoomLevels level)
  { Call<"UpdateLOD">(level); }
};

struct NavigationPan : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPan">;

  unity::Vector2 lastDelta()
  { return Field<"_lastDelta", unity::Vector2>(); }
  void SetLastDelta(unity::Vector2 value)
  { SetField<"_lastDelta">(value); }
  float deltaScaler()
  { return Field<"_deltaScaler", float>(); }
  float keyboardDeltaScaler()
  { return Field<"_keyboardDeltaScaler", float>(); }
  // Managed POI; only checked for null or cleared.
  void* trackingPOI()
  { return Field<"_trackingPOI", void*>(); }
  void SetTrackingPOI(void* value)
  { SetField<"_trackingPOI">(value); }

  float farMagRadiusRatioSystemNormal()
  { return Field<"_farMagRadiusRatioSystemNormal", float>(); }
  void SetFarMagRadiusRatioSystemExtended(float value)
  { SetField<"_farMagRadiusRatioSystemExtended">(value); }

  void MoveCamera(unity::Vector2 delta, bool isMomentum = false)
  { Call<"MoveCamera">(delta, isMomentum); }

  static bool BlockPan()
  { return Class::Field<"BlockPan", bool>(); }
};

enum class NodeDepth : int32_t {
  Galaxy       = 1,
  SolarSystem  = 2,
  PlanetSystem = 4,
  Starbase     = 8,
};

struct MathUtils {
  using Class = il2cpp::Class<"Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils">;

  // The world point under the screen position `mouse`, as seen by `camera`.
  static unity::Vector3 GetMouseWorldPos(unity::Camera* camera, unity::Vector3 mouse)
  { return Class::Call<"GetMouseWorldPos", unity::Vector3>(camera, mouse); }
};

struct NavigationZoom : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom">;

  float Distance()
  { return Get<"Distance", float>(); }
  void SetDistance(float value)
  { Set<"Distance">(value); }
  float NormalizedZoom()
  { return Get<"NormalizedZoom", float>(); }

  NodeDepth depth()
  { return Field<"_depth", NodeDepth>(); }
  ZoomLevels zoomLevel()
  { return Field<"_zoomLevel", ZoomLevels>(); }
  unity::Camera* sceneCamera()
  { return Field<"_sceneCamera", unity::Camera*>(); }

  void SetZoomtotal(float value)
  { SetField<"_zoomtotal">(value); }
  float minimum()
  { return Field<"_minimum", float>(); }
  float maximum()
  { return Field<"_maximum", float>(); }
  void SetMaximum(float value)
  { SetField<"_maximum">(value); }
  void SetZoomDelta(float value)
  { SetField<"_zoomDelta">(value); }
  float viewRadius()
  { return Field<"_viewRadius", float>(); }
  void SetLastZoomDelta(float value)
  { SetField<"_lastZoomDelta">(value); }
  void SetZoomLocation(unity::Vector2 value)
  { SetField<"_zoomLocation">(value); }
  void SetWorldPoint(unity::Vector3 value)
  { SetField<"_worldPoint">(value); }
  void SetFarRatioSystemNormal(float value)
  { SetField<"_farRatioSystemNormal">(value); }
  void SetFarRatioSystemExtended(float value)
  { SetField<"_farRatioSystemExtended">(value); }

  void ZoomCameraAtWorldPoint()
  { Call<"ZoomCameraAtWorldPoint">(); }
};

struct NavigationFleetWidget : Widget<FleetDeployedData> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationFleetWidget">;

  NavigationLOD* lod()
  { return Field<"_lod", NavigationLOD*>(); }
  il2cpp::CallResult<TextLocalizer*> TryFleetLevel()
  {
    static const bool available = Class::HasField<"_fleetLevel", TextLocalizer*>();
    return available ? il2cpp::CallResult<TextLocalizer*>{true, Field<"_fleetLevel", TextLocalizer*>()}
                     : il2cpp::CallResult<TextLocalizer*>{};
  }
};

struct NavigationTransitionAnimator : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationTransitionAnimator">;
  il2cpp::CallResult<unity::Vector3> TryWorldToDepth(unity::Vector3 position, int32_t depth)
  {
    static const auto* method = [] {
      const auto* candidate = il2cpp::detail::MethodCall<unity::Vector3(unity::Vector3, int32_t)>::Resolve(
          Class::Raw(), "WorldToDepth", false);
      if (candidate
          && (!il2cpp::detail::ValueClassMatches(candidate->parameters[0], unity::Vector3Class(),
                                                 sizeof(unity::Vector3))
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[1], IL2CPP_TYPE_I4))) {
        il2cpp::Report("[IL2CPP] NavigationTransitionAnimator.WorldToDepth requires an exact Vector3 input");
        return static_cast<const MethodInfo*>(nullptr);
      }
      return candidate;
    }();
    void* args[] = {&position, &depth};
    return unity::TryReadVector3(method, this, args);
  }
};
struct FleetNavigationHelper : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "FleetNavigationHelper">;
  il2cpp::CallResult<bool> TryValidateInput(int64_t fleet_id, LocaleTextContext*& message)
  {
    static const auto* method = [] {
      const auto* candidate = il2cpp::detail::MethodCall<bool(int64_t, LocaleTextContext*&)>::Resolve(
          Class::Raw(), "NavigationValidateInput", false);
      if (candidate
          && (!candidate->methodPointer
              || (candidate->has_full_generic_sharing_signature ? !candidate->invoker_method
                                                                : !candidate->virtualMethodPointer)
              || (candidate->flags & METHOD_ATTRIBUTE_VIRTUAL)
              || !il2cpp::detail::ExactScalarMatches(candidate->return_type, IL2CPP_TYPE_BOOLEAN)
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[0], IL2CPP_TYPE_I8)
              || !candidate->parameters[1]->byref
              || il2cpp_class_from_type(candidate->parameters[1]) != LocaleTextContext::Class::Raw())) {
        il2cpp::Report("[IL2CPP] FleetNavigationHelper.NavigationValidateInput signature is unavailable");
        return static_cast<const MethodInfo*>(nullptr);
      }
      return candidate;
    }();
    return il2cpp::detail::TryInvoke<bool>(method, this, fleet_id, std::ref(message));
  }
};
struct NavigationInteractionUIViewController;
struct NavigationManager : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager">;

  void HideInteraction()
  { Call<"HideInteraction">(); }
  il2cpp::CallResult<NavigationInteractionUIViewController*> TryInteractionController();
  il2cpp::CallResult<NavigationTransitionAnimator*>          TryTransitionAnimator()
  {
    static const bool available = Class::HasField<"_transitionAnimator", NavigationTransitionAnimator*>();
    return available ? il2cpp::CallResult<NavigationTransitionAnimator*>{true, Field<"_transitionAnimator",
                                                                                     NavigationTransitionAnimator*>()}
                     : il2cpp::CallResult<NavigationTransitionAnimator*>{};
  }
  // Shared qualification for invocation and the manual-move intent hook.
  // Resolve metadata only: no game/engine call during hook installation.
  static il2cpp::Method MoveFleetActionMethod()
  {
    using Position            = il2cpp::Nullable<unity::Vector3>;
    static const auto* method = [] {
      const auto* candidate = il2cpp::detail::MethodCall<void(Position, int64_t, int64_t, bool)>::Resolve(
          Class::Raw(), "OnMoveFleetAction", false);
      if (candidate
          && (!candidate->methodPointer || !candidate->invoker_method || candidate->return_type->byref
              || (candidate->flags & METHOD_ATTRIBUTE_VIRTUAL)
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[1], IL2CPP_TYPE_I8)
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[2], IL2CPP_TYPE_I8)
              || !il2cpp::detail::ExactScalarMatches(candidate->parameters[3], IL2CPP_TYPE_BOOLEAN)
              || !il2cpp::detail::NullableLayoutMatches(candidate->parameters[0], unity::Vector3Class(),
                                                        sizeof(Position), alignof(Position),
                                                        offsetof(Position, has_value), offsetof(Position, value)))) {
        il2cpp::Report("[IL2CPP] NavigationManager.OnMoveFleetAction nullable layout or dispatch is unavailable");
        return static_cast<const MethodInfo*>(nullptr);
      }
      return candidate;
    }();
    return il2cpp::Method(method);
  }
  il2cpp::CallResult<void> TryMovePlanned(unity::Vector3 position, int64_t node_id, int64_t fleet_id)
  {
    using Position     = il2cpp::Nullable<unity::Vector3>;
    const auto* method = MoveFleetActionMethod().Info();
    if (!method)
      return {};
    Position   destination{true, position};
    bool       force_move = false;
    void*      args[]     = {&destination, &node_id, &fleet_id, &force_move};
    const auto observed   = il2cpp::detail::TryInvokeBoxed(method, this, args);
    return {observed.succeeded};
  }
};

// A plain MonoBehaviour with its own static Instance, not a MonoSingleton.
struct NavigationSectionManager : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationSectionManager">;

  // Named after the property, so the type needs `::` here.
  ::NavigationManager* NavigationManager()
  { return Get<"NavigationManager", ::NavigationManager*>(); }

  static NavigationSectionManager* Instance()
  { return Class::Get<"Instance", NavigationSectionManager*>(); }
  static void ChangeNavigationSection(SectionID sectionId, int64_t systemId = -1)
  { Class::Call<"ChangeNavigationSection">(sectionId, systemId); }

  // Closes whatever interaction menu is open in space before the mod acts on a fleet.
  static void HideInteraction()
  {
    auto* section = Instance();
    if (auto* navigation = section ? section->NavigationManager() : nullptr)
      navigation->HideInteraction();
  }
};

struct NavigationInteractionUIContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIContext">;
  il2cpp::CallResult<ITreeNode*> TryTreeNode()
  { return planned_warp_binding::Reference<"get_TreeNode", ITreeNode>(this); }
  il2cpp::CallResult<PathFinderResultData*> TryPath()
  {
    static const bool available = Class::HasField<"PathFindResult", PathFinderResultData*>();
    return available ? il2cpp::CallResult<PathFinderResultData*>{true, Field<"PathFindResult", PathFinderResultData*>()}
                     : il2cpp::CallResult<PathFinderResultData*>{};
  }
  il2cpp::CallResult<unity::Vector3> TryPosition()
  {
    static auto* field = [] {
      auto* candidate =
          il2cpp::detail::ResolveField(Class::Raw(), "Position", il2cpp::detail::SlotOf<unity::Vector3>(), false);
      if (candidate
          && ((candidate->type->attrs & FIELD_ATTRIBUTE_LITERAL)
              || !il2cpp::detail::ValueClassMatches(candidate->type, unity::Vector3Class(), sizeof(unity::Vector3)))) {
        il2cpp::Report("[IL2CPP] NavigationInteractionUIContext.Position requires an exact Vector3 field");
        return static_cast<FieldInfo*>(nullptr);
      }
      return candidate;
    }();
    return field ? il2cpp::CallResult<unity::Vector3>{true, il2cpp::detail::ReadField<unity::Vector3>(field, this)}
                 : il2cpp::CallResult<unity::Vector3>{};
  }
};

struct SetCourseWidget : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "SetCourseWidget">;

  VisibilityController* visibilityController()
  { return Field<"_visibilityController", VisibilityController*>(); }
};

struct NavigationInteractionUIViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController">;

  bool IsSetCourseVisible()
  {
    auto* widget     = Field<"_setCourseWidget", SetCourseWidget*>();
    auto* visibility = widget ? widget->visibilityController() : nullptr;
    return visibility && visibility->IsShown() && widget->isActiveAndEnabled();
  }
  void OnSetCourseButtonClick()
  { Call<"OnSetCourseButtonClick">(); }
  il2cpp::CallResult<NavigationInteractionUIContext*> TryContext()
  { return planned_warp_binding::Reference<"get_CanvasContext", NavigationInteractionUIContext>(this); }
  il2cpp::CallResult<FleetNavigationHelper*> TryFleetHelper()
  {
    static const bool available = Class::HasField<"_fleetNavHelper", FleetNavigationHelper*>();
    return available
               ? il2cpp::CallResult<FleetNavigationHelper*>{true, Field<"_fleetNavHelper", FleetNavigationHelper*>()}
               : il2cpp::CallResult<FleetNavigationHelper*>{};
  }
};
inline il2cpp::CallResult<NavigationInteractionUIViewController*> NavigationManager::TryInteractionController()
{
  return planned_warp_binding::Reference<"get_NavigationInteractionUIViewController",
                                         NavigationInteractionUIViewController>(this);
}

struct PopulatedSystemData : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "PopulatedSystemData">;

  Il2CppObject* VariationCollection()
  { return Field<"<VariationCollection>k__BackingField", Il2CppObject*>(); }
};

struct PlanetViewUtils : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils">;

  PopulatedSystemData* popData()
  { return Field<"_popData", PopulatedSystemData*>(); }

  // The planet's 2D sprite, a Component. Calling this also runs any detour on get_FlatRenderable.
  unity::Component* FlatRenderable()
  { return static_cast<unity::Component*>(Get<"FlatRenderable", Il2CppObject*>()); }
};

struct CoordinateSearchContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Bookmarks", "CoordinateSearchContext">;

  void SetDefaultXCoordinate(Il2CppString* value)
  { Set<"DefaultXCoordinate">(value); }
  void SetDefaultYCoordinate(Il2CppString* value)
  { Set<"DefaultYCoordinate">(value); }
  void SetDefaultSystemInput(Il2CppString* value)
  { Set<"DefaultSystemInput">(value); }
};

struct BookmarksManager : MonoSingleton<BookmarksManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Bookmarks", "BookmarksManager">;

  void ViewBookmarks()
  { Call<"ViewBookmarks">(); }

  void ViewCoordinateSearch()
  {
    auto* context = il2cpp::New<CoordinateSearchContext>();
    // The binding already reported the missing class or constructor; plain bookmarks are the next best view.
    if (!context) {
      ViewBookmarks();
      return;
    }

    auto* zero = il2cpp::NewString("0");
    context->SetDefaultXCoordinate(zero);
    context->SetDefaultYCoordinate(zero);
    context->SetDefaultSystemInput(il2cpp::NewString(""));
    Hub::GotoSection(SectionID::Bookmarks_Search_Coordinates, context);
  }
};

struct CoursePromptPopupDataContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "CoursePromptPopupDataContext">;

  bool HasInstantWarp()
  { return Field<"HasInstantWarp", bool>(); }
  il2cpp::CallResult<bool> TryHasInstantWarp()
  {
    static const bool available = Class::HasField<"HasInstantWarp", bool>();
    return available ? il2cpp::CallResult<bool>{true, HasInstantWarp()} : il2cpp::CallResult<bool>{};
  }
  // A public field, named after it, so the type needs `::` here.
  ::CourseData* CourseData()
  { return Field<"CourseData", ::CourseData*>(); }
  il2cpp::CallResult<::CourseData*> TryCourseData()
  {
    static const bool available = Class::HasField<"CourseData", ::CourseData*>();
    return available ? il2cpp::CallResult<::CourseData*>{true, CourseData()} : il2cpp::CallResult<::CourseData*>{};
  }
};

struct CoursePromptPopupWidget : Widget<CoursePromptPopupDataContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation", "CoursePromptPopupWidget">;

  void InitiateRegularWarp()
  { Call<"InitiateRegularWarp">(); }
  void OnInstantWarpButtonClick()
  { Call<"OnInstantWarpButtonClick">(); }
};

struct CoursePromptPopupViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "CoursePromptPopupViewController">;

  CoursePromptPopupWidget* coursePromptPopupWidget()
  { return Field<"_coursePromptPopupWidget", CoursePromptPopupWidget*>(); }
  il2cpp::CallResult<CoursePromptPopupWidget*> TryPopupWidget()
  {
    static const bool available = Class::HasField<"_coursePromptPopupWidget", CoursePromptPopupWidget*>();
    return available ? il2cpp::CallResult<CoursePromptPopupWidget*>{true, coursePromptPopupWidget()}
                     : il2cpp::CallResult<CoursePromptPopupWidget*>{};
  }
};

// TouchKit's touch; a held touch reports Stationary, which the pan ignores.
struct TKTouch : il2cpp::Object {
  using Class = il2cpp::Class<"TouchKit", "", "TKTouch">;

  unity::TouchPhase phase()
  { return Field<"phase", unity::TouchPhase>(); }
  void SetPhase(unity::TouchPhase phase)
  { SetField<"phase">(phase); }
};
