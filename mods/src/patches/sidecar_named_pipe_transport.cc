#include "patches/sidecar_named_pipe_transport.h"

#include "patches/sidecar_local_ingest_policy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#if _WIN32
#include <windows.h>
#endif

namespace sidecar_named_pipe_transport
{
namespace
{
  using json = nlohmann::json;

  constexpr std::array<std::string_view, 15> kFailureValues{
      "none",
      "unauthorized",
      "invalid-request",
      "unsupported-protocol",
      "payload-too-large",
      "rate-limited",
      "busy",
      "chunk-conflict",
      "batch-conflict",
      "timed-out",
      "storage-rejected",
      "pipe-unavailable",
      "start-failed",
      "listener-failed",
      "shutdown-timed-out",
  };

  bool safe_failure(const std::string_view value)
  { return std::ranges::find(kFailureValues, value) != kFailureValues.end(); }

  json parse_closed_json(const std::string_view value, bool& duplicate)
  {
    std::vector<std::unordered_set<std::string>> object_keys;
    const auto callback = [&object_keys, &duplicate](const int, const json::parse_event_t event, json& parsed) {
      if (event == json::parse_event_t::object_start) {
        object_keys.emplace_back();
      } else if (event == json::parse_event_t::key) {
        if (object_keys.empty() || !object_keys.back().insert(parsed.get<std::string>()).second) {
          duplicate = true;
        }
      } else if (event == json::parse_event_t::object_end) {
        if (object_keys.empty())
          duplicate = true;
        else
          object_keys.pop_back();
      }
      return true;
    };
    return json::parse(value.begin(), value.end(), callback, false, false);
  }

  bool has_exact_response_shape(const json& response)
  {
    return response.is_object() && response.size() == 4 && response.contains("protocolVersion")
           && response.contains("status") && response.contains("acceptedRecords") && response.contains("failure")
           && response["protocolVersion"].is_string() && response["status"].is_string()
           && response["acceptedRecords"].is_number_integer() && response["failure"].is_string();
  }

#if _WIN32
  class unique_handle
  {
  public:
    explicit unique_handle(HANDLE value = INVALID_HANDLE_VALUE)
        : value_(value)
    {
    }
    ~unique_handle()
    {
      if (valid())
        CloseHandle(value_);
    }
    unique_handle(const unique_handle&)            = delete;
    unique_handle& operator=(const unique_handle&) = delete;
    unique_handle(unique_handle&& other) noexcept
        : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE))
    {
    }
    bool valid() const
    { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const
    { return value_; }

  private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
  };

  enum class IoResult { Complete, Failed, TimedOut };

  DWORD remaining_timeout(const std::chrono::steady_clock::time_point deadline)
  {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline)
      return 0;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<DWORD>(std::clamp<int64_t>(remaining, 1, std::numeric_limits<DWORD>::max() - 1));
  }

  IoResult transfer_exact(HANDLE handle, void* data, const size_t size, const bool write,
                          const std::chrono::steady_clock::time_point deadline)
  {
    auto*  bytes     = static_cast<std::byte*>(data);
    size_t completed = 0;
    while (completed < size) {
      const auto remaining = remaining_timeout(deadline);
      if (remaining == 0)
        return IoResult::TimedOut;
      unique_handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
      if (!event.valid())
        return IoResult::Failed;
      OVERLAPPED operation{};
      operation.hEvent     = event.get();
      const auto requested = static_cast<DWORD>(std::min<size_t>(size - completed, std::numeric_limits<DWORD>::max()));
      DWORD      immediate = 0;
      const auto started   = write ? WriteFile(handle, bytes + completed, requested, &immediate, &operation)
                                   : ReadFile(handle, bytes + completed, requested, &immediate, &operation);
      DWORD      transferred = immediate;
      if (!started) {
        const auto error = GetLastError();
        if (error != ERROR_IO_PENDING)
          return IoResult::Failed;
        const auto wait = WaitForSingleObject(event.get(), remaining);
        if (wait == WAIT_TIMEOUT) {
          CancelIoEx(handle, &operation);
          WaitForSingleObject(event.get(), INFINITE);
          return IoResult::TimedOut;
        }
        if (wait != WAIT_OBJECT_0 || !GetOverlappedResult(handle, &operation, &transferred, FALSE)) {
          return IoResult::Failed;
        }
      }
      if (transferred == 0)
        return IoResult::Failed;
      completed += transferred;
    }
    return IoResult::Complete;
  }

  IoResult write_frame(HANDLE handle, const std::string_view bytes,
                       const std::chrono::steady_clock::time_point deadline)
  {
    if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
      return IoResult::Failed;
    const auto               length = static_cast<uint32_t>(bytes.size());
    std::array<std::byte, 4> prefix{
        std::byte(length & 0xff),
        std::byte((length >> 8) & 0xff),
        std::byte((length >> 16) & 0xff),
        std::byte((length >> 24) & 0xff),
    };
    auto result = transfer_exact(handle, prefix.data(), prefix.size(), true, deadline);
    if (result != IoResult::Complete)
      return result;
    return transfer_exact(handle, const_cast<char*>(bytes.data()), bytes.size(), true, deadline);
  }

  std::pair<IoResult, std::string> read_frame(HANDLE handle, const size_t maximum_bytes,
                                              const std::chrono::steady_clock::time_point deadline)
  {
    std::array<std::byte, 4> prefix{};
    auto                     result = transfer_exact(handle, prefix.data(), prefix.size(), false, deadline);
    if (result != IoResult::Complete)
      return {result, {}};
    const auto length = std::to_integer<uint32_t>(prefix[0]) | (std::to_integer<uint32_t>(prefix[1]) << 8)
                        | (std::to_integer<uint32_t>(prefix[2]) << 16) | (std::to_integer<uint32_t>(prefix[3]) << 24);
    if (length == 0 || length > maximum_bytes)
      return {IoResult::Failed, {}};
    std::string bytes(length, '\0');
    result = transfer_exact(handle, bytes.data(), bytes.size(), false, deadline);
    return {result, result == IoResult::Complete ? std::move(bytes) : std::string{}};
  }
#endif
} // namespace

Result ParseResponse(const std::string_view response_bytes, const bool handshake)
{
  if (response_bytes.empty() || response_bytes.size() > kMaximumResponseBytes)
    return {ResultCode::InvalidResponse};
  bool       duplicate = false;
  const auto response  = parse_closed_json(response_bytes, duplicate);
  if (duplicate || response.is_discarded() || !has_exact_response_shape(response)) {
    return {ResultCode::InvalidResponse};
  }
  const auto protocol = response["protocolVersion"].get<std::string>();
  const auto status   = response["status"].get<std::string>();
  const auto accepted = response["acceptedRecords"].get<int64_t>();
  const auto failure  = response["failure"].get<std::string>();
  if (protocol != kProtocolVersion || accepted < 0 || accepted > 4096 || !safe_failure(failure)) {
    return {ResultCode::InvalidResponse};
  }
  if (handshake) {
    return status == "ready" && accepted == 0 && failure == "none" ? Result{ResultCode::Sent}
                                                                   : Result{ResultCode::Rejected};
  }
  if (status == "accepted" && failure == "none")
    return {ResultCode::Sent, static_cast<int>(accepted)};
  if (status == "chunk-pending" && accepted == 0 && failure == "none")
    return {ResultCode::ChunkPending};
  if (status == "rejected" && failure != "none")
    return {ResultCode::Rejected};
  return {ResultCode::InvalidResponse};
}

Result Send(const std::string_view pipe_name, const std::string_view credential, const std::string_view payload,
            const std::chrono::milliseconds connect_timeout, const std::chrono::milliseconds request_timeout)
{
  if (!SidecarLocalNamedPipeNameValid(pipe_name) || !SidecarLocalNamedPipeCredentialValid(credential) || payload.empty()
      || payload.size() > kMaximumPayloadBytes || connect_timeout.count() <= 0 || request_timeout.count() <= 0) {
    return {ResultCode::Unavailable};
  }
#if !_WIN32
  return {ResultCode::UnsupportedPlatform};
#else
  const std::wstring path = L"\\\\.\\pipe\\" + std::wstring(pipe_name.begin(), pipe_name.end());
  const auto         connect_millis =
      static_cast<DWORD>(std::clamp<int64_t>(connect_timeout.count(), 1, std::numeric_limits<DWORD>::max() - 1));
  if (!WaitNamedPipeW(path.c_str(), connect_millis)) {
    return {GetLastError() == ERROR_SEM_TIMEOUT ? ResultCode::TimedOut : ResultCode::Unavailable};
  }
  unique_handle pipe(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
  if (!pipe.valid())
    return {ResultCode::Unavailable};
  DWORD mode = PIPE_READMODE_BYTE;
  if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr))
    return {ResultCode::Unavailable};

  auto header = json{
      {"protocolVersion", kProtocolVersion},
      {"role", kRuntimeRole},
      {"operation", kIngestOperation},
      {"credential",
       credential}}.dump();
  if (header.size() > kMaximumHeaderBytes)
    return {ResultCode::Unavailable};
  const auto deadline = std::chrono::steady_clock::now() + request_timeout;
  auto       outcome  = write_frame(pipe.get(), header, deadline);
  std::ranges::fill(header, '\0');
  if (outcome == IoResult::TimedOut)
    return {ResultCode::TimedOut};
  if (outcome != IoResult::Complete)
    return {ResultCode::Unavailable};
  auto [read_outcome, handshake_bytes] = read_frame(pipe.get(), kMaximumResponseBytes, deadline);
  if (read_outcome == IoResult::TimedOut)
    return {ResultCode::TimedOut};
  if (read_outcome != IoResult::Complete)
    return {ResultCode::Unavailable};
  const auto handshake = ParseResponse(handshake_bytes, true);
  if (handshake.code != ResultCode::Sent)
    return handshake;

  outcome = write_frame(pipe.get(), payload, deadline);
  if (outcome == IoResult::TimedOut)
    return {ResultCode::TimedOut};
  if (outcome != IoResult::Complete)
    return {ResultCode::Unavailable};
  auto [response_outcome, response_bytes] = read_frame(pipe.get(), kMaximumResponseBytes, deadline);
  if (response_outcome == IoResult::TimedOut)
    return {ResultCode::TimedOut};
  if (response_outcome != IoResult::Complete)
    return {ResultCode::Unavailable};
  return ParseResponse(response_bytes, false);
#endif
}
} // namespace sidecar_named_pipe_transport
