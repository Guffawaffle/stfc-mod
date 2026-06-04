/**
 * @file live_debug_public_stubs.cc
 * @brief Public-release no-op stubs for dev-only live-debug surfaces.
 */
#include "patches/live_debug.h"
#include "patches/live_debug_event_dispatcher.h"

#if defined(STFC_PUBLIC_RELEASE) && STFC_PUBLIC_RELEASE

namespace live_debug_events {
void RecordEvent(std::string_view, nlohmann::json)
{
}

LiveDebugRecentEventStoreSnapshot Snapshot(const LiveDebugRecentEventStoreQuery&)
{
  return {};
}

size_t Clear()
{
  return 0;
}
}

void live_debug_record_space_action_warp_cancel(FleetBarViewController*, FleetPlayerData*, bool, bool, bool, bool,
                                                bool, bool, bool, bool, int, bool, bool, bool)
{
}

void live_debug_record_space_action_warp_cancel_suppressed(FleetBarViewController*, FleetPlayerData*, bool, bool,
                                                           bool, bool, bool, bool, bool, bool, int, bool, bool, bool)
{
}

void live_debug_record_incoming_fleet_materialized(std::string_view, int, uint64_t, int, uint64_t, std::string_view)
{
}

void live_debug_record_toast_notification(std::string_view, const void*, int, std::string_view)
{
}

void live_debug_record_incoming_attack_notification_context(std::string_view, std::string_view, int, uint64_t,
                                                            std::string_view, int, int)
{
}

void live_debug_record_navigation_interaction(std::string_view, std::string_view, bool, int, int, std::string_view,
                                              bool, int, bool, bool, int64_t, std::string_view, std::string_view,
                                              std::string_view, std::string_view, std::string_view,
                                              std::string_view, std::string_view)
{
}

void live_debug_tick(ScreenManager*)
{
}

#endif
