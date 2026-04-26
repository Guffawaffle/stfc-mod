#pragma once

#include "patches/live_debug_ui_observations.h"

#include <nlohmann/json.hpp>

const char* incoming_attack_target_type_name(int target_type);
const char* navigation_context_data_state_name(int state);
const char* input_interaction_type_name(int type);
const char* navigation_threat_level_name(int level);

nlohmann::json top_canvas_observation_to_json(const TopCanvasObservation& observation);
nlohmann::json station_warning_observation_to_json(const StationWarningObservation& observation);
nlohmann::json navigation_interaction_entry_to_json(const NavigationInteractionObservation::Entry& entry);
nlohmann::json navigation_interaction_observation_to_json(const NavigationInteractionObservation& observation);