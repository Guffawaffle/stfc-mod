#pragma once

#include <doctest/doctest.h>

#include "bounded_ttl_cache.h"
#include "config_schema.h"
#include "patches/async_work_queue.h"
#include "patches/battle_log_decoder.h"
#include "patches/fleet_deferred_action.h"
#include "patches/fleet_input_policy.h"
#include "patches/input_binding/input_binding.h"
#include "patches/input_binding/input_config_bridge.h"
#include "patches/live_debug_event_store.h"
#include "patches/live_debug_fleet_serializers.h"
#include "patches/live_debug_recent_event_requests.h"
#include "patches/live_debug_ui_serializers.h"
#include "patches/live_debug_viewer_serializers.h"
#include "patches/notification_queue.h"
#include "patches/notification_text.h"
#include "patches/object_tracker_core.h"
#include "str_utils_pure.h"
#include "testable_functions.h"

#include <array>
#include <chrono>
#include <utility>