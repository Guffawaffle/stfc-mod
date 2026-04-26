/**
 * @file sync_transport.h
 * @brief HTTP transport and Scopely API client for sync pipelines.
 */
#pragma once

#include "config.h"

#include <cstdint>
#include <string>

namespace http
{
namespace headers
{
  extern std::string gameServerUrl;
  extern std::string instanceSessionId;
  extern int32_t     instanceId;
  extern std::string unityVersion;
  extern std::string primeVersion;
  extern const char  poweredBy[];
} // namespace headers

void sync_log_error(const std::string& type, const std::string& target, const std::string& text);
void sync_log_warn(const std::string& type, const std::string& target, const std::string& text);
void sync_log_info(const std::string& type, const std::string& target, const std::string& text);
void sync_log_debug(const std::string& type, const std::string& target, const std::string& text);
void sync_log_trace(const std::string& type, const std::string& target, const std::string& text);

void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync);
std::string get_scopely_data(const std::string& path, const std::string& post_data);
} // namespace http