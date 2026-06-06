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

AsyncWorkQueue<SidecarLocalWorkItem> s_sidecar_local_queue(kSidecarLocalQueueMaxDepth);
std::once_flag                       s_sidecar_local_worker_once;
std::thread                          s_sidecar_local_worker_thread;
std::atomic_uint64_t                 s_sidecar_local_batch_counter = 0;

const char* sidecar_local_kind_name(const SidecarLocalIngestKind kind)
{
  switch (kind) {
    case SidecarLocalIngestKind::BattleEvents: return "battle.events";
    case SidecarLocalIngestKind::FleetRuntime: return "fleet.runtime";
    case SidecarLocalIngestKind::ObservedHostiles: return "observed.hostiles";
  }

  return "unknown";
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
  const auto prefix = [&]() {
    switch (kind) {
      case SidecarLocalIngestKind::BattleEvents: return "battle";
      case SidecarLocalIngestKind::FleetRuntime: return "fleet";
      case SidecarLocalIngestKind::ObservedHostiles: return "observed";
    }

    return "sidecar";
  }();
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
    case SidecarLocalIngestKind::ObservedHostiles:
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
  const auto envelope = build_sidecar_local_envelope(kind, payload);
  if (envelope.is_null()) {
    return;
  }

  session.SetBody(cpr::Body{envelope.dump()});
  const auto response = session.Post();
  if (response.error.code != cpr::ErrorCode::OK) {
    spdlog::warn("[SidecarLocal] {} send failed: {}", sidecar_local_kind_name(kind), response.error.message);
    return;
  }

  if (response.status_code < 200 || response.status_code >= 300) {
    spdlog::warn("[SidecarLocal] {} send failed with HTTP {}", sidecar_local_kind_name(kind), response.status_code);
    return;
  }

  spdlog::debug("[SidecarLocal] Sent {} payload to {}", sidecar_local_kind_name(kind), SidecarSyncSettings().url);
}

void process_sidecar_local_batch(cpr::Session& session, std::vector<SidecarLocalWorkItem>&& batch)
{
  json                battle_events     = json::array();
  json                observed_hostiles = json::array();
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
      case SidecarLocalIngestKind::ObservedHostiles:
        if (item.payload.is_array()) {
          for (auto& event : item.payload) {
            observed_hostiles.push_back(std::move(event));
          }
        }
        break;
    }
  }

  if (!battle_events.empty()) {
    post_sidecar_local_envelope(session, SidecarLocalIngestKind::BattleEvents, battle_events);
  }
  if (fleet_runtime.has_value()) {
    post_sidecar_local_envelope(session, SidecarLocalIngestKind::FleetRuntime, *fleet_runtime);
  }
  if (!observed_hostiles.empty()) {
    post_sidecar_local_envelope(session, SidecarLocalIngestKind::ObservedHostiles, observed_hostiles);
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
} // namespace

namespace sidecar_local_ingest
{
bool BattleEventsEnabled()
{ return SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::BattleEvents); }

bool FleetRuntimeEnabled()
{ return SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::FleetRuntime); }

bool ObservedHostilesEnabled()
{ return SidecarLocalSyncEnabledFor(SidecarSyncSettings(), SidecarLocalIngestKind::ObservedHostiles); }

bool EnqueueBattleEvents(const nlohmann::json& events)
{ return enqueue_sidecar_local_payload(SidecarLocalIngestKind::BattleEvents, events); }

bool EnqueueFleetRuntimeSnapshot(const nlohmann::json& payload)
{ return enqueue_sidecar_local_payload(SidecarLocalIngestKind::FleetRuntime, payload); }

bool EnqueueObservedHostileEvents(const nlohmann::json& events)
{ return enqueue_sidecar_local_payload(SidecarLocalIngestKind::ObservedHostiles, events); }

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
