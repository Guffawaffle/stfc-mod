#pragma once
#include "boolean_settings.h"

namespace mod_settings
{
bool            QueueRecoveryAvailable();
bool            ThinQueueProtectionAvailable();
bool            QueueAddressGuardAvailable();
bool            KirsharaFixesAvailable();
BooleanSetting& KirsharaFixesSetting();
} // namespace mod_settings
