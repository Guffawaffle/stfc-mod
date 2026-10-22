#pragma once

struct FleetPlayerData;

namespace discovery_tow
{
// The native Unity scheduler owns all IEnumerator.Current/nested-yield waits.
void Begin(FleetPlayerData* selected, FleetPlayerData* discovery, int selected_index, int discovery_index);
void Update();
void Cancel();
} // namespace discovery_tow
