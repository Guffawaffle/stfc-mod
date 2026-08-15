/**
 * @file sync_transport.cc
 * @brief HTTP transport and Scopely API client for sync pipelines.
 */
#include "patches/sync_transport.h"

#include "patches/sync_transport_policy.h"

#include "errormsg.h"
#include "str_utils.h"
#include "version.h"

#include <cpr/cpr.h>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#if _WIN32
#include <rpc.h>
#include <winrt/Windows.Foundation.h>
#else
#include <uuid/uuid.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <format>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#ifndef STR_FORMAT
#if __cpp_lib_format
#define STR_FORMAT std::format
#else
#define STR_FORMAT fmt::format
#endif
#endif

#if _WIN32
struct WinRtApartmentGuard {
  WinRtApartmentGuard()
  { winrt::init_apartment(); }
  ~WinRtApartmentGuard()
  { winrt::uninit_apartment(); }
};
#endif

namespace http
{
namespace headers
{
  namespace
  {
    std::mutex  header_mtx;
    std::string gameServerUrl;
    std::string instanceSessionId;
    int32_t     instanceId = 0;
    std::string unityVersion{"6000.0.52f1"};
    std::string primeVersion{"1.000.45324"};
  } // namespace

  const char poweredBy[] = "stfc community patch/" VER_RUNTIME_VERSION_STR;

  void SetPrimeServerHeaders(std::string serverUrl, std::string sessionId)
  {
    std::lock_guard lk(header_mtx);
    gameServerUrl     = std::move(serverUrl);
    instanceSessionId = std::move(sessionId);
  }

  void SetPrimeVersion(std::string version)
  {
    std::lock_guard lk(header_mtx);
    primeVersion = std::move(version);
  }

  void SetInstanceId(int32_t value)
  {
    std::lock_guard lk(header_mtx);
    instanceId = value;
  }

  SessionHeaderSnapshot Snapshot()
  {
    std::lock_guard lk(header_mtx);
    return {
        gameServerUrl, instanceSessionId, instanceId, unityVersion, primeVersion,
    };
  }
} // namespace headers

#ifdef _MODDBG
constexpr int kSyncConnectTimeoutMs = 10'000;
constexpr int kSyncRequestTimeoutMs = 30'000;
#else
constexpr int kSyncConnectTimeoutMs = 3'000;
constexpr int kSyncRequestTimeoutMs = 10'000;
#endif

bool should_disable_tls_verification(const SyncConfig& config, const std::string& target_identifier)
{
  const auto decision = DecideSyncTlsVerification(config);

  if (decision.warn_verify_ssl_ignored) {
    spdlog::warn("[Sync] Ignoring verify_ssl=false for '{}' because allow_unsafe_tls_without_certificate_validation "
                 "is not true.",
                 target_identifier);
  }

  if (decision.emit_unsafe_tls_error) {
    spdlog::error(
        "[Sync] UNSAFE TLS certificate verification disabled for '{}'. Traffic can be intercepted. Set "
        "verify_ssl=true or remove allow_unsafe_tls_without_certificate_validation to restore safe transport.",
        target_identifier);
  }

  return decision.disable_verification;
}

[[nodiscard]] static std::string newUUID()
{
#ifdef _WIN32
  UUID       uuid{};
  const auto create_status = UuidCreate(&uuid);
  if (create_status != RPC_S_OK && create_status != RPC_S_UUID_LOCAL_ONLY) {
    spdlog::warn("[Sync] Failed to create UUID for request headers: status={}", create_status);
    return {};
  }

  unsigned char* str              = nullptr;
  const auto     stringify_status = UuidToStringA(&uuid, &str);
  if (stringify_status != RPC_S_OK || !str) {
    spdlog::warn("[Sync] Failed to stringify UUID for request headers: status={}", stringify_status);
    if (str) {
      RpcStringFreeA(&str);
    }
    return {};
  }

  std::string result(reinterpret_cast<char*>(str));

  RpcStringFreeA(&str);
#else
  uuid_t uuid;
  uuid_generate_random(uuid);
  char result[37];
  uuid_unparse(uuid, result);
#endif
  return result;
}

class Url
{
public:
  explicit Url(const std::string& url)
      : url_(url)
  {
    handle_ = curl_url();
    if (handle_) {
      curl_url_set(handle_, CURLUPART_URL, url_.data(), 0);
    }
  }

  ~Url()
  {
    if (handle_) {
      curl_url_cleanup(handle_);
    }
  }

  Url(const Url&)            = delete;
  Url& operator=(const Url&) = delete;

  Url(Url&& other) noexcept
      : handle_(other.handle_)
      , url_(std::move(other.url_))
  { other.handle_ = nullptr; }

  Url& operator=(Url&& other) noexcept
  {
    if (this != &other) {
      if (handle_) {
        curl_url_cleanup(handle_);
      }

      handle_       = other.handle_;
      url_          = std::move(other.url_);
      other.handle_ = nullptr;
    }

    return *this;
  }

  void set_path(const std::string& path)
  {
    if (!handle_) {
      return;
    }

    if (CURLUcode result_code = curl_url_set(handle_, CURLUPART_PATH, path.c_str(), 0); result_code == CURLUE_OK) {
      char* url = nullptr;
      if (result_code = curl_url_get(handle_, CURLUPART_URL, &url, CURLU_PUNYCODE); result_code == CURLUE_OK) {
        url_ = url;
      }

      if (url != nullptr) {
        curl_free(url);
      }
    }
  }

  [[nodiscard]] const char* c_str() const
  { return url_.c_str(); }

private:
  CURLU*      handle_ = nullptr;
  std::string url_;
};

void sync_log_error(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::error("SYNC-{} - {}: {}", type, target, text);
  }
}

void sync_log_warn(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::warn("SYNC-{} - {}: {}", type, target, text);
  }
}

void sync_log_info(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::info("SYNC-{} - {}: {}", type, target, text);
  }
}

void sync_log_debug(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging && Config::Get().sync_debug) {
    spdlog::debug("SYNC-{} - {}: {}", type, target, text);
  }
}

void sync_log_trace(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging && Config::Get().sync_debug) {
    spdlog::trace("SYNC-{} - {}: {}", type, target, text);
  }
}

static const std::string CURL_TYPE_UPLOAD               = "UPLOAD";
static const std::string CURL_TYPE_DOWNLOAD             = "DOWNLOAD";
static constexpr size_t  kTargetWorkerMaxQueuedRequests = 256;
static constexpr size_t  kMajelIngestMaxEventBytes      = 256 * 1024;

struct TargetWorker {
  TargetWorker()                               = default;
  TargetWorker(const TargetWorker&)            = delete;
  TargetWorker& operator=(const TargetWorker&) = delete;

  struct Request {
    std::string target_identifier;
    std::string post_data;
    bool        is_first_sync = false;
  };

  std::shared_ptr<cpr::Session> session;
  SyncTargetConfig::Mode        mode = SyncTargetConfig::Mode::Legacy;
  std::thread                   worker_thread;
  std::atomic_bool              stop_requested{false};
  std::queue<Request>           request_queue;
  std::mutex                    queue_mtx;
  std::condition_variable       queue_cv;
  uint64_t                      dropped_requests = 0;
};

static std::unordered_map<std::string, std::shared_ptr<TargetWorker>> target_workers;
static std::mutex                                                     target_workers_mtx;
static std::atomic_bool                                               target_workers_shutdown_requested = false;
static std::atomic_uint64_t                                           majel_event_sequence              = 0;

std::string current_time_iso_utc()
{
  const auto now      = std::chrono::system_clock::now();
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

std::string majel_session_id()
{
  static const std::string session_id = [] {
    auto value = newUUID();
    return value.empty() ? std::string{"unknown-session"} : value;
  }();
  return session_id;
}

std::string make_target_post_data(const SyncTargetConfig& target_config, SyncConfig::Type type,
                                  const std::string& post_data, const std::string& target_identifier)
{
  if (!SyncTargetUsesMajelEnvelope(target_config.mode)) {
    return post_data;
  }

  auto payload = nlohmann::json::parse(post_data, nullptr, false);
  if (payload.is_discarded()) {
    sync_log_warn(CURL_TYPE_UPLOAD, target_identifier,
                  "Dropping Majel ingest event because the sync payload was not valid JSON");
    return {};
  }

  const auto sequence = majel_event_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  auto       event_id = newUUID();
  if (event_id.empty()) {
    event_id = STR_FORMAT("stfc-community-mod-{}", sequence);
  }

  auto envelope = BuildMajelIngestEnvelope({
                                               .sync_type      = type,
                                               .payload        = std::move(payload),
                                               .event_id       = std::move(event_id),
                                               .source_version = VER_RUNTIME_VERSION_STR,
                                               .install_id     = "not_configured",
                                               .session_id     = majel_session_id(),
                                               .sequence       = sequence,
                                               .observed_at    = current_time_iso_utc(),
                                           })
                      .dump();

  if (envelope.size() > kMajelIngestMaxEventBytes) {
    sync_log_warn(CURL_TYPE_UPLOAD, target_identifier,
                  STR_FORMAT("Dropping Majel ingest event because the envelope is too large ({} bytes, max: {})",
                             envelope.size(), kMajelIngestMaxEventBytes));
    return {};
  }

  return envelope;
}

static void target_worker_thread(std::shared_ptr<TargetWorker> worker)
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  while (!worker->stop_requested.load(std::memory_order_acquire)) {
    TargetWorker::Request request;

    {
      std::unique_lock lk(worker->queue_mtx);
      worker->queue_cv.wait(lk, [&worker] {
        return worker->stop_requested.load(std::memory_order_acquire) || !worker->request_queue.empty();
      });

      if (worker->stop_requested.load(std::memory_order_acquire) && worker->request_queue.empty()) {
        break;
      }

      if (!worker->request_queue.empty()) {
        auto item = std::move(worker->request_queue.front());
        worker->request_queue.pop();
        request = std::move(item);
      }
    }

    if (request.post_data.empty()) {
      continue;
    }

    try {
      const auto httpClient      = worker->session;
      auto&      request_headers = httpClient->GetHeader();

      if (worker->mode == SyncTargetConfig::Mode::Legacy && request.is_first_sync) {
        request_headers.insert_or_assign("X-PRIME-SYNC", "2");
        sync_log_trace(CURL_TYPE_UPLOAD, request.target_identifier, "Adding X-Prime-Sync header for initial sync");
      } else {
        request_headers.erase("X-PRIME-SYNC");
      }

      httpClient->SetBody(cpr::Body{request.post_data});

      sync_log_debug(CURL_TYPE_UPLOAD, request.target_identifier, "Sending data to " + httpClient->GetFullRequestUrl());

      const auto response = httpClient->Post();

      if (response.status_code == 0) {
        sync_log_error(CURL_TYPE_UPLOAD, request.target_identifier,
                       "Failed to send request: " + response.error.message);
      } else if (response.status_code >= 400) {
        sync_log_error(CURL_TYPE_UPLOAD, request.target_identifier,
                       STR_FORMAT("Failed to communicate with server: {} (after {:.1f}s)", response.status_line,
                                  response.elapsed));
      } else {
        sync_log_debug(CURL_TYPE_UPLOAD, request.target_identifier,
                       STR_FORMAT("Response: {} ({:.1f}s elapsed)", response.status_line, response.elapsed));
      }
    } catch (const std::runtime_error& exception) {
      ErrorMsg::SyncRuntime(request.target_identifier.c_str(), exception);
    } catch (const std::exception& exception) {
      ErrorMsg::SyncException(request.target_identifier.c_str(), exception);
#if _WIN32
    } catch (winrt::hresult_error const& exception) {
      ErrorMsg::SyncWinRT(request.target_identifier.c_str(), exception);
#endif
    } catch (...) {
      ErrorMsg::SyncMsg(request.target_identifier.c_str(), "Unknown error occurred");
    }
  }
}

static std::shared_ptr<TargetWorker> get_curl_client_sync(const std::string& target)
{
  std::lock_guard lk(target_workers_mtx);

  if (target_workers_shutdown_requested.load(std::memory_order_acquire)) {
    throw std::runtime_error("sync transport shutdown is in progress");
  }

  if (const auto found = target_workers.find(target); found != target_workers.end()) {
    return found->second;
  }

  auto worker               = std::make_shared<TargetWorker>();
  worker->session           = std::make_shared<cpr::Session>();
  const auto& target_config = Config::Get().sync_targets[target];
  worker->mode              = target_config.mode;

  worker->session->SetUrl(target_config.url);
  worker->session->SetUserAgent("stfc community patch " VER_RUNTIME_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
  worker->session->SetAcceptEncoding(cpr::AcceptEncoding{});
  worker->session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
  worker->session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});

  worker->session->SetConnectTimeout(cpr::ConnectTimeout{kSyncConnectTimeoutMs});
  worker->session->SetTimeout(cpr::Timeout{kSyncRequestTimeoutMs});

  if (!target_config.proxy.empty()) {
    worker->session->SetProxies({{"http", target_config.proxy}, {"https", target_config.proxy}});
  }

  if (should_disable_tls_verification(target_config, target)) {
    worker->session->SetSslOptions(
        cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true}));
  }

  cpr::Header target_headers;
  for (const auto& [key, value] : BuildSyncTargetHeaders(target_config, headers::poweredBy)) {
    target_headers.emplace(key, value);
  }
  worker->session->SetHeader(std::move(target_headers));

  worker->worker_thread  = std::thread(target_worker_thread, worker);
  target_workers[target] = worker;

  return worker;
}

void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync)
{
  if (target_workers_shutdown_requested.load(std::memory_order_acquire)) {
    return;
  }

  static std::once_flag emit_warning;
  const auto&           targets = Config::Get().sync_targets;

  std::call_once(emit_warning, [targets] {
    if (targets.empty()) {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to send");
    }
  });

  for (const auto& [target, target_config] : targets | std::views::filter([type](const auto& target_entry) {
                                               return SyncTargetAcceptsType(target_entry.second, type);
                                             })) {
    const auto target_identifier = STR_FORMAT("{} ({})", target, to_string(type));

    try {
      const auto worker           = get_curl_client_sync(target);
      const auto target_post_data = make_target_post_data(target_config, type, post_data, target_identifier);
      if (target_post_data.empty()) {
        continue;
      }

      {
        std::lock_guard lk(worker->queue_mtx);
        if (worker->request_queue.size() >= kTargetWorkerMaxQueuedRequests) {
          ++worker->dropped_requests;
          sync_log_warn(CURL_TYPE_UPLOAD, target_identifier,
                        STR_FORMAT("Dropping request because target queue is full (queue size: {}, dropped: {})",
                                   worker->request_queue.size(), worker->dropped_requests));
          continue;
        }

        worker->request_queue.emplace(TargetWorker::Request{
            .target_identifier = target_identifier,
            .post_data         = target_post_data,
            .is_first_sync     = is_first_sync,
        });
        sync_log_trace(CURL_TYPE_UPLOAD, target_identifier,
                       STR_FORMAT("Queued request (queue size: {})", worker->request_queue.size()));
      }
      worker->queue_cv.notify_all();

    } catch (const std::runtime_error& exception) {
      spdlog::error("Failed to send sync data to target '{}' - Runtime error: {}", target_identifier, exception.what());
    } catch (const std::exception& exception) {
      spdlog::error("Failed to send sync data to target '{}' - Exception: {}", target_identifier, exception.what());
    } catch (...) {
      spdlog::error("Failed to send sync data to target '{}' - Unknown error occurred", target_identifier);
    }
  }
}

void shutdown_workers()
{
  std::unordered_map<std::string, std::shared_ptr<TargetWorker>> workers;
  {
    std::lock_guard lk(target_workers_mtx);
    if (target_workers_shutdown_requested.exchange(true, std::memory_order_acq_rel)) {
      return;
    }

    workers.swap(target_workers);
  }

  for (auto& [target, worker] : workers) {
    if (!worker) {
      continue;
    }

    worker->stop_requested.store(true, std::memory_order_release);
    worker->queue_cv.notify_all();
  }

  for (auto& [target, worker] : workers) {
    if (worker && worker->worker_thread.joinable()) {
      worker->worker_thread.join();
    }
  }
}

static std::shared_ptr<cpr::Session> get_curl_client_scopely()
{
  static std::shared_ptr<cpr::Session> session{nullptr};
  static std::once_flag                init_flag;

  std::call_once(init_flag, [] {
    const auto header_snapshot = headers::Snapshot();
    const auto session_headers = BuildScopelySessionHeaders(header_snapshot, newUUID());
    session                    = std::make_shared<cpr::Session>();
    session->SetAcceptEncoding(cpr::AcceptEncoding{});
    session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
    session->SetConnectTimeout(cpr::ConnectTimeout{kSyncConnectTimeoutMs});
    session->SetTimeout(cpr::Timeout{kSyncRequestTimeoutMs});

    if (!Config::Get().sync_options.proxy.empty()) {
      session->SetProxies({{"https", Config::Get().sync_options.proxy}});
    }

    if (should_disable_tls_verification(Config::Get().sync_options, "scopely-api")) {
      session->SetSslOptions(
          cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true}));
    }

    session->SetUserAgent("UnityPlayer/" + header_snapshot.unityVersion + " (UnityWebRequest/1.0, libcurl/8.10.1-DEV)");
    session->SetHeader({
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"X-TRANSACTION-ID", session_headers.transaction_id},
        {"X-AUTH-SESSION-ID", session_headers.auth_session_id},
        {"X-PRIME-VERSION", session_headers.prime_version},
        {"X-Instance-ID", session_headers.instance_id},
        {"X-PRIME-SYNC", "0"},
        {"X-Unity-Version", session_headers.unity_version},
        {"X-Powered-By", headers::poweredBy},
    });
  });

  return session;
}

std::string get_scopely_data(const std::string& path, const std::string& post_data)
{
  static std::once_flag emit_warning;

  const auto header_snapshot = headers::Snapshot();
  if (header_snapshot.gameServerUrl.empty() || header_snapshot.instanceSessionId.empty()) {
    std::call_once(emit_warning, [] {
      sync_log_warn(CURL_TYPE_DOWNLOAD, "GLOBAL", "Game session headers are unavailable; cannot retrieve data");
    });

    return {};
  }

  Url url(header_snapshot.gameServerUrl);
  url.set_path(path);

  const auto        httpClient = get_curl_client_scopely();
  static std::mutex client_mutex;

  std::string response_text;

  {
    std::lock_guard lk(client_mutex);
    httpClient->SetUrl(url.c_str());

    const auto session_headers = BuildScopelySessionHeaders(header_snapshot, newUUID());
    auto&      request_headers = httpClient->GetHeader();
    request_headers.insert_or_assign("X-TRANSACTION-ID", session_headers.transaction_id);
    request_headers.insert_or_assign("X-AUTH-SESSION-ID", session_headers.auth_session_id);
    request_headers.insert_or_assign("X-PRIME-VERSION", session_headers.prime_version);
    request_headers.insert_or_assign("X-Instance-ID", session_headers.instance_id);
    request_headers.insert_or_assign("X-Unity-Version", session_headers.unity_version);

    httpClient->SetBody(post_data);
    const auto response = httpClient->Post();

    if (response.status_code == 0) {
      sync_log_error(CURL_TYPE_DOWNLOAD, path, "Failed to send request: " + response.error.message);
      return {};
    }

    if (response.status_code >= 400) {
      sync_log_error(CURL_TYPE_DOWNLOAD, path, "Failed to communicate with server: " + response.status_line);
      return {};
    }

    const auto  response_headers = response.header;
    std::string type;

    try {
      type = response_headers.at("Content-Type");
    } catch (const std::out_of_range&) {
      type = "unknown";
    }

    sync_log_debug(CURL_TYPE_DOWNLOAD, path,
                   STR_FORMAT("Response: {} ({}), {:.1f}s elapsed,", response.status_line, type, response.elapsed));
    response_text = response.text;
  }

  return response_text;
}
} // namespace http
