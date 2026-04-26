/**
 * @file live_debug_event_dispatcher.cc
 * @brief Thin live-debug event recording facade over the recent-event store.
 */
#include "patches/live_debug_event_dispatcher.h"

#include "config.h"

#include <chrono>
#include <utility>

namespace {
LiveDebugRecentEventStore g_recentEventStore;

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}
}

namespace live_debug_events {
void RecordEvent(std::string_view kind, nlohmann::json details)
{
  if (!LiveDebugChannelEnabled()) {
    return;
  }

  g_recentEventStore.append(kind, std::move(details), current_time_millis_utc());
}

LiveDebugRecentEventStoreSnapshot Snapshot(const LiveDebugRecentEventStoreQuery& query)
{
  return g_recentEventStore.snapshot(query);
}

size_t Clear()
{
  return g_recentEventStore.clear();
}
}