/**
 * @file hostile_observation_sidecar_sync.h
 * @brief Sidecar-local observed hostile event bridge.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>

bool hostile_observation_sidecar_delivery_enabled();
void hostile_observation_sidecar_emit(const nlohmann::json& observation);
