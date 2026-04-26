/**
 * @file live_debug_state_results.h
 * @brief Result builders for simple live-debug state request commands.
 */
#pragma once

#include <nlohmann/json.hpp>

namespace live_debug_state_results {
nlohmann::json Ping();
nlohmann::json TrackerList();
nlohmann::json TopCanvas();
nlohmann::json FleetbarState();
nlohmann::json FleetSlotsState();
nlohmann::json MineViewerState();
nlohmann::json TargetViewerState();
}