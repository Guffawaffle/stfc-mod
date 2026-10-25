#pragma once

// The HUD around the space views: the mission buttons, the faction and system panel, the alliance drawer, and the
// galactic anomaly the viewed system may have.

#include "fleet.h"
#include "ui.h"

#include <il2cpp/il2cpp.h>

#include <cstdint>

// The Q-Trials, field training, outposts and missions buttons.
struct MissionsHudViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "MissionsHudViewController">;

  // The buttons are a mix of Button and GenericButtonWidget fields; only their GameObject matters to the mod.
  template <il2cpp::Name N> unity::Component* ButtonField()
  { return Field<N, unity::Component*>(); }

  bool TryRefreshAchievements()
  { return TryCall<"SetupAchievementsButton", void()>().succeeded; }
  bool TryRefreshOutposts()
  { return TryCall<"HandleOutpostsAndChallengesHUD", void()>().succeeded; }
  bool ActiveForRefresh()
  {
    const auto observed = TryCall<"get_isActiveAndEnabled", bool>();
    return observed && observed.value;
  }
};

struct FactionInformation : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "FactionInformation">;

  // TextLocalizer components, one per system name label.
  il2cpp::List<unity::Component*>* systemName()
  { return Field<"_systemName", il2cpp::List<unity::Component*>*>(); }
};

// Galactic anomalies and navigation view data are read on their runtime classes: the mod only needs a few members,
// and these classes have moved between client builds.

// Digit.PrimeServer.Models.GalacticAnomaly
struct GalacticAnomaly : il2cpp::Object {
  bool IsActive()
  { return Get<"IsActive", bool>(); }
  TimerDataContext* EndTimeTimerDataContext()
  { return Get<"EndTimeTimerDataContext", TimerDataContext*>(); }
};

struct NavigationViewData : il2cpp::Object {
  NodeAddress* ViewingAddress()
  { return Get<"ViewingAddress", NodeAddress*>(); }
};

struct HudNavigationViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Navigation.UI", "HudNavigationViewController">;

  unity::Component* galacticAnomaliesButtonWidget()
  { return Field<"_galacticAnomaliesButtonWidget", unity::Component*>(); }
  FactionInformation* factionInformationHelper()
  { return Field<"_factionInformationHelper", FactionInformation*>(); }
  bool CanShowGalacticAnomaly()
  { return Call<"CanShowGalacticAnomaly", bool>(); }
  NavigationViewData* CanvasContext()
  { return Get<"CanvasContext", NavigationViewData*>(); }
};

// The upper-right drawer with Help All, Engage and Claim.
struct HudAllianceAndNewsViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "HudAllianceAndNewsViewController">;

  unity::Component* chestPromotion()
  { return Field<"_chestPromotion", unity::Component*>(); }
};

struct GalacticAnomaliesManager : MonoSingleton<GalacticAnomaliesManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GalacticAnomalies", "GalacticAnomaliesManager">;

  GalacticAnomaly* GetSystemGalacticAnomalies(int64_t system)
  { return Call<"GetSystemGalacticAnomalies", GalacticAnomaly*>(system); }

  // The singleton MonoSingleton`1 already holds, or null when there is none or it was destroyed. Instance would
  // search the scene, or create a replacement, when there is none.
  static GalacticAnomaliesManager* Current()
  {
    auto* manager = Class::Field<"s_instance", GalacticAnomaliesManager*>();
    return unity::Alive(manager) ? manager : nullptr;
  }
};
