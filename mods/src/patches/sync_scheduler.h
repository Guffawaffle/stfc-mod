/**
 * @file sync_scheduler.h
 * @brief Main sync queue scheduler and consumer thread.
 */
#pragma once

#include "config.h"
#include "patches/fleet_runtime_diagnostics.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

void queue_data(SyncConfig::Type type, const std::string& data, bool is_first_sync = false,
				std::optional<FleetRuntimeTraceContext> fleet_runtime_trace = std::nullopt);
void queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync = false,
				std::optional<FleetRuntimeTraceContext> fleet_runtime_trace = std::nullopt);
void StartSyncSchedulerWorker();
void ShutdownSyncSchedulerWorker();
