#pragma once

#include "ui.h"

namespace streamer_mode::game
{
struct PlayerIdDisplay : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.UI", "PlayerIdViewController">;
  TextLocalizer* Label()
  { return Field<"_textLocalizer", TextLocalizer*>(); }
};

struct ResourceAmount : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "ResourceWidget">;
  TextLocalizer* Quantity()
  { return Field<"_quantityLocalizer", TextLocalizer*>(); }
  unity::TMP_Text* CostText()
  { return Field<"_costTextRenderer", unity::TMP_Text*>(); }
};

struct HudResourceBalance : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "HudResourceWidget">;
  ResourceAmount* Resource()
  { return Field<"_resourceWidget", ResourceAmount*>(); }
};

struct ResourceDropdownList : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "Dropdown">;
  unity::Canvas* Canvas()
  { return Field<"_dropdownCanvas", unity::Canvas*>(); }
  unity::RectTransform* Content()
  { return Field<"_content", unity::RectTransform*>(); }
};

struct ResourceDropdown : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "ResourceListDropdownWidget">;
  ResourceDropdownList* List()
  { return Field<"_dropdown", ResourceDropdownList*>(); }
};

struct GenericHudBalance : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "GenericHudResourcesWidget">;
  ResourceAmount* HardResource()
  { return Field<"_hardResource", ResourceAmount*>(); }
  il2cpp::Object* SoftResources()
  { return Field<"_softResources", il2cpp::Object*>(); }
  ResourceDropdown* Dropdown()
  { return Field<"_resourcesListDropdown", ResourceDropdown*>(); }
};

struct ShopShowcaseBalance : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController">;
  unity::GameObject* Resources()
  { return Field<"_resourcesContainer", unity::GameObject*>(); }
};
} // namespace streamer_mode::game
