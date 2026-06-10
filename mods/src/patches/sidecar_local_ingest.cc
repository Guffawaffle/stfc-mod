#include "patches/sidecar_local_ingest.h"

#include "config.h"
#include "patches/async_work_queue.h"
#include "patches/sidecar_local_ingest_policy.h"
#include "patches/sync_transport.h"
#include "patches/sync_transport_policy.h"
#include "version.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using json = nlohmann::json;

struct SidecarLocalWorkItem {
  SidecarLocalIngestKind kind = SidecarLocalIngestKind::BattleEvents;
  json                   payload;
};

constexpr size_t kSidecarLocalQueueMaxDepth = 128;
constexpr auto   kSidecarLocalBatchQuietFor = std::chrono::milliseconds(75);
constexpr auto   kSidecarLocalJoinWarnAfter = std::chrono::seconds(2);
constexpr int    kSidecarLocalConnectTimeoutMs = 2500;
constexpr int    kSidecarLocalRequestTimeoutMs = 8000;
constexpr auto   kSidecarLocalInitialBackoff = std::chrono::seconds(15);
constexpr auto   kSidecarLocalMaxBackoff = std::chrono::minutes(2);
constexpr auto   kSidecarLocalBackoffLogEvery = std::chrono::seconds(15);

AsyncWorkQueue<SidecarLocalWorkItem> s_sidecar_local_queue(kSidecarLocalQueueMaxDepth);
std::once_flag                       s_sidecar_local_worker_once;
std::thread                          s_sidecar_local_worker_thread;
std::atomic_uint64_t                 s_sidecar_local_batch_counter = 0;
std::atomic_uint64_t                 s_fleet_runtime_transport_mode_suppressed = 0;

std::mutex                            s_transport_backoff_mutex;
std::chrono::steady_clock::time_point s_transport_backoff_until;
std::chrono::steady_clock::time_point s_transport_backoff_last_log;
uint32_t                              s_transport_consecutive_failures = 0;
uint64_t                              s_transport_backoff_suppressed = 0;

const char* sidecar_local_kind_name(const SidecarLocalIngestKind kind)
{
  switch (kind) {
    case SidecarLocalIngestKind::BattleEvents: return "battle.events";
    case SidecarLocalIngestKind::FleetRuntime: return "fleet.runtime";
  }

  return "unknown";
}

std::string_view fleet_runtime_mode()
{ return SidecarSyncSettings().fleet_runtime_mode; }

bool fleet_runtime_mode_is(std::string_view mode)
{ return fleet_runtime_mode() == mode; }

std::chrono::milliseconds sidecar_local_backoff_delay_for_failure_count(uint32_t failure_count)
{
  if (failure_count == 0) {
    failure_count = 1;
  }

  const auto multiplier = 1 << std::min<uint32_t>(failure_count - 1, 3);
  return std::min(std::chrono::duration_cast<std::chrono::milliseconds>(kSidecarLocalInitialBackoff * multiplier),
                  std::chrono::duration_cast<std::chrono::milliseconds>(kSidecarLocalMaxBackoff));
}

bool sidecar_local_transport_backoff_active(SidecarLocalIngestKind kind)
{
  const auto now = std::chrono::steady_clock::now();
  uint64_t   suppressed = 0;
  int64_t    remaining_ms = 0;
  bool       should_log = false;

  {
    std::lock_guard lock(s_transport_backoff_mutex);
    if (now >= s_transport_backoff_until) {
      return false;
    }

    ++s_transport_backoff_suppressed;
    suppressed = s_transport_backoff_suppressed;
    remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(s_transport_backoff_until - now).count();
    if (s_transport_backoff_last_log.time_since_epoch().count() == 0
        || now - s_transport_backoff_last_log >= kSidecarLocalBackoffLogEvery) {
      s_transport_backoff_last_log = now;
      should_log = true;
    }
  }

  if (should_log) {
    spdlog::info("[SidecarLocal] kind={} transport=suppressed reason=backoff remainingMs={} suppressed={}",
                 sidecar_local_kind_name(kind),
                 remaining_ms,
                 suppressed);
  }
  return true;
}

void sidecar_local_transport_failure(SidecarLocalIngestKind kind, std::string_view message)
{
  uint32_t failures = 0;
  int64_t  backoff_ms = 0;
  {
    std::lock_guard lock(s_transport_backoff_mutex);
    failures = ++s_transport_consecutive_failures;
    const auto delay = sidecar_local_backoff_delay_for_failure_count(failures);
    s_transport_backoff_until = std::chrono::steady_clock::now() + delay;
    s_transport_backoff_last_log = {};
    backoff_ms = delay.count();
  }

  spdlog::warn("[SidecarLocal] {} send failed: {}; backoffMs={} consecutiveFailures={}",
               sidecar_local_kind_name(kind),
               message,
               backoff_ms,
               failures);
}

void sidecar_local_transport_success(SidecarLocalIngestKind kind)
{
  uint32_t failures = 0;
  uint64_t suppressed = 0;
  {
    std::lock_guard lock(s_transport_backoff_mutex);
    failures = s_transport_consecutive_failures;
    suppressed = s_transport_backoff_suppressed;
    s_transport_consecutive_failures = 0;
    s_transport_backoff_suppressed = 0;
    s_transport_backoff_until = {};
    s_transport_backoff_last_log = {};
  }

  if (failures > 0 || suppressed > 0) {
    spdlog::info("[SidecarLocal] kind={} transport=recovered previousFailures={} suppressedDuringBackoff={}",
                 sidecar_local_kind_name(kind),
                 failures,
                 suppressed);
  }
}

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string current_time_iso_utc()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_time = std::chrono::system_clock::to_time_t(now);

  std::tm utc{};
#if _WIN32
  gmtime_s(&utc, &now_time);
#else
  gmtime_r(&now_time, &utc);
#endif

  char buffer[sizeof("2026-05-17T22:00:00Z")];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
    return {};
  }

  return buffer;
}

std::string sidecar_local_session_id()
{
  static const std::string session_id = std::format("sidecar-local-{}", current_time_millis_utc());
  return session_id;
}

std::string next_sidecar_batch_id(const SidecarLocalIngestKind kind)
{
  const auto sequence = s_sidecar_local_batch_counter.fetch_add(1, std::memory_order_relaxed);
  const auto prefix = kind == SidecarLocalIngestKind::BattleEvents ? "battle" : "fleet";
  return std::format("{}-{}-{}", prefix, current_time_millis_utc(), sequence);
}

SyncConfig sidecar_sync_transport_config(const SidecarSyncConfig& config)
{
  SyncConfig transport_config;
  transport_config.verify_ssl = config.verify_ssl;
  transport_config.allow_unsafe_tls_without_certificate_validation =
      config.allow_unsafe_tls_without_certificate_validation;
  return transport_config;
}

bool apply_sidecar_tls_policy(cpr::Session& session, const SidecarSyncConfig& config)
{
  const auto decision = http::DecideSyncTlsVerification(sidecar_sync_transport_config(config));
  if (decision.warn_verify_ssl_ignored) {
    spdlog::warn("[SidecarLocal] verify_ssl=false ignored because allow_unsafe_tls_without_certificate_validation=false");
  }

  if (decision.emit_unsafe_tls_error) {
    spdlog::error("[SidecarLocal] UNSAFE TLS certificate verification disabled for local sidecar delivery.");
  }

  if (!decision.disable_verification) {
    return false;
  }

  session.SetSslOptions(cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true}));
  return true;
}

std::shared_ptr<cpr::Session> create_sidecar_local_session()
{
  const auto& config = SidecarSyncSettings();
  if (!SidecarLocalSyncTransportReady(config)) {
    return nullptr;
  }

  auto session = std::make_shared<cpr::Session>();
  session->SetUrl(config.url);
  session->SetUserAgent("stfc community patch " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
  session->SetAcceptEncoding(cpr::AcceptEncoding{});
  session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
  session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});
  session->SetConnectTimeout(cpr::ConnectTimeout{kSidecarLocalConnectTimeoutMs});
  session->SetTimeout(cpr::Timeout{kSidecarLocalRequestTimeoutMs});

  if (!config.proxy.empty()) {
    session->SetProxies({{"http", config.proxy}, {"https", config.proxy}});
  }

  apply_sidecar_tls_policy(*session, config);
  session->SetHeader({
      {"Content-Type", "application/json"},
      {"X-Powered-By", http::headers::poweredBy},
      {"stfc-sync-token", config.token},
  });

  return session;
}

json build_sidecar_local_envelope(const SidecarLocalIngestKind kind, const json& payload)
{
  const char* payload_protocol = nullptr;
  switch (kind) {
    case SidecarLocalIngestKind::BattleEvents:
      if (!payload.is_array() || payload.empty()) {
        return nullptr;
      }
      payload_protocol = "stfc.sidecar.events.v0";
      break;
    case SidecarLocalIngestKind::FleetRuntime:
      if (!payload.is_object()) {
        return nullptr;
      }
      payload_protocol = "stfc.fleet.runtime_snapshot.v1";
      break;
  }

  return json{
      {"protocolVersion", "stfc.sidecar.ingest.v1"},
      {"kind", sidecar_local_kind_name(kind)},
      {"batchId", next_sidecar_batch_id(kind)},
      {"producedAt", current_time_iso_utc()},
      {"sessionId", sidecar_local_session_id()},
      {"source", "stfc-community-mod"},
      {"modVersion", VER_FILE_VERSION_STR},
      {"payloadProtocol", payload_protocol},
      {"payload", payload},
  };
}

void post_sidecar_local_envelope(cpr::Session& session, const SidecarLocalIngestKind kind, const json& payload)
{
  if (sidecar_local_transport_backoff_active(kind)) {
    return;
  }

  const auto envelope = build_sidecar_local_envelope(kind, payload);
  if (envelope.is_null()) {
    return;
  }

  session.SetBody(cpr::Body{envelope.dump()});
  const auto response = session.Post();
  if (response.error.code != cpr::ErrorCode::OK) {
    sidecar_local_transport_failure(kind, response.error.message);
    return;
  }

  if (response.status_code < 200 || response.status_code >= 300) {
    sidecar_local_transport_failure(kind, std::format("HTTP {}", response.status_code));
    return;
  }

  sidecar_local_transport_success(kind);
  spdlog::debug("[SidecarLocal] Sent {} payload to {}", sidecar_local_kind_name(kind), SidecarSyncSettings().url);
}

void process_sidecar_local_batch(cpr::Session& session, std::vector<SidecarLocalWorkItem>&& batch)
{
  json                battle_events = json::array();
  std::optional<json> fleet_runtime;

  for (auto& item : batch) {
    switch (item.kind) {
      case SidecarLocalIngestKind::BattleEvents:
        if (item.payload.is_array()) {
          for (auto& event : item.payload) {
            battle_events.push_back(std::move(event));
          }
        }
        break;
      case SidecarLocalIngestKind::FleetRuntime:
        if (item.payload.is_object()) {
          fleet_runtime = std::move(item.payload);
        }
        break;
    }
  }

  if (!battle_events.empty()) {
    post_sidecar_local_envelope(session, SidecarLocalIngestKind::BattleEvents, battle_events);
  }
  if (fleet_runtime.has_value()) {
    if (fleet_runtime_mode_is("enqueue_no_transport")) {
      const auto suppressed = s_fleet_runtime_transport_mode_suppressed.fetch_add(1, std::memory_order_relaxed) + 1;
      spdlog::info("[SidecarLocal] kind=fleet.runtime transport=suppressed reason=enqueue-no-transport count={}",
                   suppressed);
    } else {
      post_sidecar_local_envelope(session, SidecarLocalIngestKind::FleetRuntime, *fleet_runtime);
    }
  }
}

void sidecar_local_worker_main()
{
  s_sidecar_local_queue.set_worker_active(true);
  auto session = create_sidecar_local_session();
  if (!session) {
    s_sidecar_local_queue.record_worker_error();
    s_sidecar_local_queue.set_worker_active(false);
    return;
  }

  try {
    for (;;) {
      auto batch = s_sidecar_local_queue.wait_for_batch_after_quiet(kSidecarLocalBatchQuietFor);
      if (batch.empty()) {
        if (s_sidecar_local_queue.shutdown_requested()) {
          break;
        }
        continue;
      }

      process_sidecar_local_batch(*session, std::move(batch));
    }
  } catch (const std::exception& exception) {
    s_sidecar_local_queue.record_worker_error();
    spdlog::error("[SidecarLocal] Worker failed: {}", exception.what());
  } catch (...) {
    s_sidecar_local_queue.record_worker_error();
    spdlog::error("[SidecarLocal] Worker failed with an unknown exception");
  }

  s_sidecar_local_queue.set_worker_active(false);
}

void ensure_sidecar_local_worker_started()
{
  std::call_once(s_sidecar_local_worker_once, [] { s_sidecar_local_worker_thread = std::thread(sidecar_local_worker_main); });
}

bool enqueue_sidecar_local_payload(const SidecarLocalIngestKind kind, const json& payload)
{
  if (!SidecarLocalSyncEnabledFor(SidecarSyncSettings(), kind)) {
    return false;
  }

  ensure_sidecar_local_worker_started();
  if (s_sidecar_local_queue.enqueue({kind, payload})) {
    return true;
  }

  const auto diagnostics = s_sidecar_local_queue.diagnostics();
  spdlog::warn("[SidecarLocal] Dropped {} payload because queue is full (depth={}, dropped={})",
               sidecar_local_kind_name(kind), diagnostics.depth, diagnostics.dropped);
  return false;
}

sidecar_local_ingest::EnqueueResult enqueue_fleet_runtime_payload(const json& payload)
{
  sidecar_local_ingest::EnqueueResult result;
  if (!SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::FleetRuntime)) {
    return result;
  }

  ensure_sidecar_local_worker_started();

  bool coalesced = false;
  result.accepted = s_sidecar_local_queue.enqueue_or_replace(
      {SidecarLocalIngestKind::FleetRuntime, payload},
      [](const SidecarLocalWorkItem& item) { return item.kind == SidecarLocalIngestKind::FleetRuntime; },
      coalesced);
  result.coalesced = coalesced;

  const auto diagnostics = s_sidecar_local_queue.diagnostics();
  result.depth = diagnostics.depth;
  result.enqueued = diagnostics.enqueued;
  result.dropped = diagnostics.dropped;
  result.coalesced_total = diagnostics.coalesced;

  if (result.accepted) {
    spdlog::debug("[SidecarLocal] kind=fleet.runtime enqueue=accepted coalesced={} depth={} enqueued={} "
                  "coalescedTotal={} dropped={}",
                  result.coalesced,
                  result.depth,
                  result.enqueued,
                  result.coalesced_total,
                  result.dropped);
    return result;
  }

  spdlog::warn("[SidecarLocal] Dropped fleet.runtime payload because queue is full or stopped (depth={}, dropped={})",
               result.depth,
               result.dropped);
  return result;
}
} // namespace

namespace sidecar_local_ingest
{
bool BattleEventsEnabled()
{ return SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::BattleEvents); }

bool FleetRuntimeEnabled()
{ return SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::FleetRuntime); }

std::string_view FleetRuntimeMode()
{ return fleet_runtime_mode(); }

bool FleetRuntimeRequestOnlyMode()
{ return fleet_runtime_mode_is("request_only"); }

bool FleetRuntimeSnapshotOnlyMode()
{ return fleet_runtime_mode_is("snapshot_only"); }

bool FleetRuntimeEnqueueNoTransportMode()
{ return fleet_runtime_mode_is("enqueue_no_transport"); }

bool EnqueueBattleEvents(const nlohmann::json& events)
{ return enqueue_sidecar_local_payload(SidecarLocalIngestKind::BattleEvents, events); }

EnqueueResult EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload)
{ return enqueue_fleet_runtime_payload(payload); }

void Shutdown()
{
  s_sidecar_local_queue.request_shutdown();
  if (!s_sidecar_local_worker_thread.joinable()) {
    return;
  }

  const auto join_started_at = std::chrono::steady_clock::now();
  s_sidecar_local_worker_thread.join();
  const auto join_elapsed = std::chrono::steady_clock::now() - join_started_at;
  if (join_elapsed > kSidecarLocalJoinWarnAfter) {
    spdlog::warn("[SidecarLocal] Worker join waited {} ms during shutdown",
                 std::chrono::duration_cast<std::chrono::milliseconds>(join_elapsed).count());
  }
}
} // namespace sidecar_local_ingest
