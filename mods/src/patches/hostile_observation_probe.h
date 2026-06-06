/**
 * @file hostile_observation_probe.h
 * @brief Sidecar-local hostile observation probe tick hooks.
 */
#pragma once

#include <nlohmann/json.hpp>

struct ScreenManager;

bool           hostile_observation_frame_subscriber_enabled();
void           hostile_observation_tick(ScreenManager* screen_manager);
nlohmann::json hostile_observation_state();
