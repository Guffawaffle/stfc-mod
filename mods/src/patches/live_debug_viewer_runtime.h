/**
 * @file live_debug_viewer_runtime.h
 * @brief Runtime viewer/top-canvas observation and JSON helpers for live-debug.
 */
#pragma once

#include "patches/live_debug_ui_observations.h"
#include "patches/live_debug_viewer_serializers.h"

#include <nlohmann/json.hpp>

struct TimerDataContext;
struct MiningObjectViewerWidget;
struct StarNodeObjectViewerWidget;
struct PreScanTargetWidget;
struct CelestialObjectViewerWidget;

nlohmann::json timer_context_to_json(TimerDataContext* timer);
nlohmann::json mining_viewer_to_json(MiningObjectViewerWidget* mining_viewer);
nlohmann::json star_node_viewer_to_json(StarNodeObjectViewerWidget* star_node_viewer);
nlohmann::json prescan_target_to_json(PreScanTargetWidget* prescan_target);
nlohmann::json celestial_viewer_to_json(CelestialObjectViewerWidget* celestial_viewer);

TargetViewerObservation observe_target_viewer();
MineViewerObservation observe_mine_viewer();
TopCanvasObservation observe_top_canvas();