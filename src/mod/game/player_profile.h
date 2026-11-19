#pragma once

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

struct PlayerProfileDataContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "PlayerProfileDataContext">;
};

struct PlayerProfileViewController : unity::Behaviour {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.PlayerProfile", "PlayerProfileViewController">;

  unity::Component* avatarEditButton()
  { return Field<"_avatarEditButton", unity::Component*>(); }
};
