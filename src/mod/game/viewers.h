#pragma once

// The object viewer cards shown for targets in space, and their scan, engage and armada buttons.

#include "fleet.h"
#include "navigation.h"
#include "ui.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>

struct ParentObjectViewerViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "ParentObjectViewerViewController">;

  bool IsShowing()
  { return Get<"IsShowing", bool>(); }
};

struct ObjectViewerBaseWidget : Widget<NavigationInteractionUIContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget">;

  VisibilityController* visibilityController()
  { return Field<"_visibilityController", VisibilityController*>(); }
  il2cpp::CallResult<VisibilityController*> TryVisibilityController()
  {
    static const bool available = Class::HasField<"_visibilityController", VisibilityController*>();
    return available ? il2cpp::CallResult<VisibilityController*>{true, visibilityController()}
                     : il2cpp::CallResult<VisibilityController*>{};
  }
  ParentObjectViewerViewController* Parent()
  { return Get<"Parent", ParentObjectViewerViewController*>(); }
  void HideAllViewers()
  { Call<"HideAllViewers">(); }
};

struct RewardsButtonWidget : Widget<BattleTargetData> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget">;

  VisibilityController* rewardsController()
  { return Field<"_rewardsController", VisibilityController*>(); }
  il2cpp::CallResult<VisibilityController*> TryRewardsController()
  {
    static const bool available = Class::HasField<"_rewardsController", VisibilityController*>();
    return available ? il2cpp::CallResult<VisibilityController*>{true, rewardsController()}
                     : il2cpp::CallResult<VisibilityController*>{};
  }
};

struct ScanEngageButtonsWidget : Widget<BattleTargetData> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "ScanEngageButtonsWidget">;

  void OnArmadaButtonClicked()
  { Call<"OnArmadaButtonClicked">(); }
  void OnEngageButtonClicked()
  { Call<"OnEngageButtonClicked">(); }
  void OnScanButtonClicked()
  { Call<"OnScanButtonClicked">(); }
};

struct PreScanTargetWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget">;

  ScanEngageButtonsWidget* scanEngageButtonsWidget()
  { return Field<"_scanEngageButtonsWidget", ScanEngageButtonsWidget*>(); }
  RewardsButtonWidget* rewardsButtonWidget()
  { return Field<"_rewardsButtonWidget", RewardsButtonWidget*>(); }
  il2cpp::CallResult<RewardsButtonWidget*> TryRewardsButton()
  {
    static const bool available = Class::HasField<"_rewardsButtonWidget", RewardsButtonWidget*>();
    return available ? il2cpp::CallResult<RewardsButtonWidget*>{true, rewardsButtonWidget()}
                     : il2cpp::CallResult<RewardsButtonWidget*>{};
  }
  GenericButtonWidget* addToQueueButtonWidget()
  { return Field<"_addToQueueButtonWidget", GenericButtonWidget*>(); }
  GenericButtonWidget* armadaAttackButton()
  { return Field<"_armadaAttackButton", GenericButtonWidget*>(); }
};

struct AllianceStarbaseObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "AllianceStarbaseObjectViewerWidget">;
};

struct ArmadaObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "ArmadaObjectViewerWidget">;

  GenericButtonContext* joinContext()
  { return Field<"_joinContext", GenericButtonContext*>(); }
  void ValidateThenJoinArmada()
  { Call<"ValidateThenJoinArmada">(); }
};

struct CelestialObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "CelestialObjectViewerWidget">;
};

struct EmbassyObjectViewer : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "EmbassyObjectViewer">;
};

struct HousingObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "HousingObjectViewerWidget">;
};

// MiningObjectViewerWidget.OccupiedState, a private nested enum.
enum class OccupiedState : int32_t {
  NotOccupied         = 0,
  LocalPlayerOccupied = 1,
  OtherPlayerOccupied = 2,
};

struct MiningObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "MiningObjectViewerWidget">;

  ScanEngageButtonsWidget* scanEngageButtonsWidget()
  { return Field<"_scanEngageButtonsWidget", ScanEngageButtonsWidget*>(); }
  void MineClicked()
  { Call<"MineClicked">(); }
};

struct MissionsObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "MissionsObjectViewerWidget">;
};

struct StarNodeObjectViewerWidget : ObjectViewerBaseWidget {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.ObjectViewer", "StarNodeObjectViewerWidget">;

  GenericButtonWidget* visitButton()
  { return Field<"_visitButton", GenericButtonWidget*>(); }
  il2cpp::CallResult<GenericButtonWidget*> TryVisitButton()
  {
    static const bool available = Class::HasField<"_visitButton", GenericButtonWidget*>();
    return available ? il2cpp::CallResult<GenericButtonWidget*>{true, visitButton()}
                     : il2cpp::CallResult<GenericButtonWidget*>{};
  }
  void OnViewButtonActivation()
  { Call<"OnViewButtonActivation">(); }
  void InitiateWarp()
  { Call<"InitiateWarp">(); }
};
