#pragma once

#include "chat.h"
#include "server.h"

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

struct PlayerProfileDataContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "PlayerProfileDataContext">;
};

struct IVisibilityHandler : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "IVisibilityHandler">;
};

struct PlayerProfileAllianceAreaWidget : unity::Component {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "PlayerProfileAllianceAreaWidget">;

  TextLocalizer* allianceName()
  { return Field<"_allianceName", TextLocalizer*>(); }
  TextLocalizer* allianceTag()
  { return Field<"_allianceTag", TextLocalizer*>(); }
  unity::Component* allianceIcon()
  { return Field<"_allianceIcon", unity::Component*>(); }
};

struct PlayerProfileViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "PlayerProfileViewController">;

  unity::Component* avatarEditButton()
  { return Field<"_avatarEditButton", unity::Component*>(); }
  UserProfileWidget* playerInfoWidget()
  { return Field<"_playerInfoWidget", UserProfileWidget*>(); }
  PlayerProfileAllianceAreaWidget* profileAllianceWidget()
  { return Field<"_profileAllianceWidget", PlayerProfileAllianceAreaWidget*>(); }

  // AboutToShow binds this exact model, then uses its IsLocalPlayer flag for the native profile UI.
  il2cpp::CallResult<bool> TryIsLocalProfile()
  {
    auto* profile = Field<"_userProfile", UserProfile*>();
    return profile && profile->Is<UserProfile>() ? profile->TryCall<"get_IsLocalPlayer", bool>()
                                                 : il2cpp::CallResult<bool>{};
  }
};
