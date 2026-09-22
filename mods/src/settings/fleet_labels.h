#pragma once
#include "choice_setting.h"
#include "boolean_settings.h"
#include "slider_setting.h"
namespace mod_settings
{
BooleanSetting& ShipHotkeyBadgesSetting();
ChoiceSetting& FleetLabelDetailSetting(bool player);
SliderSetting& FleetLabelThresholdSetting(bool player);
std::string    FleetLabelSummary(bool player);
} // namespace mod_settings
// Existing zoom adapter owns native fleet labels and reports installation.
bool FleetLabelControlsAvailable();
void RefreshFleetLabelControls();
