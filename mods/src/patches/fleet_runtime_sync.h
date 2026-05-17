/**
 * @file fleet_runtime_sync.h
 * @brief Event-driven fleet runtime sync producer.
 */
#pragma once

#include <string_view>

/**
 * @brief Observe current fleet-bar runtime state and enqueue a sync snapshot when it meaningfully changes.
 */
void fleet_runtime_sync_capture(std::string_view source);
