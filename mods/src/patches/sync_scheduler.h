/**
 * @file sync_scheduler.h
 * @brief Main sync queue scheduler and consumer thread.
 */
#pragma once

#include "config.h"

#include <nlohmann/json.hpp>

#include <string>

void queue_data(SyncConfig::Type type, const std::string& data, bool is_first_sync = false);
void queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync = false);
void ship_sync_data();