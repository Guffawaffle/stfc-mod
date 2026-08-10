#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sidecar_named_pipe_transport
{
inline constexpr std::string_view kProtocolVersion      = "stfc.battle-bridge.local-ipc.v1";
inline constexpr std::string_view kRuntimeRole          = "stfc-mod-runtime";
inline constexpr std::string_view kIngestOperation      = "ingest";
inline constexpr size_t           kMaximumHeaderBytes   = 4096;
inline constexpr size_t           kMaximumResponseBytes = 4096;
inline constexpr size_t           kMaximumPayloadBytes  = 512 * 1024;

enum class ResultCode {
  Sent,
  ChunkPending,
  Rejected,
  Unavailable,
  TimedOut,
  InvalidResponse,
  UnsupportedPlatform,
};

struct Result {
  ResultCode code             = ResultCode::Unavailable;
  int        accepted_records = 0;
};

Result Send(std::string_view pipe_name, std::string_view credential, std::string_view payload,
            std::chrono::milliseconds connect_timeout, std::chrono::milliseconds request_timeout);

Result ParseResponse(std::string_view response, bool handshake);
} // namespace sidecar_named_pipe_transport
