/**
 * @file hostile_observation_fleet_data_hooks.h
 * @brief Lower-level hostile observation from loaded fleet data events.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>

bool           hostile_observation_fleet_data_enabled();
nlohmann::json hostile_observation_fleet_data_state();
void           InstallHostileObservationFleetDataHooks();
