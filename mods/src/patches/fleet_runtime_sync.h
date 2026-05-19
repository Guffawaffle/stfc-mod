/**
 * @file fleet_runtime_sync.h
 * @brief Event-driven fleet runtime sync producer.
 */
#pragma once

#include <string_view>

/**
 * @brief Process a trigger source through diagnostics and change-gated capture when fleet runtime sync is enabled.
 */
void fleet_runtime_sync_trigger(std::string_view source);

/**
 * @brief Observe current fleet-bar runtime state and enqueue a sync snapshot when it meaningfully changes.
 */
void fleet_runtime_sync_capture(std::string_view source);
