/**
 * @file fleet_runtime_sync.h
 * @brief Event-driven fleet runtime sync producer.
 */
#pragma once

#include "patches/gameplay_dispatch_context.h"

/**
 * @brief Record a gameplay dispatch context for deferred processing from a frame tick.
 */
void fleet_runtime_sync_trigger(const GameplayDispatchContext& dispatch);

/**
 * @brief True when deferred fleet runtime sync needs the frame tick subscriber.
 */
bool fleet_runtime_sync_frame_subscriber_enabled();

/**
 * @brief Flush one pending fleet runtime sync request from the frame tick subscriber.
 */
void fleet_runtime_sync_process_pending();

/**
 * @brief Observe current fleet-bar runtime state with dispatch provenance and enqueue a sync snapshot when it changes.
 */
void fleet_runtime_sync_capture(const GameplayDispatchContext& dispatch);
