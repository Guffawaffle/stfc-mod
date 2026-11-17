#pragma once
#include "boolean_settings.h"

namespace mod_settings
{
BooleanSetting& FleetCommanderConfirmationSetting();
BooleanSetting& ForbiddenTechConfirmationSetting();
void            InvalidateConfirmationSettings();
} // namespace mod_settings
