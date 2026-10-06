#pragma once
#include "boolean_settings.h"

namespace mod_settings
{
bool            QueueRecoveryAvailable();
void            ResetQueueRecovery();
BooleanSetting& QueueRecoverySetting();
} // namespace mod_settings
