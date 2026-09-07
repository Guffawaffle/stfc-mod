/**
 * @file game_error_probe.cc
 * @brief Opt-in science probe for the game's central managed GSError handler.
 */
#include "dev/diagnostics.h"
#include "str_utils.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>

#include <spud/detour.h>

#include <nlohmann/json.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#if _WIN32
#include <Windows.h>
#endif

#if defined(_MODDBG) && defined(_WIN32)
namespace
{
constexpr std::string_view kProbeEnvironmentVariable = "STFC_MOD_SCIENCE_ERROR_PROBE";
constexpr std::string_view kProbeLogFilename         = "community_patch_science_errors.jsonl";
constexpr size_t           kMaxCategoryBytes         = 64;
constexpr size_t           kMaxMessageBytes          = 240;
constexpr size_t           kMaxRouteBytes            = 160;
constexpr size_t           kMaxTransactionIdBytes    = 96;
constexpr size_t           kConsoleEventBytes        = 768;
constexpr size_t           kMaximumEventsPerMinute   = 60;
constexpr auto             kDuplicateWindow          = std::chrono::seconds(10);
constexpr auto             kRateWindow               = std::chrono::minutes(1);
constexpr uintptr_t        kValidatedHandlerRva      = 0x8198C0;
// SPUD's x64 detour relocates complete instructions until its 24-byte absolute jump fits. For this entry that is
// the first 29 bytes, so fingerprint the whole relocation window rather than only the common function prologue.
constexpr std::array<uint8_t, 29> kValidatedHandlerRelocationWindow{
    0x48, 0x89, 0x5C, 0x24, 0x20, 0x56, 0x48, 0x83, 0xEC, 0x30, 0x80, 0x3D, 0x33, 0xCE, 0x3F,
    0x05, 0x00, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF1, 0x0F, 0x85, 0x06, 0x01, 0x00, 0x00,
};

struct ErrorSnapshot {
  int32_t     type               = 0;
  int32_t     code               = 0;
  int32_t     http_response_code = 0;
  std::string category;
  std::string message;
  std::string request_route;
  std::string transaction_id;
  bool        present = false;
};

struct ProbeState {
  std::mutex                            mutex;
  std::shared_ptr<spdlog::logger>       logger;
  uint64_t                              next_sequence = 1;
  std::string                           last_fingerprint;
  std::chrono::steady_clock::time_point last_event_at;
  std::chrono::steady_clock::time_point rate_window_started_at;
  size_t                                emitted_in_rate_window      = 0;
  size_t                                duplicate_events_suppressed = 0;
  size_t                                rate_limited_events         = 0;
  std::string                           session_id;
};

ProbeState& probe_state()
{
  static ProbeState state;
  return state;
}

const auto s_console_source = dev::diagnostics::RegisterSource("game-errors");

bool probe_requested()
{
  const auto* value = std::getenv(kProbeEnvironmentVariable.data());
  return value != nullptr && std::string_view{value} == "1";
}

IL2CppClassHelper& error_handler_helper()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "GsErrorHandler");
  return helper;
}

IL2CppClassHelper& gs_error_helper()
{
  static auto helper = il2cpp_get_class_helper("Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "GSError");
  return helper;
}

bool required_error_fields_available()
{
  constexpr std::array required_fields{
      "<Type>k__BackingField",          "<Code>k__BackingField",    "<HttpResponseCode>k__BackingField",
      "<Category>k__BackingField",      "<Message>k__BackingField", "<RequestUrl>k__BackingField",
      "<TransactionId>k__BackingField",
  };
  auto& helper = gs_error_helper();
  return std::ranges::all_of(required_fields, [&helper](const char* name) {
    auto field = helper.GetField(name);
    return field.isValidHelper();
  });
}

template <typename T> T read_field(void* object, IL2CppClassHelper& helper, const char* name, T fallback = {})
{
  if (object == nullptr || !helper.isValidHelper()) {
    return fallback;
  }
  auto field = helper.GetField(name);
  if (!field.isValidHelper()) {
    return fallback;
  }
  return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + field.offset());
}

std::string managed_string(void* value)
{
  if (value == nullptr) {
    return {};
  }
  try {
    return to_string(static_cast<Il2CppString*>(value));
  } catch (...) {
    return "<conversion-failed>";
  }
}

std::string bounded_single_line(std::string value, size_t maximum_bytes)
{
  for (auto& character : value) {
    if (character == '\r' || character == '\n' || character == '\t') {
      character = ' ';
    }
  }
  if (value.size() > maximum_bytes) {
    auto boundary = maximum_bytes;
    while (boundary > 0 && (static_cast<uint8_t>(value[boundary]) & 0xC0U) == 0x80U) {
      --boundary;
    }
    value.resize(boundary);
  }
  return value;
}

void append_console_field(std::string& line, std::string_view label, std::string_view value,
                          size_t maximum_value_bytes = kConsoleEventBytes)
{
  if (value.empty() || line.size() + label.size() >= kConsoleEventBytes) {
    return;
  }
  const auto remaining = kConsoleEventBytes - line.size() - label.size();
  auto       displayed = bounded_single_line(std::string{value}, std::min(remaining, maximum_value_bytes));
  for (auto& character : displayed) {
    const auto byte = static_cast<uint8_t>(character);
    if (byte < 0x20U || byte == 0x7FU) {
      character = ' ';
    }
  }
  for (size_t index = 0; index < displayed.size(); ++index) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(displayed.data());
    if (index + 1 < displayed.size() && bytes[index] == 0xC2U && bytes[index + 1] == 0x85U) {
      displayed[index] = displayed[index + 1] = ' ';
      ++index;
    } else if (index + 2 < displayed.size() && bytes[index] == 0xE2U && bytes[index + 1] == 0x80U
               && (bytes[index + 2] == 0xA8U || bytes[index + 2] == 0xA9U)) {
      displayed[index] = displayed[index + 1] = displayed[index + 2] = ' ';
      index += 2;
    }
  }
  std::ranges::replace(displayed, '<', '[');
  std::ranges::replace(displayed, '>', ']');
  line += label;
  line += displayed;
}

std::string_view error_type_name(int32_t type);

std::string compose_console_summary(const ErrorSnapshot& error)
{
  auto summary = std::string{error_type_name(error.type)} + " c=" + std::to_string(error.code)
                 + " h=" + std::to_string(error.http_response_code);
  append_console_field(summary, " cat=", error.category, kMaxCategoryBytes);
  append_console_field(summary, " msg=", error.message, kMaxMessageBytes);
  append_console_field(summary, " tx=", error.transaction_id, kMaxTransactionIdBytes);
  append_console_field(summary, " route=", error.request_route, kMaxRouteBytes);
  return summary;
}

bool utf8_boundary_self_test()
{
  auto value = std::string(239, 'a');
  value += "\xC3\xA9";
  return bounded_single_line(std::move(value), 240).size() == 239;
}

bool console_display_self_test()
{
  auto hostile = std::string{"<tag>"};
  hostile.push_back('\0');
  hostile.push_back('\b');
  hostile.push_back('\x1B');
  hostile.push_back('\x7F');
  hostile += "\xC2\x85\xE2\x80\xA8\xE2\x80\xA9";
  hostile += " text";
  std::string sanitized;
  append_console_field(sanitized, "msg=", hostile);

  auto unicode_boundary = std::string(kConsoleEventBytes - 5, 'a');
  append_console_field(unicode_boundary, " x=", "a\xC3\xA9");

  std::string details;
  append_console_field(details, "tx=", "02d29a88-e604-4d0e-9c51-6cf4bfeab819", 48);
  append_console_field(details, " route=", std::string(160, 'r'));

  ErrorSnapshot maximal{
      .type               = 1,
      .code               = 429,
      .http_response_code = 429,
      .category           = std::string(kMaxCategoryBytes - 1, 'c') + 'C',
      .message            = std::string(kMaxMessageBytes - 1, 'm') + 'M',
      .request_route      = std::string(kMaxRouteBytes - 1, 'r') + 'R',
      .transaction_id     = std::string(kMaxTransactionIdBytes - 1, 't') + 'T',
      .present            = true,
  };
  const auto maximal_summary = compose_console_summary(maximal);

  const auto has_control = std::ranges::any_of(sanitized, [](char character) {
    const auto byte = static_cast<uint8_t>(character);
    return byte < 0x20U || byte == 0x7FU;
  });
  return sanitized.find_first_of("<>") == std::string::npos && sanitized.find("\xC2\x85") == std::string::npos
         && sanitized.find("\xE2\x80\xA8") == std::string::npos && sanitized.find("\xE2\x80\xA9") == std::string::npos
         && !has_control && simdutf::validate_utf8(unicode_boundary.data(), unicode_boundary.size())
         && unicode_boundary.size() <= kConsoleEventBytes && details.size() < kConsoleEventBytes
         && details.find("02d29a88-e604-4d0e-9c51-6cf4bfeab819") != std::string::npos
         && details.find(std::string(160, 'r')) != std::string::npos && maximal_summary.size() <= kConsoleEventBytes
         && maximal_summary.find(maximal.category) != std::string::npos
         && maximal_summary.find(maximal.message) != std::string::npos
         && maximal_summary.find(maximal.transaction_id) != std::string::npos
         && maximal_summary.find(maximal.request_route) != std::string::npos;
}

std::string sanitize_request_route(std::string_view url)
{
  const auto scheme = url.find("://");
  auto       start  = scheme == std::string_view::npos ? size_t{0} : url.find('/', scheme + 3);
  if (start == std::string_view::npos) {
    return {};
  }

  auto end = std::min(url.find('?', start), url.find('#', start));
  if (end == std::string_view::npos) {
    end = url.size();
  }

  return bounded_single_line(std::string{url.substr(start, end - start)}, kMaxRouteBytes);
}

ErrorSnapshot snapshot_error(void* error)
{
  ErrorSnapshot snapshot{.present = error != nullptr};
  if (error == nullptr) {
    return snapshot;
  }

  auto& helper                = gs_error_helper();
  snapshot.type               = read_field<int32_t>(error, helper, "<Type>k__BackingField");
  snapshot.code               = read_field<int32_t>(error, helper, "<Code>k__BackingField");
  snapshot.http_response_code = read_field<int32_t>(error, helper, "<HttpResponseCode>k__BackingField");
  snapshot.category = bounded_single_line(managed_string(read_field<void*>(error, helper, "<Category>k__BackingField")),
                                          kMaxCategoryBytes);
  snapshot.message  = bounded_single_line(managed_string(read_field<void*>(error, helper, "<Message>k__BackingField")),
                                          kMaxMessageBytes);
  snapshot.request_route =
      sanitize_request_route(managed_string(read_field<void*>(error, helper, "<RequestUrl>k__BackingField")));
  snapshot.transaction_id = bounded_single_line(
      managed_string(read_field<void*>(error, helper, "<TransactionId>k__BackingField")), kMaxTransactionIdBytes);
  return snapshot;
}

std::string_view error_type_name(int32_t type)
{
  switch (type) {
    case 1:
      return "network";
    case 2:
      return "platform";
    case 3:
      return "server";
    case 4:
      return "client";
    case 5:
      return "unknown";
    case 7:
      return "s3";
    default:
      return "unrecognized";
  }
}

std::string error_fingerprint(const ErrorSnapshot& error)
{
  return std::to_string(error.type) + '|' + std::to_string(error.code) + '|' + std::to_string(error.http_response_code)
         + '|' + error.category + '|' + error.message + '|' + error.request_route + '|' + error.transaction_id;
}

bool validated_handler_bytes(const uint8_t* entry)
{ return std::equal(kValidatedHandlerRelocationWindow.begin(), kValidatedHandlerRelocationWindow.end(), entry); }

bool handler_fingerprint_self_test()
{
  auto altered = kValidatedHandlerRelocationWindow;
  altered[10] ^= 0x01U;
  return validated_handler_bytes(kValidatedHandlerRelocationWindow.data()) && !validated_handler_bytes(altered.data());
}

bool validated_handler_entry(const MethodInfo* method)
{
  const auto game_assembly = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
  const auto entry         = reinterpret_cast<uintptr_t>(method->methodPointer);
  if (game_assembly == 0 || entry != game_assembly + kValidatedHandlerRva) {
    return false;
  }
  return validated_handler_bytes(reinterpret_cast<const uint8_t*>(entry));
}

int64_t unix_timestamp_milliseconds()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void record_session_start()
{
  auto&                  state = probe_state();
  nlohmann::ordered_json event{
      {"schema", "stfc-science-game-error/v2"},
      {"event", "session-start"},
      {"session_id", state.session_id},
      {"timestamp_unix_ms", unix_timestamp_milliseconds()},
      {"client_build", 260},
      {"expected_game_assembly_sha256", "3B219F2556F677C818C892B06D091C154D7FBDA9DC459A4A0F80F95D35AC1C47"},
      {"mod_version", VER_FILE_VERSION_STR},
      {"source_state", STFC_SOURCE_STATE_ID},
      {"base_commit", STFC_BASE_COMMIT},
  };
  state.logger->info("{}", event.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
}

void publish_console_summary(const ErrorSnapshot& error)
{
  dev::diagnostics::PublishLazy(s_console_source, dev::diagnostics::Severity::Error,
                                [&error] { return compose_console_summary(error); });
}

void record_error(const ErrorSnapshot& error)
{
  auto&            state       = probe_state();
  const auto       now_steady  = std::chrono::steady_clock::now();
  const auto       fingerprint = error_fingerprint(error);
  std::scoped_lock lock{state.mutex};

  if (!state.last_fingerprint.empty() && fingerprint == state.last_fingerprint
      && now_steady - state.last_event_at < kDuplicateWindow) {
    ++state.duplicate_events_suppressed;
    state.last_event_at = now_steady;
    return;
  }

  if (state.rate_window_started_at == std::chrono::steady_clock::time_point{}
      || now_steady - state.rate_window_started_at >= kRateWindow) {
    state.rate_window_started_at = now_steady;
    state.emitted_in_rate_window = 0;
  }
  if (state.emitted_in_rate_window >= kMaximumEventsPerMinute) {
    ++state.rate_limited_events;
    return;
  }

  nlohmann::ordered_json event{
      {"schema", "stfc-science-game-error/v2"},
      {"event", "game-error"},
      {"session_id", state.session_id},
      {"sequence", state.next_sequence++},
      {"timestamp_unix_ms", unix_timestamp_milliseconds()},
      {"source", "GsErrorHandler.OnGSError"},
      {"client_build", 260},
      {"expected_game_assembly_sha256", "3B219F2556F677C818C892B06D091C154D7FBDA9DC459A4A0F80F95D35AC1C47"},
      {"mod_version", VER_FILE_VERSION_STR},
      {"source_state", STFC_SOURCE_STATE_ID},
      {"base_commit", STFC_BASE_COMMIT},
      {"error",
       {{"present", error.present},
        {"type", error.type},
        {"type_name", error_type_name(error.type)},
        {"code", error.code},
        {"http_response_code", error.http_response_code},
        {"category", error.category},
        {"message", error.message},
        {"request_route", error.request_route},
        {"transaction_id", error.transaction_id}}},
      {"prior_duplicate_events_suppressed", state.duplicate_events_suppressed},
      {"prior_rate_limited_events", state.rate_limited_events},
  };

  state.logger->info("{}", event.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
  ++state.emitted_in_rate_window;
  state.last_fingerprint            = fingerprint;
  state.last_event_at               = now_steady;
  state.duplicate_events_suppressed = 0;
  state.rate_limited_events         = 0;
  publish_console_summary(error);
}

void GsErrorHandler_OnGSError_Hook(auto original, void* handler, void* error)
{
  ErrorSnapshot snapshot;
  bool          captured = false;
  try {
    snapshot = snapshot_error(error);
    captured = true;
  } catch (...) {
  }

  original(handler, error);

  if (captured) {
    try {
      record_error(snapshot);
    } catch (...) {
    }
  }
}
} // namespace
#endif

void InstallGameErrorProbe()
{
#if defined(_MODDBG) && defined(_WIN32)
  if (!probe_requested()) {
    spdlog::debug("[GameErrorProbe] disabled; set {}=1 before launch to enable", kProbeEnvironmentVariable);
    return;
  }

  auto& handler_helper = error_handler_helper();
  auto& error_helper   = gs_error_helper();
  if (!handler_helper.isValidHelper() || !error_helper.isValidHelper() || !required_error_fields_available()) {
    spdlog::error("[GameErrorProbe] required class metadata is unavailable; probe not installed");
    return;
  }

  const auto method = handler_helper.GetMethodInfo("OnGSError", 1);
  if (method == nullptr || method->methodPointer == nullptr) {
    spdlog::error("[GameErrorProbe] GsErrorHandler.OnGSError(GSError) is unavailable; probe not installed");
    return;
  }
  if (!validated_handler_entry(method)) {
    spdlog::error("[GameErrorProbe] build-260 handler identity check failed; probe not installed");
    return;
  }
  if (!handler_fingerprint_self_test()) {
    spdlog::error("[GameErrorProbe] handler fingerprint self-test failed; probe not installed");
    return;
  }
  if (!utf8_boundary_self_test()) {
    spdlog::error("[GameErrorProbe] UTF-8 boundary self-test failed; probe not installed");
    return;
  }
  if (!console_display_self_test()) {
    spdlog::error("[GameErrorProbe] console display self-test failed; probe not installed");
    return;
  }

  try {
    auto& state = probe_state();
    state.logger =
        spdlog::rotating_logger_mt("science-game-errors", std::string{kProbeLogFilename}, 1024 * 1024, 2, false);
    state.logger->set_pattern("%v");
    state.logger->set_level(spdlog::level::info);
    state.logger->flush_on(spdlog::level::info);
    state.session_id = std::to_string(GetCurrentProcessId()) + "-" + std::to_string(unix_timestamp_milliseconds());
  } catch (const std::exception& error) {
    spdlog::error("[GameErrorProbe] failed to open {}: {}; probe not installed", kProbeLogFilename, error.what());
    return;
  }

  if (SPUD_STATIC_DETOUR(method->methodPointer, GsErrorHandler_OnGSError_Hook) == nullptr) {
    spdlog::error("[GameErrorProbe] detour installation failed; probe not installed");
    return;
  }

  try {
    record_session_start();
  } catch (...) {
    spdlog::warn("[GameErrorProbe] session marker could not be written; error capture remains enabled");
  }

  spdlog::warn("[GameErrorProbe] SCIENCE probe enabled; writing bounded events to {}", kProbeLogFilename);
#endif
}
