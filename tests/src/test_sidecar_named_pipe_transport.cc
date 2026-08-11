#include <doctest/doctest.h>

#include "patches/sidecar_named_pipe_transport.h"

#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#if _WIN32
#include <windows.h>
#endif

using namespace std::chrono_literals;

TEST_SUITE("sidecar_named_pipe_transport")
{
  TEST_CASE("closed response contract accepts only exact handshake and terminal states")
  {
    using sidecar_named_pipe_transport::ParseResponse;
    using sidecar_named_pipe_transport::ResultCode;

    CHECK(
        ParseResponse(
            R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","acceptedRecords":0,"failure":"none"})",
            true)
            .code
        == ResultCode::Sent);
    const auto accepted = ParseResponse(
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"accepted","acceptedRecords":3,"failure":"none"})",
        false);
    CHECK(accepted.code == ResultCode::Sent);
    CHECK(accepted.accepted_records == 3);
    CHECK(
        ParseResponse(
            R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"chunk-pending","acceptedRecords":0,"failure":"none"})",
            false)
            .code
        == ResultCode::ChunkPending);
    CHECK(
        ParseResponse(
            R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"rejected","acceptedRecords":0,"failure":"busy"})",
            false)
            .code
        == ResultCode::Rejected);
  }

  TEST_CASE("hostile or noncanonical response shapes fail closed")
  {
    using sidecar_named_pipe_transport::ParseResponse;
    using sidecar_named_pipe_transport::ResultCode;
    const std::array hostile{
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","statu\u0073":"ready","acceptedRecords":0,"failure":"none"})",
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","acceptedRecords":0,"failure":"none","extra":"x"})",
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v2","status":"ready","acceptedRecords":0,"failure":"none"})",
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","acceptedRecords":"0","failure":"none"})",
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","acceptedRecords":0,"failure":"secret text"})",
        R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"accepted","acceptedRecords":1,"failure":"busy"})",
        R"([])",
        R"({)",
    };
    for (const auto* response : hostile) {
      CHECK(ParseResponse(response, false).code == ResultCode::InvalidResponse);
    }
  }

  TEST_CASE("invalid input rejects without opening a transport")
  {
    using sidecar_named_pipe_transport::ResultCode;
    using sidecar_named_pipe_transport::Send;
    const std::string credential(43, 'A');
    CHECK(Send("../pipe", credential, "{}", 10ms, 10ms).code == ResultCode::Unavailable);
    CHECK(Send("valid.pipe", "short", "{}", 10ms, 10ms).code == ResultCode::Unavailable);
    CHECK(Send("valid.pipe", credential, "", 10ms, 10ms).code == ResultCode::Unavailable);
    CHECK(Send("valid.pipe", credential, std::string(sidecar_named_pipe_transport::kMaximumPayloadBytes + 1, 'x'), 10ms,
               10ms)
              .code
          == ResultCode::Unavailable);
  }

#if _WIN32
  namespace
  {
    bool read_exact(HANDLE pipe, void* data, size_t size)
    {
      auto*  bytes = static_cast<std::byte*>(data);
      size_t read  = 0;
      while (read < size) {
        DWORD current = 0;
        if (!ReadFile(pipe, bytes + read, static_cast<DWORD>(size - read), &current, nullptr) || current == 0)
          return false;
        read += current;
      }
      return true;
    }

    bool write_exact(HANDLE pipe, const void* data, size_t size)
    {
      const auto* bytes   = static_cast<const std::byte*>(data);
      size_t      written = 0;
      while (written < size) {
        DWORD current = 0;
        if (!WriteFile(pipe, bytes + written, static_cast<DWORD>(size - written), &current, nullptr) || current == 0)
          return false;
        written += current;
      }
      return true;
    }

    std::string read_frame(HANDLE pipe)
    {
      std::array<std::byte, 4> prefix{};
      if (!read_exact(pipe, prefix.data(), prefix.size()))
        return {};
      const auto length = std::to_integer<uint32_t>(prefix[0]) | (std::to_integer<uint32_t>(prefix[1]) << 8)
                          | (std::to_integer<uint32_t>(prefix[2]) << 16) | (std::to_integer<uint32_t>(prefix[3]) << 24);
      if (length == 0 || length > sidecar_named_pipe_transport::kMaximumPayloadBytes)
        return {};
      std::string value(length, '\0');
      return read_exact(pipe, value.data(), value.size()) ? value : std::string{};
    }

    bool write_frame(HANDLE pipe, std::string_view value)
    {
      const auto               length = static_cast<uint32_t>(value.size());
      std::array<std::byte, 4> prefix{
          std::byte(length & 0xff),
          std::byte((length >> 8) & 0xff),
          std::byte((length >> 16) & 0xff),
          std::byte((length >> 24) & 0xff),
      };
      return write_exact(pipe, prefix.data(), prefix.size()) && write_exact(pipe, value.data(), value.size());
    }

    HANDLE create_server(std::string_view pipe_name)
    {
      const std::wstring path = L"\\\\.\\pipe\\" + std::wstring(pipe_name.begin(), pipe_name.end());
      return CreateNamedPipeW(path.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                              4096, 4096, 0, nullptr);
    }
  } // namespace

  TEST_CASE("windows client sends the exact authenticated header and payload")
  {
    using sidecar_named_pipe_transport::ResultCode;
    const auto        pipe_name = "stfc-mod-pipe-test-" + std::to_string(GetCurrentProcessId()) + "-exact";
    const std::string credential(43, 'A');
    const std::string payload = R"({"exact":"payload bytes"})";
    const auto        server  = create_server(pipe_name);
    REQUIRE(server != INVALID_HANDLE_VALUE);
    std::string      observed_header;
    std::string      observed_payload;
    std::atomic<int> observed_impersonation_level{-1};
    std::thread      server_thread([&] {
      const auto connected = ConnectNamedPipe(server, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
      if (!connected)
        return;
      if (ImpersonateNamedPipeClient(server)) {
        HANDLE token = nullptr;
        if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
          SECURITY_IMPERSONATION_LEVEL level{};
          DWORD                        returned = 0;
          if (GetTokenInformation(token, TokenImpersonationLevel, &level, sizeof(level), &returned)) {
            observed_impersonation_level.store(static_cast<int>(level));
          }
          CloseHandle(token);
        }
        RevertToSelf();
      }
      observed_header = read_frame(server);
      write_frame(
          server,
          R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"ready","acceptedRecords":0,"failure":"none"})");
      observed_payload = read_frame(server);
      write_frame(
          server,
          R"({"protocolVersion":"stfc.battle-bridge.local-ipc.v1","status":"accepted","acceptedRecords":1,"failure":"none"})");
      FlushFileBuffers(server);
      DisconnectNamedPipe(server);
    });

    const auto result = sidecar_named_pipe_transport::Send(pipe_name, credential, payload, 2s, 2s);
    server_thread.join();
    CloseHandle(server);

    CHECK(result.code == ResultCode::Sent);
    CHECK(result.accepted_records == 1);
    CHECK(observed_impersonation_level.load() == static_cast<int>(SecurityIdentification));
    CHECK(observed_payload == payload);
    const auto header = nlohmann::json::parse(observed_header);
    CHECK(header.size() == 4);
    CHECK(header["protocolVersion"].get<std::string>() == std::string(sidecar_named_pipe_transport::kProtocolVersion));
    CHECK(header["role"].get<std::string>() == std::string(sidecar_named_pipe_transport::kRuntimeRole));
    CHECK(header["operation"].get<std::string>() == std::string(sidecar_named_pipe_transport::kIngestOperation));
    CHECK(header["credential"].get<std::string>() == credential);
  }

  TEST_CASE("windows client applies one bounded request deadline")
  {
    using sidecar_named_pipe_transport::ResultCode;
    const auto        pipe_name = "stfc-mod-pipe-test-" + std::to_string(GetCurrentProcessId()) + "-timeout";
    const std::string credential(43, 'A');
    const auto        server = create_server(pipe_name);
    REQUIRE(server != INVALID_HANDLE_VALUE);
    std::thread server_thread([&] {
      const auto connected = ConnectNamedPipe(server, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
      if (!connected)
        return;
      static_cast<void>(read_frame(server));
      std::this_thread::sleep_for(150ms);
      DisconnectNamedPipe(server);
    });

    const auto started = std::chrono::steady_clock::now();
    const auto result  = sidecar_named_pipe_transport::Send(pipe_name, credential, "{}", 1s, 40ms);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    server_thread.join();
    CloseHandle(server);

    CHECK(result.code == ResultCode::TimedOut);
    CHECK(elapsed < 500ms);
  }
#endif
}
