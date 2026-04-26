/**
 * @file live_debug_recent_event_requests.h
 * @brief Pure request/response helpers for the live-debug recent-events command.
 */
#pragma once

#include "patches/live_debug_event_store.h"

#include <nlohmann/json.hpp>

LiveDebugRecentEventStoreQuery live_debug_recent_events_query_from_request(const nlohmann::json& request);
nlohmann::json live_debug_recent_events_result(LiveDebugRecentEventStoreSnapshot snapshot);