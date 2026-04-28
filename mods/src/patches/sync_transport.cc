/**
 * @file sync_transport.cc
 * @brief HTTP transport and Scopely API client for sync pipelines.
 */
#include "patches/sync_transport.h"

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
#include <condition_variable>
#include <format>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#ifndef STR_FORMAT
#if __cpp_lib_format
#define STR_FORMAT std::format
#else
#define STR_FORMAT fmt::format
#endif
#endif

#if _WIN32
struct WinRtApartmentGuard {
  WinRtApartmentGuard() { winrt::init_apartment(); }
  ~WinRtApartmentGuard() { winrt::uninit_apartment(); }
};
#endif

namespace http
{
namespace headers
{
  std::string gameServerUrl;
  std::string instanceSessionId;
  int32_t     instanceId = 0;
  std::string unityVersion{"6000.0.52f1"};
  std::string primeVersion{"1.000.45324"};
  const char  poweredBy[] = "stfc community patch/" VER_FILE_VERSION_STR;
} // namespace headers

[[nodiscard]] static std::string newUUID()
{
#ifdef _WIN32
  UUID uuid;
  UuidCreate(&uuid);

  unsigned char* str;
  UuidToStringA(&uuid, &str);

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
  explicit Url(const std::string& url) : url_(url)
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

  Url(const Url&) = delete;
  Url& operator=(const Url&) = delete;

  Url(Url&& other) noexcept : handle_(other.handle_), url_(std::move(other.url_))
  {
    other.handle_ = nullptr;
  }

  Url& operator=(Url&& other) noexcept
  {
    if (this != &other) {
      if (handle_) {
        curl_url_cleanup(handle_);
      }

      handle_ = other.handle_;
      url_ = std::move(other.url_);
      other.handle_ = nullptr;
    }

    return *this;
  }

  void set_path(const std::string& path)
  {
    if (!handle_) {
      return;
    }

    if (CURLUcode result_code = curl_url_set(handle_, CURLUPART_PATH, path.c_str(), 0);
        result_code == CURLUE_OK) {
      char* url = nullptr;
      if (result_code = curl_url_get(handle_, CURLUPART_URL, &url, CURLU_PUNYCODE);
          result_code == CURLUE_OK) {
        url_ = url;
      }

      if (url != nullptr) {
        curl_free(url);
      }
    }
  }

  [[nodiscard]] const char* c_str() const
  {
    return url_.c_str();
  }

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

static const std::string CURL_TYPE_UPLOAD   = "UPLOAD";
static const std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";

struct TargetWorker {
  TargetWorker() = default;
  TargetWorker(const TargetWorker&) = delete;
  TargetWorker& operator=(const TargetWorker&) = delete;

  using request_t = std::tuple<std::string, std::string, bool>;

  std::shared_ptr<cpr::Session> session;
  std::thread                   worker_thread;
  std::atomic_bool              stop_requested{false};
  std::queue<request_t>         request_queue;
  std::mutex                    queue_mtx;
  std::condition_variable       queue_cv;
};

static std::unordered_map<std::string, std::shared_ptr<TargetWorker>> target_workers;
static std::mutex target_workers_mtx;

static void target_worker_thread(std::shared_ptr<TargetWorker> worker)
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  while (!worker->stop_requested.load(std::memory_order_acquire)) {
    std::string identifier;
    std::string post_data;
    bool is_first_sync = false;

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
        identifier = std::move(std::get<0>(item));
        post_data = std::move(std::get<1>(item));
        is_first_sync = std::get<2>(item);
      }
    }

    if (post_data.empty()) {
      continue;
    }

    try {
      const auto httpClient = worker->session;
      auto& request_headers = httpClient->GetHeader();

      if (is_first_sync) {
        request_headers.insert_or_assign("X-PRIME-SYNC", "2");
        sync_log_trace(CURL_TYPE_UPLOAD, identifier, "Adding X-Prime-Sync header for initial sync");
      } else {
        request_headers.erase("X-PRIME-SYNC");
      }

      httpClient->SetBody(cpr::Body{post_data});

      sync_log_debug(CURL_TYPE_UPLOAD, identifier, "Sending data to " + httpClient->GetFullRequestUrl());

      const auto response = httpClient->Post();

      if (response.status_code == 0) {
        sync_log_error(CURL_TYPE_UPLOAD, identifier, "Failed to send request: " + response.error.message);
      } else if (response.status_code >= 400) {
        sync_log_error(CURL_TYPE_UPLOAD, identifier,
                       STR_FORMAT("Failed to communicate with server: {} (after {:.1f}s)", response.status_line,
                                  response.elapsed));
      } else {
        sync_log_debug(CURL_TYPE_UPLOAD, identifier,
                       STR_FORMAT("Response: {} ({:.1f}s elapsed)", response.status_line, response.elapsed));
      }
    } catch (const std::runtime_error& exception) {
      ErrorMsg::SyncRuntime(identifier.c_str(), exception);
    } catch (const std::exception& exception) {
      ErrorMsg::SyncException(identifier.c_str(), exception);
#if _WIN32
    } catch (winrt::hresult_error const& exception) {
      ErrorMsg::SyncWinRT(identifier.c_str(), exception);
#endif
    } catch (...) {
      ErrorMsg::SyncMsg(identifier.c_str(), "Unknown error occurred");
    }
  }
}

static std::shared_ptr<TargetWorker> get_curl_client_sync(const std::string& target)
{
  std::lock_guard lk(target_workers_mtx);

  if (const auto found = target_workers.find(target); found != target_workers.end()) {
    return found->second;
  }

  auto worker = std::make_shared<TargetWorker>();
  worker->session = std::make_shared<cpr::Session>();
  const auto& target_config = Config::Get().sync_targets[target];

  worker->session->SetUrl(target_config.url);
  worker->session->SetUserAgent("stfc community patch " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
  worker->session->SetAcceptEncoding(cpr::AcceptEncoding{});
  worker->session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
  worker->session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});

#ifndef _MODDBG
  worker->session->SetConnectTimeout(cpr::ConnectTimeout{3'000});
  worker->session->SetTimeout(cpr::Timeout{10'000});
#endif

  if (!target_config.proxy.empty()) {
    worker->session->SetProxies({{"http", target_config.proxy}, {"https", target_config.proxy}});

    if (!target_config.verify_ssl) {
      worker->session->SetSslOptions(
        cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true})
      );
    }
  }

  worker->session->SetHeader({
    {"Content-Type", "application/json"},
    {"X-Powered-By", headers::poweredBy},
    {"stfc-sync-token", target_config.token},
  });

  worker->worker_thread = std::thread(target_worker_thread, worker);
  target_workers[target] = worker;

  return worker;
}

void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync)
{
  static std::once_flag emit_warning;
  const auto& targets = Config::Get().sync_targets;

  std::call_once(emit_warning, [targets] {
    if (targets.empty()) {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to send");
    }
  });

  for (const auto& target : targets
       | std::views::filter([type](const auto& target_entry) { return target_entry.second.enabled(type); })
       | std::views::keys) {
    const auto target_identifier = STR_FORMAT("{} ({})", target, to_string(type));

    try {
      const auto worker = get_curl_client_sync(target);

      {
        std::lock_guard lk(worker->queue_mtx);
        worker->request_queue.emplace(target_identifier, post_data, is_first_sync);
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

static std::shared_ptr<cpr::Session> get_curl_client_scopely()
{
  static std::shared_ptr<cpr::Session> session{nullptr};
  static std::once_flag init_flag;

  std::call_once(init_flag, [] {
    session = std::make_shared<cpr::Session>();
    session->SetAcceptEncoding(cpr::AcceptEncoding{});
    session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});

    if (!Config::Get().sync_options.proxy.empty()) {
      session->SetProxies({{"https", Config::Get().sync_options.proxy}});

      if (!Config::Get().sync_options.verify_ssl) {
        session->SetSslOptions(
          cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true})
        );
      }
    }

    session->SetUserAgent("UnityPlayer/" + headers::unityVersion + " (UnityWebRequest/1.0, libcurl/8.10.1-DEV)");
    session->SetHeader({
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"X-TRANSACTION-ID", newUUID()},
        {"X-AUTH-SESSION-ID", headers::instanceSessionId},
        {"X-PRIME-VERSION", headers::primeVersion},
        {"X-Instance-ID", STR_FORMAT("{:03}", headers::instanceId)},
        {"X-PRIME-SYNC", "0"},
        {"X-Unity-Version", headers::unityVersion},
        {"X-Powered-By", headers::poweredBy},
    });
  });

  return session;
}

std::string get_scopely_data(const std::string& path, const std::string& post_data)
{
  static std::once_flag emit_warning;

  if (headers::gameServerUrl.empty() || headers::instanceSessionId.empty()) {
    std::call_once(emit_warning, [] {
      sync_log_warn(CURL_TYPE_DOWNLOAD, "GLOBAL", "Game session headers are unavailable; cannot retrieve data");
    });

    return {};
  }

  Url url(headers::gameServerUrl);
  url.set_path(path);

  const auto        httpClient = get_curl_client_scopely();
  static std::mutex client_mutex;

  std::string response_text;

  {
    std::lock_guard lk(client_mutex);
    httpClient->SetUrl(url.c_str());

    auto& request_headers = httpClient->GetHeader();
    request_headers.insert_or_assign("X-TRANSACTION-ID", newUUID());
    request_headers.insert_or_assign("X-AUTH-SESSION-ID", headers::instanceSessionId);
    request_headers.insert_or_assign("X-Instance-ID", STR_FORMAT("{:03}", headers::instanceId));

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