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
  struct SessionHeaderSnapshot {
    std::string gameServerUrl;
    std::string instanceSessionId;
    int32_t     instanceId = 0;
    std::string unityVersion;
    std::string primeVersion;
  };

  extern const char poweredBy[];

  void SetPrimeServerHeaders(std::string serverUrl, std::string sessionId);
  void SetPrimeVersion(std::string version);
  void SetInstanceId(int32_t instanceId);
  SessionHeaderSnapshot Snapshot();
} // namespace headers

void sync_log_error(const std::string& type, const std::string& target, const std::string& text);
void sync_log_warn(const std::string& type, const std::string& target, const std::string& text);
void sync_log_info(const std::string& type, const std::string& target, const std::string& text);
void sync_log_debug(const std::string& type, const std::string& target, const std::string& text);
void sync_log_trace(const std::string& type, const std::string& target, const std::string& text);

void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync);
std::string get_scopely_data(const std::string& path, const std::string& post_data);
void shutdown_workers();
} // namespace http