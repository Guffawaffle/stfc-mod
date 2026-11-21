#pragma once

#include "ui.h"

// Serialized display components owned by native report and toast prefabs. These
// bindings do not write contexts, participants, result data or navigation.
template <typename Owner, il2cpp::Name... Fields> struct StreamerReportFields {
  static bool Available()
  { return (Owner::Class::template HasField<Fields, unity::Component*>() && ...); }

  static void Visit(Owner* owner, auto&& visitor)
  {
    if (owner)
      (visitor(owner->template Field<Fields, unity::Component*>()), ...);
  }
};

struct StreamerBattleReportRow : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inbox", "BattleReportRowWidget">;
  using Identity = StreamerReportFields<StreamerBattleReportRow, "_playerFleetName", "_playerStationName",
                                        "_allianceStarbaseName", "_playerFallbackName", "_playerUserProfileWidget",
                                        "_enemyUserProfileWidget", "_enemyAllianceProfileWidget">;
};

struct StreamerDestructionReportRow : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Inbox", "DestructionReportRowWidget">;
  using Identity = StreamerReportFields<StreamerDestructionReportRow, "_fleetName">;
  using Location = StreamerReportFields<StreamerDestructionReportRow, "_destroyedSystemName">;
};

struct StreamerReportTarget : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "TargetInfoWidget">;
  using Identity = StreamerReportFields<StreamerReportTarget, "_userProfileWidget", "_allianceProfileWidget",
                                        "_enemyStarbaseName", "_enemyAllianceStarbaseName">;
};

struct StreamerReportMultiTarget : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "MultiTargetInfoWidget">;
  using Identity = StreamerReportFields<StreamerReportMultiTarget, "_userProfileWidget", "_allianceProfileWidget",
                                        "_shipHeaderLabel">;

  StreamerReportTarget* SingleTarget()
  { return Field<"_singleTargetInfoWidget", StreamerReportTarget*>(); }
};

struct StreamerReportSystem : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Factions", "SystemInformationWidget">;
  // The game's serialized member uses this spelling.
  using Location = StreamerReportFields<StreamerReportSystem, "_sytemName">;
};

struct StreamerBattleReport : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "BattleReportViewController">;

  StreamerReportSystem* SystemInformation()
  { return Field<"_systemNameWidget", StreamerReportSystem*>(); }

  static bool TargetsAvailable()
  {
    return Class::HasField<"_systemNameWidget", StreamerReportSystem*>()
           && Class::HasField<"_playerInfoWidget", StreamerReportTarget*>()
           && Class::HasField<"_singleTargetInfoWidget", StreamerReportTarget*>()
           && Class::HasField<"_multiPlayerInfoWidget", StreamerReportMultiTarget*>()
           && Class::HasField<"_multiTargetInfoWidget", StreamerReportMultiTarget*>()
           && StreamerReportMultiTarget::Class::HasField<"_singleTargetInfoWidget", StreamerReportTarget*>();
  }

  void VisitTargets(auto&& visit_single, auto&& visit_multi)
  {
    visit_single(Field<"_playerInfoWidget", StreamerReportTarget*>());
    visit_single(Field<"_singleTargetInfoWidget", StreamerReportTarget*>());
    visit_multi(Field<"_multiPlayerInfoWidget", StreamerReportMultiTarget*>());
    visit_multi(Field<"_multiTargetInfoWidget", StreamerReportMultiTarget*>());
  }
};

struct StreamerBattleLogEntry : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "BattleLogGenericEntryDetailWidget">;
  using Identity = StreamerReportFields<StreamerBattleLogEntry, "_playerName", "_opponentName">;
};

struct StreamerReportTargetRow : il2cpp::Object {
  using Class    = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Combat", "TargetRowWidget">;
  using Identity = StreamerReportFields<StreamerReportTargetRow, "_playerName", "_starbaseName">;

  static bool OwnerAvailable()
  {
    return Class::HasField<"_parentWidget", StreamerReportMultiTarget*>()
           && StreamerReportMultiTarget::Class::HasField<"_scanTargetController", unity::Component*>();
  }

  bool IsReportRow()
  {
    auto parent = il2cpp::GcHandle::Strong(Field<"_parentWidget", StreamerReportMultiTarget*>());
    if (!parent)
      return false;
    auto* controller = parent.Get<StreamerReportMultiTarget>()->Field<"_scanTargetController", unity::Component*>();
    return controller && controller->As<StreamerBattleReport>();
  }
};

struct StreamerBattleToast : il2cpp::Object {
  using Class   = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "BattleResultToastWidget">;
  using Details = StreamerReportFields<StreamerBattleToast, "_fleetName", "_enemyName", "_allianceName">;
};

struct StreamerIncomingToast : il2cpp::Object {
  using Class   = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "IncomingAttackToastWidget">;
  using Details = StreamerReportFields<StreamerIncomingToast, "_enemyNameWidget">;
};

struct StreamerArmadaToast : il2cpp::Object {
  using Class   = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "ArmadaToastWidget">;
  using Details = StreamerReportFields<StreamerArmadaToast, "_label", "_topLabel", "_bottomLabel">;
};

struct StreamerStandardToast : il2cpp::Object {
  using Class   = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "StandardToastWidget">;
  using Details = StreamerReportFields<StreamerStandardToast, "_label">;
};
