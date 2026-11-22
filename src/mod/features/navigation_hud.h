#pragma once

#include "game/hud.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

namespace navigation_hud
{
inline bool Active(HudNavigationViewController* navigation)
{ return unity::Alive(navigation) && navigation->isActiveAndEnabled(); }

inline il2cpp::GcHandle FindNavigation()
{
  // The registered controller is on a visible canvas and active. Root it while
  // the consumer inspects its context or children.
  return il2cpp::GcHandle::Strong(
      ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>());
}

inline il2cpp::GcHandle FindAllianceDrawer()
{
  return il2cpp::GcHandle::Strong(
      ScreenManager::RegisteredController<HudAllianceAndNewsViewController, HudAllianceAndNewsViewControllerContext>());
}

struct DrawerAnchor {
  il2cpp::GcHandle promotion;
  il2cpp::GcHandle chest;
  il2cpp::GcHandle parent;
};

inline DrawerAnchor FindDrawerAnchor(HudAllianceAndNewsViewController* drawer)
{
  if (!unity::Alive(drawer))
    return {};
  DrawerAnchor anchor;
  anchor.promotion = il2cpp::GcHandle::Strong(drawer->chestPromotion());
  auto* promotion  = anchor.promotion.Get<unity::Component>();
  if (!unity::Alive(promotion))
    return {};
  anchor.chest = il2cpp::GcHandle::Strong(promotion->transform());
  auto* chest  = anchor.chest.Get<unity::Transform>();
  if (!unity::Alive(chest))
    return {};
  anchor.parent = il2cpp::GcHandle::Strong(chest->parent());
  return anchor;
}

inline il2cpp::GcHandle FindSystemNames(HudNavigationViewController* navigation)
{
  auto  helper = il2cpp::GcHandle::Strong(navigation->factionInformationHelper());
  auto* data   = helper.Get<FactionInformation>();
  return il2cpp::GcHandle::Strong(data ? data->systemName() : nullptr);
}

inline il2cpp::GcHandle SystemNameObject(unity::Component* name)
{
  auto  root      = il2cpp::GcHandle::Strong(name);
  auto* component = root.Get<unity::Component>();
  return il2cpp::GcHandle::Strong(unity::Alive(component) ? component->gameObject() : nullptr);
}
} // namespace navigation_hud
