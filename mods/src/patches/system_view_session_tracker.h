/**
 * @file system_view_session_tracker.h
 * @brief Diagnostics-only system-view section transition tracker.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>

struct ScreenManager;

bool           system_view_session_frame_subscriber_enabled();
void           system_view_session_tick(ScreenManager* screen_manager);
void           system_view_session_note_passive_observation(const nlohmann::json& observation);
nlohmann::json system_view_session_state();
