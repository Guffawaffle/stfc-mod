#pragma once

struct NavigationFleetWidget;

namespace ship_shortcut_badges
{
// Called by the existing fleet-widget owner; these functions install no hooks.
void Bind(NavigationFleetWidget* widget);
void Release(NavigationFleetWidget* widget);
void Refresh();
} // namespace ship_shortcut_badges
