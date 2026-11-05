#pragma once
#include "fleet_notification_types.h"

// Existing watcher/audio owners publish readiness; Settings does not install
// another watcher or infer availability from a configured nonzero value.
bool FleetNotificationAudioAvailable(FleetNotificationKind kind);
void RefreshFleetNotificationAudio();
bool ToastAudioAvailable();
