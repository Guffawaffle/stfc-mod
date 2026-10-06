#pragma once
#include "boolean_settings.h"
#include "slider_setting.h"

namespace mod_settings
{
SliderSetting&  KeyboardZoomSpeedSetting();
SliderSetting&  PanGlideSetting();
SliderSetting&  HavenZoomSetting();
BooleanSetting& HavenWaterSetting();
bool            HavenCameraControlAvailable();
bool            HavenWaterControlAvailable();
bool            KeyboardZoomControlAvailable();
bool            PanGlideControlAvailable();
} // namespace mod_settings
