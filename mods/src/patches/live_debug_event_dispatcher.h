/**
 * @file live_debug_event_dispatcher.h
 * @brief Thin live-debug event recording facade over the recent-event store.
 */
#pragma once

#include "patches/live_debug_event_store.h"

#include <string_view>

#include <nlohmann/json.hpp>

namespace live_debug_events {
void RecordEvent(std::string_view kind, nlohmann::json details);
LiveDebugRecentEventStoreSnapshot Snapshot(const LiveDebugRecentEventStoreQuery& query = {});
size_t Clear();
}