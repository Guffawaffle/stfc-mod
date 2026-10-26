#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "str_utils.h"
#include "sync_bounded_queue.h"
#include "sync_diagnostics.h"
#include "sync_resolver_cache.h"
#include "sync_session_headers.h"

#include <Digit.PrimeServer.Models.pb.h>
#include <il2cpp-api-types.h>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/runtime.h>
#include <il2cpp/method_contract.h>
#include <prime/EntityGroup.h>
#include <prime/RealtimeDataPayload.h>
#include <prime/ServiceResponse.h>
#include <prime/UserProfileManager.h>
#include "patches/spud_install.h"

#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#if _WIN32
#include <rpc.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.Headers.h>
#else
#include <uuid/uuid.h>
#endif

#include <EASTL/algorithm.h>
#include <EASTL/bonus/ring_buffer.h>
#include <cpr/cpr.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef STR_FORMAT
#if __cpp_lib_format
#define STR_FORMAT std::format
#else
#define STR_FORMAT fmt::format
#endif
#endif

#ifndef _WIN32
#include <time.h>
#endif

#if _WIN32
// Introduce RAII guard for COM apartments
struct WinRtApartmentGuard {
  WinRtApartmentGuard() { winrt::init_apartment(); }
  ~WinRtApartmentGuard() { winrt::uninit_apartment(); }
};
#endif

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T> struct adl_serializer<google::protobuf::RepeatedField<T>> {
  static void to_json(json& j, const google::protobuf::RepeatedField<T>& proto)
  {
    j = json::array();

    for (const auto& v : proto) {
      j.push_back(v);
    }
  }

  static void from_json(const json& j, google::protobuf::RepeatedField<T>& proto)
  {
    if (j.is_array()) {
      for (const auto& v : j) {
        proto.Add(v.get<T>());
      }
    }
  }
};

template <> struct adl_serializer<SyncConfig::Type> {
  static void to_json(json& j, const SyncConfig::Type t)
  {
    for (const auto& opt : SyncOptions) {
      if (opt.type == t) {
        j = opt.type_str;
        return;
      }
    }

    j = nullptr;
  }

  static void from_json(const json& j, SyncConfig::Type& t)
  {
    if (j.is_string()) {
      const auto& s = j.get_ref<const std::string&>();

      for (const auto& opt : SyncOptions) {
        if (opt.type_str == s) {
          t = opt.type;
          return;
        }
      }
    }
  }
};
NLOHMANN_JSON_NAMESPACE_END

namespace http
{

namespace headers
{
  // The owned workers are process-lifetime; their metadata must outlive static
  // teardown too. These bounded owners perform no shutdown-time runtime calls.
  static auto&       state        = *new sync_session::Headers;
  static const auto& unityVersion = *new std::string{"6000.0.59f2-deadlock-fix-test"};
  static const auto& poweredBy    = *new std::string{"stfc community mod/" VER_FILE_VERSION_STR};

  static uint64_t CurrentSessionGeneration() noexcept
  { return state.Generation(); }
  static bool IsCurrentSessionGeneration(uint64_t generation) noexcept
  { return state.IsCurrent(generation); }
} // namespace headers

namespace logging
{
  static constexpr std::string CURL_TYPE_UPLOAD   = "UPLOAD";
  static constexpr std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";

  static void error(const std::string& type, const std::string& target, const std::string& text)
  {
    if (Config::Get().sync_logging) {
      spdlog::error("SYNC-{} - {}: {}", type, target, text);
    }
  }

  static void warn(const std::string& type, const std::string& target, const std::string& text)
  {
    if (Config::Get().sync_logging) {
      spdlog::warn("SYNC-{} - {}: {}", type, target, text);
    }
  }

  static void info(const std::string& type, const std::string& target, const std::string& text)
  {
    if (Config::Get().sync_logging) {
      spdlog::info("SYNC-{} - {}: {}", type, target, text);
    }
  }

  static void debug(const std::string& type, const std::string& target, const std::string& text)
  {
    if (Config::Get().sync_logging && Config::Get().sync_debug) {
      spdlog::debug("SYNC-{} - {}: {}", type, target, text);
    }
  }

  static void trace(const std::string& type, const std::string& target, const std::string& text)
  {
    if (Config::Get().sync_logging && Config::Get().sync_debug) {
      spdlog::trace("SYNC-{} - {}: {}", type, target, text);
    }
  }
}

[[nodiscard]] static std::string newUUID()
{
#ifdef _WIN32
  UUID uuid;
  UuidCreate(&uuid);

  unsigned char* str;
  UuidToStringA(&uuid, &str);

  std::string s(reinterpret_cast<char*>(str));

  RpcStringFreeA(&str);
#else
  uuid_t uuid;
  uuid_generate_random(uuid);
  char s[37];
  uuid_unparse(uuid, s);
#endif
  return s;
}

// Simple URL manipulation class to replace boost::url
class Url
{
public:
  explicit Url(std::string url) : m_url(std::move(url))
  {
    m_handle = curl_url();
    if (m_handle != nullptr) {
      curl_url_set(m_handle, CURLUPART_URL, m_url.data(), 0);
    }
  }

  ~Url()
  {
    if (m_handle != nullptr) {
      curl_url_cleanup(m_handle);
    }
  }

  // Delete copy operations to prevent double-free
  Url(const Url&) = delete;
  Url& operator=(const Url&) = delete;

  Url(Url&& other) noexcept : m_handle(other.m_handle), m_url(std::move(other.m_url))
  {
    other.m_handle = nullptr;
  }

  Url& operator=(Url&& other) noexcept
  {
    if (this != &other) {
      if (m_handle != nullptr) {
        curl_url_cleanup(m_handle);
      }

      m_handle = other.m_handle;
      m_url = std::move(other.m_url);
      other.m_handle = nullptr;
    }

    return *this;
  }

  void set_path(const std::string& path)
  {
    if (m_handle == nullptr) {
      return;
    }

    if (CURLUcode rc = curl_url_set(m_handle, CURLUPART_PATH, path.c_str(), 0); rc == CURLUE_OK) {
      char *url = nullptr;
      if (rc = curl_url_get(m_handle, CURLUPART_URL , &url, CURLU_PUNYCODE); rc == CURLUE_OK) {
        m_url = url;
      }

      if (url != nullptr) {
        curl_free(url);
      }
    }
  }

  [[nodiscard]] const char* c_str() const
  {
    return m_url.c_str();
  }

private:
  CURLU *m_handle;
  std::string m_url;
};

// Per-target worker thread infrastructure
struct TargetWorker {
  TargetWorker() = default;
  TargetWorker(const TargetWorker&) = delete;
  TargetWorker& operator=(const TargetWorker&) = delete;

  struct request_t {
    std::string                        identifier;
    std::shared_ptr<const std::string> post_data;
    bool                               is_first_sync;
    uint64_t                           generation;
  };

  std::shared_ptr<cpr::Session> session;
  std::thread                   worker_thread;
  sync_queue::BoundedQueue<request_t> requests{{256, 32 * 1024 * 1024, 16 * 1024 * 1024}};
  std::mutex                          admission_mtx;
  bool                                accepting        = false;
  uint64_t                            diagnostic_drops = 0;
};

// Hook workers have process lifetime. No join or live DLL teardown is supported
// under the loader lock; their queues and sessions must live as long as they do.
static auto&            target_workers         = *new std::unordered_map<std::string, std::shared_ptr<TargetWorker>>;
static auto&            target_workers_mtx     = *new std::mutex;
static bool             target_registry_failed = false; // Protected by target_workers_mtx.
static std::atomic_bool http_uploads_accepting{true};

static void target_worker_thread(std::shared_ptr<TargetWorker> worker)
{
  try {
#if _WIN32
    WinRtApartmentGuard apartmentGuard;
#endif

    while (auto item = worker->requests.WaitPop()) {
      const auto& [identifier, post_data, is_first_sync, generation] = item->value;

      if (post_data->empty() || generation != headers::CurrentSessionGeneration())
        continue;

      // Process the request
      try {
        const auto httpClient = worker->session;
        auto&      headers    = httpClient->GetHeader();

        if (is_first_sync) {
          headers.insert_or_assign("X-PRIME-SYNC", "2");
          logging::trace(logging::CURL_TYPE_UPLOAD, identifier, "Adding X-Prime-Sync header for initial sync");
        } else {
          headers.erase("X-PRIME-SYNC");
        }

        // CPR retains its request body after Post. Clear it before releasing the
        // queue's in-flight charge, including when the transport throws.
        struct ClearBody {
          cpr::Session& session;
          ~ClearBody()
          {
            try {
              session.SetBody(cpr::Body{});
            } catch (...) {
            }
          }
        } clear_body{*httpClient};
        httpClient->SetBody(cpr::Body{*post_data});

        logging::debug(logging::CURL_TYPE_UPLOAD, identifier, "Sending data to " + httpClient->GetFullRequestUrl());

        // Synchronously wait for response
        const auto response = httpClient->Post();

        if (response.status_code == 0) {
          logging::error(logging::CURL_TYPE_UPLOAD, identifier, "Failed to send request: " + response.error.message);
        } else if (response.status_code >= 400) {
          if (response.status_code == 501) {
            logging::error(logging::CURL_TYPE_UPLOAD, identifier,
                           STR_FORMAT("Sync target does not support this operation ({}, after {:.0f} ms). Consider "
                                      "turning off this upload category for the target.",
                                      response.status_line, response.elapsed * 1'000));
          } else {
            logging::error(logging::CURL_TYPE_UPLOAD, identifier,
                           STR_FORMAT("Failed to communicate with server: {} (after {:.0f} ms)", response.status_line,
                                      response.elapsed * 1'000));
          }
        } else {
          logging::debug(
              logging::CURL_TYPE_UPLOAD, identifier,
              STR_FORMAT("Response: {} ({:.0f} ms elapsed)", response.status_line, response.elapsed * 1'000));
        }
      } catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime(identifier.c_str(), e);
      } catch (const std::exception& e) {
        ErrorMsg::SyncException(identifier.c_str(), e);
#if _WIN32
    } catch (winrt::hresult_error const& ex) {
      ErrorMsg::SyncWinRT(identifier.c_str(), ex);
#endif
    } catch (...) {
      ErrorMsg::SyncMsg(identifier.c_str(), "Unknown error occurred");
    }
    }
  } catch (const std::exception& e) {
    std::scoped_lock lock(worker->admission_mtx);
    worker->accepting = false;
    worker->requests.Close();
    spdlog::error("Sync target worker stopped: {}. Restart the game to resume this target.", e.what());
  } catch (...) {
    std::scoped_lock lock(worker->admission_mtx);
    worker->accepting = false;
    worker->requests.Close();
    spdlog::error("Sync target worker stopped. Restart the game to resume this target.");
  }
}

static std::shared_ptr<TargetWorker> get_curl_client_sync(const std::string& target)
{
  std::scoped_lock lk(target_workers_mtx);
  if (target_registry_failed)
    return nullptr;

  // Check if a worker already exists
  if (const auto it = target_workers.find(target); it != target_workers.end()) {
    return it->second;
  }

  std::shared_ptr<TargetWorker> worker;
  // Publish ownership before launching. If initialization fails, retain a
  // disabled tombstone rather than spawning a replacement for each payload.
  try {
    worker = std::make_shared<TargetWorker>();
    target_workers.emplace(target, worker);
  } catch (...) {
    target_registry_failed = true;
    spdlog::error("Sync target registry allocation failed; uploads are disabled until the game restarts.");
    throw;
  }

  try {
    // Initialize session
    worker->session           = std::make_shared<cpr::Session>();
    const auto& target_config = Config::Get().sync_targets[target];

    worker->session->SetUrl(target_config.url);
    worker->session->SetUserAgent("stfc community mod " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
    worker->session->SetAcceptEncoding(cpr::AcceptEncoding{});
    worker->session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
    worker->session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});

    // Debug builds must not pin an in-flight body forever either.
    worker->session->SetConnectTimeout(cpr::ConnectTimeout{3'000});
    worker->session->SetTimeout(cpr::Timeout{10'000});

    spdlog::debug("sync target '{}': proxy='{}', verify_ssl={}", target, target_config.proxy, target_config.verify_ssl);

    if (!target_config.proxy.empty()) {
      worker->session->SetProxies({{"http", target_config.proxy}, {"https", target_config.proxy}});
    }

    if (!target_config.verify_ssl) {
      worker->session->SetSslOptions(
          cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true}));
    }

    worker->session->SetHeader({
        {"Content-Type", "application/json"},
        {"X-Powered-By", headers::poweredBy},
        {"stfc-sync-token", target_config.token},
    });

    // Start the worker thread
    worker->accepting     = true;
    worker->worker_thread = std::thread(target_worker_thread, worker);
  } catch (...) {
    worker->accepting = false;
    worker->requests.Close();
    spdlog::error("Sync target '{}' could not start; uploads to this target are disabled until the game restarts.",
                  target);
  }

  return worker;
}

static void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync, uint64_t generation)
{
  if (!http_uploads_accepting.load(std::memory_order_acquire) || generation != headers::CurrentSessionGeneration())
    return;
  static std::once_flag emit_warning;
  const auto& targets = Config::Get().sync_targets;

  std::call_once(emit_warning, [&targets] {
    if (targets.empty()) {
      logging::warn(logging::CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to send");
    }
  });

  std::shared_ptr<const std::string> owned_body;
  for (const auto& target : targets
       | std::views::filter([type](const auto& t) { return t.second.enabled(type); })
       | std::views::keys) {

    try {
      const auto target_identifier = STR_FORMAT("{} ({})", target, to_string(type));
      const auto worker = get_curl_client_sync(target);
      if (!worker)
        continue;

      sync_queue::Admission admission          = sync_queue::Admission::accepted;
      bool                  diagnostic_dropped = false;
      {
        std::scoped_lock lk(worker->admission_mtx);
        if (!worker->accepting)
          continue;
        const auto charge = post_data.size() + target_identifier.size();
        if (type == SyncConfig::Type::Diagnostics) {
          const auto stats = worker->requests.Stats();
          // Observations use only a small portion of the ordinary FIFO budget.
          // They must never evict a first-sync snapshot or ordinary delta.
          if (stats.count >= 64 || charge > 4 * 1024 * 1024 || stats.bytes > 4 * 1024 * 1024 - charge) {
            diagnostic_dropped = (++worker->diagnostic_drops % 64 == 1);
            admission          = sync_queue::Admission::full;
          }
        }
        if (admission == sync_queue::Admission::accepted) {
          admission = worker->requests.TryEmplace(charge, [&] {
            if (!owned_body)
              owned_body = std::make_shared<const std::string>(post_data);
            return TargetWorker::request_t{target_identifier, owned_body, is_first_sync, generation};
          });
          if (admission != sync_queue::Admission::accepted) {
            if (type == SyncConfig::Type::Diagnostics)
              diagnostic_dropped = (++worker->diagnostic_drops % 64 == 1);
            else
              worker->accepting = false;
          }
        }
      }
      if (admission != sync_queue::Admission::accepted) {
        if (type != SyncConfig::Type::Diagnostics) {
          const auto stats = worker->requests.Stats();
          spdlog::error(
              "Sync target '{}' backlog limit reached ({} items, {} bytes, high-water {} / {}, oldest {} ms). "
              "Queued requests will drain; further uploads to this target are disabled. "
              "Restart the game to begin a fresh sync; other targets continue.",
              target, stats.count, stats.bytes, stats.highWaterCount, stats.highWaterBytes, stats.oldestAgeMs);
        } else if (diagnostic_dropped) {
          spdlog::warn("Diagnostic sync queue budget exhausted for '{}'; dropping observations", target);
        }
      }

    } catch (const std::runtime_error& e) {
      if (type == SyncConfig::Type::Diagnostics) {
        spdlog::warn("Diagnostic sync admission failed for '{}'; observation dropped: {}", target, e.what());
        continue;
      }
      http_uploads_accepting.store(false, std::memory_order_release);
      spdlog::error("Sync upload admission failed for '{}': {}. Uploads are disabled until the game restarts.", target,
                    e.what());
      return;
    } catch (const std::exception& e) {
      if (type == SyncConfig::Type::Diagnostics) {
        spdlog::warn("Diagnostic sync admission failed for '{}'; observation dropped: {}", target, e.what());
        continue;
      }
      http_uploads_accepting.store(false, std::memory_order_release);
      spdlog::error("Sync upload admission failed for '{}': {}. Uploads are disabled until the game restarts.", target,
                    e.what());
      return;
    } catch (...) {
      if (type == SyncConfig::Type::Diagnostics) {
        spdlog::warn("Diagnostic sync admission failed for '{}'; observation dropped", target);
        continue;
      }
      http_uploads_accepting.store(false, std::memory_order_release);
      spdlog::error("Sync upload admission failed for '{}'. Uploads are disabled until the game restarts.", target);
      return;
    }
  }
}

static std::shared_ptr<cpr::Session> get_curl_client_scopely()
{
  // Match process-lifetime workers rather than destroying their HTTP session
  // or synchronization objects while a request can still be draining.
  static auto& session   = *new std::shared_ptr<cpr::Session>;
  static auto& init_flag = *new std::once_flag;

  // thread-safe initialization
  std::call_once(init_flag, [] {
    session = std::make_shared<cpr::Session>();
    session->SetAcceptEncoding(cpr::AcceptEncoding{});
    session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
    session->SetConnectTimeout(cpr::ConnectTimeout{3'000});
    session->SetTimeout(cpr::Timeout{10'000});

    spdlog::debug("scopely session: proxy='{}', verify_ssl={}", Config::Get().sync_options.proxy, Config::Get().sync_options.verify_ssl);

    if (!Config::Get().sync_options.proxy.empty()) {
      session->SetProxies({{"https", Config::Get().sync_options.proxy}});
    }

    if (!Config::Get().sync_options.verify_ssl) {
      session->SetSslOptions(
        cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true})
      );
    }

    session->SetUserAgent("UnityPlayer/" + headers::unityVersion + " (UnityWebRequest/1.0, libcurl/8.10.1-DEV)");
  });

  return session;
}

static std::string get_scopely_data(const std::string& path, const std::string& post_data,
                                    uint64_t expectedGeneration = 0)
{
  static std::once_flag emit_warning;

  if (Config::Get().sync_targets.empty()) {
    std::call_once(emit_warning, [] {
      logging::warn(logging::CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to retrieve data");
    });

    return {};
  }

  const auto        httpClient = get_curl_client_scopely();
  static auto&      client_mutex = *new std::mutex;

  std::string response_text;

  {
    std::scoped_lock lk(client_mutex);
    // Snapshot only after waiting for the HTTP session. Never hold the metadata
    // mutex through URL setup or network I/O, and never mix fields from logins.
    const auto snapshot = headers::state.Read();
    if ((expectedGeneration && snapshot.generation != expectedGeneration) || snapshot.gameServerUrl.empty()
        || snapshot.instanceSessionId.empty())
      return {};
    Url url(snapshot.gameServerUrl);
    url.set_path(path);
    httpClient->SetUrl(url.c_str());
    httpClient->SetHeader({
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"X-TRANSACTION-ID", newUUID()},
        {"X-AUTH-SESSION-ID", snapshot.instanceSessionId},
        {"X-PRIME-VERSION", snapshot.primeVersion},
        {"X-Instance-ID", STR_FORMAT("{:03}", snapshot.instanceId)},
        {"X-PRIME-SYNC", "0"},
        {"X-Unity-Version", headers::unityVersion},
        {"X-Powered-By", headers::poweredBy},
    });

    httpClient->SetBody(post_data);
    if (!headers::IsCurrentSessionGeneration(snapshot.generation))
      return {};
    const auto response = httpClient->Post();
    if (!headers::IsCurrentSessionGeneration(snapshot.generation))
      return {}; // A completed old-login response must not populate current state.

    if (response.status_code == 0) {
      logging::error(logging::CURL_TYPE_DOWNLOAD, path, "Failed to send request: " + response.error.message);
      return {};
    }

    if (response.status_code >= 400) {
      logging::error(logging::CURL_TYPE_DOWNLOAD, path, "Failed to communicate with server: " + response.status_line);
      return {};
    }

    const auto  response_headers = response.header;
    std::string type;

    try {
      type = response_headers.at("Content-Type");
    } catch (const std::out_of_range&) {
      type = "unknown";
    }

    logging::debug(logging::CURL_TYPE_DOWNLOAD, path,
                   STR_FORMAT("Response: {} ({}), {:.0f} ms elapsed,", response.status_line, type, response.elapsed * 1'000));
    response_text = response.text;
  }

  return response_text;
}

} // namespace http

namespace trackers
{

namespace types
{
  struct RankLevelState {
    explicit RankLevelState(const int32_t r = -1, const int32_t l = -1)
        : rank(r)
        , level(l)
    {
    }

    bool operator==(const RankLevelState& other) const
    { return this->rank == other.rank && this->level == other.level; }

  private:
    int64_t rank  = -1;
    int64_t level = -1;
  };

  struct RankLevelShardsState {
    explicit RankLevelShardsState(const int32_t r = -1, const int32_t l = -1, const int32_t s = -1)
        : rank(r)
        , level(l)
        , shards(s)
    {
    }

    bool operator==(const RankLevelShardsState& other) const
    { return this->rank == other.rank && this->level == other.level && this->shards == other.shards; }

  private:
    int32_t rank   = -1;
    int32_t level  = -1;
    int32_t shards = -1;
  };

  struct ShipState {
    explicit ShipState(const int32_t t = -1, const int32_t l = -1, const double_t lp = -1.0,
                       const std::vector<int64_t>& c = {})
        : tier(t)
        , level(l)
        , level_percentage(lp)
        , components(c)
    {
    }

    bool operator==(const ShipState& other) const
    {
      return this->tier == other.tier && this->level == other.level
             && std::fabs(this->level_percentage - other.level_percentage) < 0.01
             && this->components == other.components;
    }

  private:
    int32_t              tier             = -1;
    int32_t              level            = -1;
    double_t             level_percentage = -1.0;
    std::vector<int64_t> components;
  };

  struct PlanetaryBaseState {
    explicit PlanetaryBaseState(const int32_t l = -1, const int32_t s = -1, const int64_t p = -1)
        : level(l)
        , status(s)
        , position(p)
    {
    }

    bool operator==(const PlanetaryBaseState& other) const
    { return this->level == other.level && this->status == other.status && this->position == other.position; }

  private:
    int32_t level    = -1;
    int32_t status   = -1;
    int64_t position = -1;
  };

  struct pairhash {
    template <typename T, typename U> std::size_t operator()(const std::pair<T, U>& x) const
    { return std::hash<T>()(x.first) ^ std::hash<U>()(x.second); }
  };

  struct CachedPlayerData {
    std::string                           name;
    int64_t                               alliance{-1};
    std::chrono::steady_clock::time_point expires_at;
  };

  struct CachedAllianceData {
    std::string                           name;
    std::string                           tag;
    std::chrono::steady_clock::time_point expires_at;
  };
} // namespace types

namespace names
{
  using namespace types;

  // Match the worker lifetime; no cache/mutex destructor races a draining worker.
  static auto& player_data_cache     = *new sync_resolver::Cache<std::string, CachedPlayerData>;
  static auto& player_data_cache_mtx = *new std::mutex;

  static auto& alliance_data_cache     = *new sync_resolver::Cache<int64_t, CachedAllianceData>;
  static auto& alliance_data_cache_mtx = *new std::mutex;

  static void cache_player_names(std::unique_ptr<std::string>&& bytes)
  {
    if (auto response = Digit::PrimeServer::Models::UserProfilesResponse(); response.ParseFromString(*bytes)) {

      const auto       now        = std::chrono::steady_clock::now();
      const auto       expires_at = now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);
      std::scoped_lock lk(player_data_cache_mtx);
      player_data_cache.Prune(now);
      for (const auto& profile : response.userprofiles()) {
        if (!profile.userid().empty() && sync_resolver::BoundedField(profile.userid())
            && sync_resolver::BoundedField(profile.name())) {
          player_data_cache.Put(
              profile.userid(),
              CachedPlayerData{.name = profile.name(), .alliance = profile.allianceid(), .expires_at = expires_at},
              now);
        }
      }
    } else {
      spdlog::error("Failed to parse user profile");
    }
  }

  static void cache_alliance_names(std::unique_ptr<std::string>&& bytes)
  {
    if (auto response = Digit::PrimeServer::Models::GetAllianceProfilesResponse(); response.ParseFromString(*bytes)) {

      const auto       now        = std::chrono::steady_clock::now();
      const auto       expires_at = now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);
      std::scoped_lock lk(alliance_data_cache_mtx);
      alliance_data_cache.Prune(now);
      for (const auto& alliance : response.allianceprofiles()) {
        if (alliance.id() > 0 && sync_resolver::BoundedField(alliance.name())
            && sync_resolver::BoundedField(alliance.tag())) {
          alliance_data_cache.Put(
              alliance.id(),
              CachedAllianceData{.name = alliance.name(), .tag = alliance.tag(), .expires_at = expires_at}, now);
        }
      }

    } else {
      spdlog::error("Failed to parse alliance profile");
    }
  }

  static inline void collect_user_ids_from_fleet(const nlohmann::json&            fleet_data,
                                                 std::unordered_set<std::string>& user_ids)
  {
    if (!fleet_data.contains("ref_ids") || fleet_data["ref_ids"].is_null()) {
      for (const auto& fleet : fleet_data["deployed_fleets"]) {
        const auto& player_id = fleet["uid"].get<std::string>();
        user_ids.insert(player_id);
      }
    }
  }

  // Cracked stations have no fleet, so a base raid's defender isn't in deployed_fleets -- take the
  // participant ids from the journal instead.
  static inline void collect_journal_participants(const nlohmann::json&            journal,
                                                  std::unordered_set<std::string>& user_ids)
  {
    for (const char* key : {"initiator_id", "target_id"}) {
      if (!journal.contains(key) || journal[key].is_null()) {
        continue;
      }
      const auto& id = journal[key].get<std::string>();
      // NPC ids contain '_' (mar_61, npc_<id>_1); player ids don't -- skip, nothing to resolve.
      if (!id.empty() && id.find('_') == std::string::npos) {
        user_ids.insert(id);
      }
    }
  }

  static inline void collect_alliance_ids(const nlohmann::json& names, std::unordered_set<int64_t>& alliance_ids)
  {
    for (const auto& [player_id, entry] : names.items()) {
      try {
        const auto alliance_id = entry["alliance_id"].get<int64_t>();
        alliance_ids.insert(alliance_id);
      } catch (const nlohmann::json::exception&) {
      }
    }
  }

  static void resolve_player_names(const std::unordered_set<std::string>& user_ids, nlohmann::json& out_names,
                                   nlohmann::json&                                          out_request_ids,
                                   const std::chrono::time_point<std::chrono::steady_clock> now)
  {
    std::scoped_lock lk(player_data_cache_mtx);
    player_data_cache.Prune(now);

    for (const auto& user_id : user_ids) {
      if (const auto* cached = player_data_cache.Find(user_id, now)) {
        out_names[user_id] = {{"name", cached->name},
                              {"alliance_id", cached->alliance},
                              {"alliance_name", nullptr},
                              {"alliance_tag", nullptr}};
      } else {
        // cache miss: queue for fetch
        out_request_ids.push_back(user_id);
      }
    }
  }

  static void resolve_alliance_names(const std::unordered_set<int64_t>& alliance_ids, nlohmann::json& out_names,
                                     nlohmann::json&                                          out_request_ids,
                                     const std::chrono::time_point<std::chrono::steady_clock> now)
  {
    std::scoped_lock lk(alliance_data_cache_mtx);
    alliance_data_cache.Prune(now);

    for (const auto& alliance_id : alliance_ids) {
      if (const auto* cached = alliance_data_cache.Find(alliance_id, now)) {
        for (const auto& [player_id, entry] : out_names.items()) {
          try {
            if (entry["alliance_id"].get<int64_t>() == alliance_id) {
              entry["alliance_name"] = cached->name;
              entry["alliance_tag"]  = cached->tag;
              entry.erase("alliance_id");
            }
          } catch (const nlohmann::json::exception&) {
          }
        }
      } else {
        out_request_ids.push_back(alliance_id);
      }
    }
  }
} // namespace names

static std::unordered_map<int64_t, int64_t> module_states;
static std::mutex                           module_states_mtx;

static std::unordered_map<int64_t, int64_t> resource_states;
static std::mutex                           resource_states_mtx;

static std::unordered_map<int64_t, int64_t> resource_states_alliance;
static std::mutex                           resource_states_alliance_mtx;

static std::unordered_map<int64_t, int64_t> slot_states;
static std::mutex                           slot_states_mtx;

static std::unordered_map<int64_t, size_t> away_assignment_states;
static std::mutex                          away_assignment_states_mtx;

static std::unordered_map<int64_t, types::PlanetaryBaseState> structure_states;
static std::mutex                                             structure_states_mtx;

static std::unordered_map<int64_t, int64_t> pieces_states;
static std::mutex                           pieces_states_mtx;

static eastl::ring_buffer<uint64_t> previously_sent_battlelogs;
static std::mutex                   previously_sent_battlelogs_mtx;

static void load_previously_sent_logs()
{
  using json = nlohmann::json;
  std::scoped_lock lk(previously_sent_battlelogs_mtx);

  previously_sent_battlelogs.set_capacity(300);

  try {
    const std::filesystem::path path(File::MakePath(File::Battles()));
    std::ifstream              file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      spdlog::warn("Failed to open battles file (not found or not readable); starting with empty cache");
      return;
    }

    const auto battlelogs = json::parse(file);
    for (const auto& v : battlelogs) {
      previously_sent_battlelogs.push_back(v.get<uint64_t>());
    }

    spdlog::debug("Loaded {} previously sent battle logs", previously_sent_battlelogs.size());
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse battles file: {}", e.what());
  } catch (...) {
    spdlog::error("Failed to parse battles file");
  }
}

static void save_previously_sent_logs()
{
  using json           = nlohmann::json;
  auto battlelog_array = json::array();

  {
    std::scoped_lock lk(previously_sent_battlelogs_mtx);
    for (auto id : previously_sent_battlelogs) {
      battlelog_array.push_back(id);
    }
  }

  try {
    const std::filesystem::path path(File::MakePath(File::Battles(), true));
    std::ofstream               file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      spdlog::error("Failed to open battles file for writing");
      return;
    }

    file << battlelog_array.dump();
    spdlog::trace("Saved {} previously sent battle logs", battlelog_array.size());

  } catch (const std::exception& e) {
    spdlog::error("Failed to save battles JSON: {}", e.what());
  } catch (...) {
    spdlog::error("Unknown error while saving battles JSON.");
  }
}
} // namespace trackers

namespace workers
{

// A missed delta cannot be repaired by blindly resuming: processors already
// advanced their de-dup and first-sync state. Preserve each admitted FIFO prefix,
// stop new ordinary payloads for this process, and make the gap visible once.
static std::atomic_bool                     ordinary_sync_accepting{true};
static std::atomic_bool                     workers_ready{false};
static thread_local uint64_t                processing_generation = 0;
static sync_session::OrdinarySessionBinding ordinary_session;
static std::atomic<const char*>             overload_reason{nullptr};
static std::atomic_bool                     overload_reported{false};
static void                                 quarantine_ordinary_session() noexcept;

static void stop_ordinary_sync(const char* reason) noexcept
{
  ordinary_sync_accepting.store(false, std::memory_order_release);
  const char* empty = nullptr;
  overload_reason.compare_exchange_strong(empty, reason);
}

static void report_overload()
{
  const auto* reason = overload_reason.load(std::memory_order_acquire);
  if (reason && !overload_reported.exchange(true)) {
    spdlog::warn("Sync stopped accepting ordinary updates: {}. Sync may be incomplete. "
                 "Restart the game to establish a fresh sync snapshot.",
                 reason);
  }
}

static constexpr size_t   queue_mib = 1024 * 1024;
static std::atomic_size_t diagnostic_count{0}, diagnostic_bytes{0};

struct DiagnosticCharge {
  size_t bytes       = 0;
  DiagnosticCharge() = default;
  explicit DiagnosticCharge(size_t charge)
      : bytes(charge)
  {
  }
  DiagnosticCharge(const DiagnosticCharge&)            = delete;
  DiagnosticCharge& operator=(const DiagnosticCharge&) = delete;
  DiagnosticCharge(DiagnosticCharge&& other) noexcept
      : bytes(std::exchange(other.bytes, 0))
  {
  }
  ~DiagnosticCharge()
  {
    if (bytes) {
      diagnostic_bytes.fetch_sub(bytes);
      diagnostic_count.fetch_sub(1);
    }
  }
};

struct SyncData {
  // Destroy the string before releasing diagnostic admission, even on failure.
  DiagnosticCharge diagnostic;
  SyncConfig::Type type;
  bool             is_first_sync;
  uint64_t         generation;
  std::string      data;
};

struct DispatchWorker {
  sync_queue::BoundedQueue<SyncData> queue{{256, 32 * queue_mib, 16 * queue_mib}};
  std::atomic_bool                   ordinary_open{true};
  std::thread                        thread;
  DispatchWorker();
};

struct CombatTask {
  uint64_t id, generation;
};

struct CombatWorker {
  sync_queue::BoundedQueue<CombatTask> queue{{4096, 4096 * sizeof(CombatTask), sizeof(CombatTask)}};
  std::atomic_bool                     ordinary_open{true};
  std::thread                          thread;
  CombatWorker();
};

// Deliberately process lifetime, with joinable threads: no detach, static
// destructor, or worker join can run under the game DLL's loader lock.
static DispatchWorker* dispatch_worker = nullptr;
static CombatWorker*   combat_worker   = nullptr;

static bool queue_data(SyncConfig::Type type, std::string&& data, bool is_first_sync = false)
{
  const bool diagnostic = type == SyncConfig::Type::Diagnostics;
  if (!workers_ready.load(std::memory_order_acquire) || (!diagnostic && !dispatch_worker->ordinary_open.load()))
    return false;
  if (!diagnostic && !http::headers::IsCurrentSessionGeneration(processing_generation)) {
    quarantine_ordinary_session();
    return false;
  }
  const auto generation = diagnostic ? http::headers::CurrentSessionGeneration() : processing_generation;

  const auto       charge = sizeof(SyncData) + data.size() + 1;
  DiagnosticCharge diagnostic_charge;
  if (diagnostic) {
    // Best-effort diagnostics cannot consume the ordinary stream's full budget.
    if (charge > 256 * 1024 || diagnostic_count.fetch_add(1) >= 64) {
      if (charge <= 256 * 1024)
        diagnostic_count.fetch_sub(1);
      return false;
    }
    if (diagnostic_bytes.fetch_add(charge) > 4 * queue_mib - charge) {
      diagnostic_bytes.fetch_sub(charge);
      diagnostic_count.fetch_sub(1);
      return false;
    }
    diagnostic_charge.bytes = charge;
  }
  const auto admission = dispatch_worker->queue.TryEmplace(
      charge, [&] { return SyncData{std::move(diagnostic_charge), type, is_first_sync, generation, std::move(data)}; });
  if (admission == sync_queue::Admission::accepted)
    return true;
  if (!diagnostic) {
    dispatch_worker->ordinary_open.store(false);
    stop_ordinary_sync("ordinary delivery dispatch queue exceeded its bounds or failed admission");
    report_overload(); // This path runs only on owned-payload/background producers.
  }
  return false;
}

static bool queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync = false)
{
  // Parsing and serialization run on the fixed owned-payload worker, never under
  // the dispatch queue mutex or on the native response callback.
  if (type != SyncConfig::Type::Diagnostics && !http::headers::IsCurrentSessionGeneration(processing_generation)) {
    quarantine_ordinary_session();
    return false;
  }
  return queue_data(type, data.dump(), is_first_sync);
}

static void ship_sync_data(DispatchWorker& worker)
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  try {
    while (auto sync_data = worker.queue.WaitPop()) {
      try {
        auto& value = sync_data->value;
        if (http::headers::IsCurrentSessionGeneration(value.generation))
          http::send_data(value.type, value.data, value.is_first_sync, value.generation);
        else if (value.type != SyncConfig::Type::Diagnostics)
          quarantine_ordinary_session();
      } catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime("ship", e);
      } catch (const std::exception& e) {
        ErrorMsg::SyncMsg("ship", e.what());
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("ship", sz);
#if _WIN32
      } catch (winrt::hresult_error const& ex) {
        ErrorMsg::SyncWinRT("ship", ex);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("ship", "Unknown error during sending of sync data");
      }
      report_overload();
    }
  } catch (const std::exception& e) {
    spdlog::critical("ship_sync_data thread terminated: {}", e.what());
  } catch (...) {
    spdlog::critical("ship_sync_data thread terminated: unknown exception");
  }
  worker.ordinary_open.store(false);
  worker.queue.Close();
  stop_ordinary_sync("delivery dispatch worker terminated");
  report_overload();
}

DispatchWorker::DispatchWorker()
    : thread([this] {
      try {
        ship_sync_data(*this);
      } catch (...) {
        ordinary_open.store(false);
        queue.Close();
        stop_ordinary_sync("delivery dispatch worker initialization failed");
        report_overload();
      }
    })
{
}

static void ship_combat_log_data(CombatWorker& worker)
{
  using json = nlohmann::json;
  using namespace trackers::names;

#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  while (auto queued_id = worker.queue.WaitPop()) {
    const auto journal_id = queued_id->value.id;
    const auto generation = queued_id->value.generation;
    if (!http::headers::IsCurrentSessionGeneration(generation)) {
      quarantine_ordinary_session();
      report_overload();
      continue;
    }
    try {
      http::logging::trace("PROCESS", "combat log", STR_FORMAT("Fetching combat log for battle {}", journal_id));

      const json journals_body{{"journal_id", journal_id}};
      auto       battle_log = http::get_scopely_data("/journals/get", journals_body.dump(), generation);
      json       battle_json;

      if (!http::headers::IsCurrentSessionGeneration(generation)) {
        quarantine_ordinary_session();
        report_overload();
        continue;
      }
      if (battle_log.empty()) {
        continue;
      }

      try {
        battle_json = std::move(json::parse(battle_log));
      } catch (const json::exception& e) {
        spdlog::error("Error parsing journal response from game server: {}", e.what());
        continue;
      }

      const auto& journal              = battle_json["journal"];
      const auto& target_fleet_data    = journal["target_fleet_data"];
      const auto& initiator_fleet_data = journal["initiator_fleet_data"];

      auto       names      = json::object();
      const auto now        = std::chrono::steady_clock::now();
      {
        std::unordered_set<std::string> user_ids;
        collect_user_ids_from_fleet(target_fleet_data, user_ids);
        collect_user_ids_from_fleet(initiator_fleet_data, user_ids);
        collect_journal_participants(journal, user_ids);

        json profiles_request{{"user_ids", json::array()}};
        resolve_player_names(user_ids, names, profiles_request["user_ids"], now);

        const auto fetch_count = profiles_request["user_ids"].size();
        if (fetch_count > 0) {
          http::logging::trace("PROCESS", "combat log", STR_FORMAT("Fetching {} player profiles", fetch_count));

          auto profiles = http::get_scopely_data("/user_profile/profiles", profiles_request.dump(), generation);
          if (profiles.empty() || !http::headers::IsCurrentSessionGeneration(generation))
            continue;
          auto profiles_json = json::parse(profiles);

          std::scoped_lock lk(player_data_cache_mtx);
          if (!http::headers::IsCurrentSessionGeneration(generation))
            continue;
          const auto cache_now  = std::chrono::steady_clock::now();
          const auto expires_at = cache_now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);
          player_data_cache.Prune(cache_now);

          try {
            for (const auto& [player_id, profile] : profiles_json["user_profiles"].get<json::object_t>()) {
              const auto& name        = profile["name"].get<std::string>();
              const auto& alliance_id = profile["alliance_id"].get<int64_t>();

              names[player_id] = {
                  {"name", name}, {"alliance_id", alliance_id}, {"alliance_name", nullptr}, {"alliance_tag", nullptr}};
              if (!player_id.empty() && sync_resolver::BoundedField(player_id) && sync_resolver::BoundedField(name)
                  && http::headers::IsCurrentSessionGeneration(generation)) {
                player_data_cache.Put(player_id,
                                      CachedPlayerData{.name = name, .alliance = alliance_id, .expires_at = expires_at},
                                      cache_now);
              }
            }
          } catch (const json::exception& e) {
            spdlog::error("Failed to parse user profiles: {}", e.what());
          }
        }
      }

      {
        std::unordered_set<int64_t> alliance_ids;
        json alliances_request{{"user_current_rank", 0}, {"alliance_id", 0}, {"alliance_ids", json::array()}};

        collect_alliance_ids(names, alliance_ids);
        resolve_alliance_names(alliance_ids, names, alliances_request["alliance_ids"], now);

        const auto fetch_count = alliances_request["alliance_ids"].size();
        if (fetch_count > 0) {
          http::logging::trace("PROCESS", "combat log", STR_FORMAT("Fetching {} alliance profiles", fetch_count));

          auto profiles =
              http::get_scopely_data("/alliance/get_alliances_public_info", alliances_request.dump(), generation);
          if (profiles.empty() || !http::headers::IsCurrentSessionGeneration(generation))
            continue;
          auto profiles_json = json::parse(profiles);

          std::scoped_lock lk(alliance_data_cache_mtx);
          if (!http::headers::IsCurrentSessionGeneration(generation))
            continue;
          const auto cache_now  = std::chrono::steady_clock::now();
          const auto expires_at = cache_now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);
          alliance_data_cache.Prune(cache_now);

          try {
            for (const auto& [alliance_id_str, profile] : profiles_json["alliances_info"].get<json::object_t>()) {
              const auto  id   = profile["id"].get<int64_t>();
              const auto& name = profile["name"].get<std::string>();
              const auto& tag  = profile["tag"].get<std::string>();

              if (id > 0 && sync_resolver::BoundedField(name) && sync_resolver::BoundedField(tag)
                  && http::headers::IsCurrentSessionGeneration(generation)) {
                alliance_data_cache.Put(id, CachedAllianceData{.name = name, .tag = tag, .expires_at = expires_at},
                                        cache_now);
              }
            }
          } catch (json::exception& e) {
            spdlog::error("Failed to parse alliance profiles: {}", e.what());
          }

          for (const auto& [player_id, entry] : names.items()) {
            try {
              if (entry.contains("alliance_id")) {
                const auto alliance_id = entry["alliance_id"].get<int64_t>();
                if (const auto* cached = alliance_data_cache.Find(alliance_id, cache_now)) {
                  entry["alliance_name"] = cached->name;
                  entry["alliance_tag"]  = cached->tag;
                  entry.erase("alliance_id");
                }
              }
            } catch (json::exception& e) {
              spdlog::error("Failed to update cached player data: {}", e.what());
            }
          }
        }
      }

      auto battle_array = json::array();
      battle_array.push_back(
          {{"type", SyncConfig::Type::Battles}, {"names", names}, {"journal", battle_json["journal"]}});

      try {
        auto ship_data = battle_array.dump();
        if (http::headers::IsCurrentSessionGeneration(generation))
          http::send_data(SyncConfig::Type::Battles, ship_data, false, generation);
      } catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime("combat", e);
      } catch (const std::exception& e) {
        ErrorMsg::SyncException("combat", e);
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("combat", sz);
#if _WIN32
      } catch (winrt::hresult_error const& ex) {
        ErrorMsg::SyncWinRT("combat", ex);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("combat", "Unknown error during sending of sync data");
      }

    } catch (json::exception& e) {
      spdlog::error("Error parsing combat log or profiles: {}", e.what());
    } catch (std::exception& e) {
      spdlog::error("Error processing combat log: {}", e.what());
    } catch (...) {
      spdlog::error("Unknown error during processing of combat log data");
    }
  }
}

CombatWorker::CombatWorker()
    : thread([this] {
      try {
        ship_combat_log_data(*this);
      } catch (...) {
        ordinary_open.store(false);
        queue.Close();
        stop_ordinary_sync("combat download worker terminated");
        report_overload();
      }
    })
{
}

} // namespace workers

void PublishDiagnosticEvent(std::string_view stream, const nlohmann::json& event) noexcept
{
  try {
    if (!workers::workers_ready.load(std::memory_order_acquire) || !Config::Get().sync_options.diagnostics
        || !event.is_object())
      return;
    const auto sequence = event.find("seq");
    if (sequence == event.end() || !sequence->is_number_unsigned())
      return;
    static const std::string sourceId = http::newUUID();
    nlohmann::json envelope = {{"type", "diagnostic"}, {"schema", 1}, {"source_id", sourceId},
      {"producer_version", VER_FILE_VERSION_STR}, {"session", event.value("session", std::string{})},
      {"stream", stream}, {"sequence", sequence->dump()},
      {"event", event.value("event", event.value("phase", std::string{"observation"}))}, {"payload", event}};
    const auto utc = event.find("utc_ms");
    envelope["utc_ms"] = utc != event.end() && utc->is_number_integer() ? nlohmann::json(utc->dump()) : nlohmann::json(nullptr);
    auto body = nlohmann::json::array({std::move(envelope)}).dump();
    if (body.size() > 256 * 1024) {
      spdlog::warn("Diagnostic sync observation exceeds 256 KiB; dropped");
      return;
    }
    if (!workers::queue_data(SyncConfig::Type::Diagnostics, std::move(body))) {
      static std::atomic<uint64_t> dropped{0};
      if (dropped.fetch_add(1) % 64 == 0)
        spdlog::warn("Diagnostic sync dispatch bounds reached; dropping observations");
    }
  } catch (...) { /* Diagnostics must never throw into game code. */ }
}

namespace processors
{

// Builds the sync event for one away team assignment instance and appends it to `out_array` only if the
// instance's observable state changed since the last time it was emitted. Caller must hold
// trackers::away_assignment_states_mtx.
static void away_assignment_event(const Digit::PrimeServer::Models::AwayAssignmentInstance& instance,
                                  nlohmann::json&                                           out_array)
{
  using json = nlohmann::json;
  using trackers::away_assignment_states;

  // Protobuf map iteration order is not guaranteed to match slot order, so sort by slot index
  // before emitting officer_ids to keep the array (and therefore the de-dup hash) stable.
  std::vector<std::pair<int64_t, int64_t>> officer_slots(instance.officerids().begin(), instance.officerids().end());
  std::ranges::sort(officer_slots, {}, &std::pair<int64_t, int64_t>::first);

  auto officer_ids = json::array();
  for (const auto& [slot, officer_id] : officer_slots) {
    if (officer_id != 0) {
      officer_ids.push_back(officer_id);
    }
  }

  auto officer_traits = json::array();
  for (const auto& trait : instance.officertraits()) {
    officer_traits.push_back({{"tid", trait.traitid()}, {"max_level", trait.maxlevel()}});
  }

  // Same ordering concern as officer_ids: sort by trait id so the array and the de-dup hash are stable.
  std::vector<std::pair<int64_t, int32_t>> critical_trait_pairs(instance.parameters().criticaltraitscores().begin(),
                                                                instance.parameters().criticaltraitscores().end());
  std::ranges::sort(critical_trait_pairs, {}, &std::pair<int64_t, int32_t>::first);

  auto critical_trait_scores = json::array();
  for (const auto& [tid, score] : critical_trait_pairs) {
    critical_trait_scores.push_back({{"tid", tid}, {"score", score}});
  }

  json event = {{"type", SyncConfig::Type::AwayAssignments},
                {"aid", instance.id()},
                {"template_id", instance.awayassignmenttemplateid()},
                {"state", instance.state()},
                {"officer_ids", officer_ids},
                {"job_uuid", instance.jobuuid()},
                {"duration", instance.parameters().duration()},
                {"rarity", instance.rarity()},
                {"officer_traits", officer_traits},
                {"critical_trait_scores", critical_trait_scores},
                {"success_chance", instance.parameters().successchance()},
                {"critical_success_chance", instance.parameters().criticalsuccesschance()},
                {"max_critical_success_chance", instance.maxcriticalsuccesschance()},
                {"key_stat", instance.keystat()},
                {"max_assignable_officers", instance.maxassignableofficerscount()},
                {"attack_weight", instance.attackweight()},
                {"defense_weight", instance.defenseweight()},
                {"health_weight", instance.healthweight()}};

  const auto state_value = std::hash<json>{}(event);

  if (const auto& it = away_assignment_states.find(instance.id());
      it == away_assignment_states.end() || it->second != state_value) {
    away_assignment_states[instance.id()] = state_value;
    out_array.push_back(std::move(event));
  }
}

static void away_assignments_list(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::atomic_bool is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::AwayAssignmentUserListResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "away assignments",
                         STR_FORMAT("Processing {} away assignments", response.instances_size()));

    std::unordered_set<int64_t> ids_in_response;
    ids_in_response.reserve(static_cast<size_t>(response.instances_size()));
    auto assignment_array = json::array();

    {
      std::scoped_lock lk(trackers::away_assignment_states_mtx);

      for (const auto& instance : response.instances()) {
        ids_in_response.insert(instance.id());
        away_assignment_event(instance, assignment_array);
      }

      // Prune entries that are no longer present to prevent unbounded growth
      for (auto it = trackers::away_assignment_states.begin(); it != trackers::away_assignment_states.end();) {
        if (!ids_in_response.contains(it->first)) {
          assignment_array.push_back({{"type", "collected_" + SyncConfig::Type::AwayAssignments}, {"aid", it->first}});
          it = trackers::away_assignment_states.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!assignment_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::AwayAssignments, assignment_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse away assignments");
  }
}

static void away_assignment_instance(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto instance = Digit::PrimeServer::Models::AwayAssignmentInstance(); instance.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "away assignment instance",
                         STR_FORMAT("Processing away assignment {}", instance.id()));

    auto assignment_array = json::array();
    {
      std::scoped_lock lk(trackers::away_assignment_states_mtx);
      away_assignment_event(instance, assignment_array);
    }

    if (!assignment_array.empty()) {
      workers::queue_data(SyncConfig::Type::AwayAssignments, assignment_array);
    }
  } else {
    spdlog::error("Failed to parse away assignment instance");
  }
}

static void queue_battle_ids(const std::vector<uint64_t>& battle_ids)
{
  if (!workers::combat_worker || !workers::combat_worker->ordinary_open.load())
    return;
  const auto generation = workers::processing_generation;
  if (!http::headers::IsCurrentSessionGeneration(generation)) {
    workers::quarantine_ordinary_session();
    return;
  }
  size_t admitted = 0;
  {
    using trackers::previously_sent_battlelogs;
    using trackers::previously_sent_battlelogs_mtx;

    std::scoped_lock lk(previously_sent_battlelogs_mtx);

    for (const auto id : battle_ids | std::views::reverse) {
      if (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id)
          == previously_sent_battlelogs.end()) {
        const auto result = workers::combat_worker->queue.TryEmplace(
            sizeof(workers::CombatTask), [id, generation] { return workers::CombatTask{id, generation}; });
        if (result != sync_queue::Admission::accepted) {
          workers::combat_worker->ordinary_open.store(false);
          workers::stop_ordinary_sync("combat download queue exceeded its bounds or failed admission");
          workers::combat_worker->queue.Close();
          break;
        }
        // Do not persist a rejected ID as already sent. A later fresh session
        // must still be able to discover and retrieve it.
        previously_sent_battlelogs.push_back(id);
        ++admitted;
      }
    }
  }

  workers::report_overload();
  if (admitted) {
    http::logging::debug("QUEUE", "battle headers",
                         STR_FORMAT("Queuing {} battles for background processing", admitted));

    trackers::save_previously_sent_logs();
  }
}

static void battle_result_headers(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::BattleResultHeadersResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "battle headers",
                         STR_FORMAT("Processing {} battle headers", response.headers_size()));

    std::vector<uint64_t> battle_ids;
    battle_ids.reserve(static_cast<size_t>(response.headers_size()));

    for (const auto& header : response.headers()) {
      battle_ids.push_back(static_cast<uint64_t>(header.id()));
    }

    queue_battle_ids(battle_ids);
  } else {
    spdlog::error("Failed to parse battle result headers");
  }
}

struct ShipRecord {
  int64_t              id;
  int64_t              hull_id;
  int32_t              tier;
  int32_t              level;
  double               level_percentage;
  std::vector<int64_t> components;
};

static void sync_ships(const std::vector<ShipRecord>& ships)
{
  using json = nlohmann::json;
  using trackers::types::ShipState;

  static std::unordered_map<int64_t, ShipState> ship_states;
  static std::mutex                             ship_states_mtx;
  static std::atomic_bool                       is_first_sync{true};

  auto ship_array = json::array();
  {
    std::scoped_lock lk(ship_states_mtx);

    for (const auto& ship : ships) {
      const ShipState state{ship.tier, ship.level, ship.level_percentage, ship.components};

      if (const auto& it = ship_states.find(ship.id); it == ship_states.end() || it->second != state) {
        ship_states[ship.id] = state;
        ship_array.push_back({{"type", SyncConfig::Type::Ships},
                              {"psid", ship.id},
                              {"level", ship.level},
                              {"level_percentage", ship.level_percentage},
                              {"tier", ship.tier},
                              {"hull_id", ship.hull_id},
                              {"components", ship.components}});
      }
    }
  }

  if (!ship_array.empty()) {
    const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
    workers::queue_data(SyncConfig::Type::Ships, ship_array, first_sync);
  }
}

static void player_ships(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::PlayerShipsResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "ships", STR_FORMAT("Processing {} ships", response.ships_size()));

    std::vector<ShipRecord> ships;
    ships.reserve(static_cast<size_t>(response.ships_size()));

    for (const auto& ship : response.ships() | std::views::values) {
      ships.push_back({ship.id(), ship.hullid(), ship.tier(), ship.level(), ship.levelpercentage(),
                       std::vector<int64_t>(ship.components().begin(), ship.components().end())});
    }

    sync_ships(ships);
  } else {
    spdlog::error("Failed to parse player ships");
  }
}

static void battle_report(std::unique_ptr<std::string>&& bytes)
{
  // TODO: Placeholder for future client support; currently unused by the game client.
  spdlog::debug("process_battle_report() was called");
}

static void global_active_buffs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, std::pair<int32_t, int64_t>> buff_states;
  static std::mutex                                               buff_states_mtx;
  static std::atomic_bool                                         is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::GlobalActiveBuffsResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "global buffs",
                         STR_FORMAT("Processing {} active buffs", response.globalactivebuffs_size()));

    auto buff_array = json::array();
    {
      std::scoped_lock lk(buff_states_mtx);

      // Track all buff ids present in the response to detect removals.
      std::unordered_set<int64_t> present_ids;
      present_ids.reserve(static_cast<size_t>(response.globalactivebuffs_size()));

      for (const auto& buff : response.globalactivebuffs()) {
        present_ids.insert(buff.buffid());
        const bool expires = buff.has_activebuff() && buff.activebuff().has_expirytime();
        const auto state   = std::make_pair(buff.level(), expires ? buff.activebuff().expirytime().seconds() : -1);

        if (const auto& it = buff_states.find(buff.buffid()); it == buff_states.end() || it->second != state) {
          buff_states[buff.buffid()] = state;
          buff_array.push_back({{"type", SyncConfig::Type::Buffs},
                                {"bid", buff.buffid()},
                                {"level", state.first},
                                {"expiry_time", expires ? json(state.second) : json(nullptr)}});
        }
      }

      // Remove buffs that are no longer present and record each removal.
      for (auto it = buff_states.begin(); it != buff_states.end();) {
        if (!present_ids.contains(it->first)) {
          buff_array.push_back({
              {"type", "expired_" + SyncConfig::Type::Buffs},
              {"bid", it->first},
          });
          it = buff_states.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!buff_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Buffs, buff_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse global active buffs");
  }
}

static void alliance_games_props(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::atomic_int32_t emerald_chain_level{-1};

  if (auto response = Digit::PrimeServer::Models::AllianceGamePropertiesResponse(); response.ParseFromString(*bytes)) {

    for (const auto& prop : response.properties()) {
      if (prop.propertyname() == "claimed_loyalty_tiers") {
        const auto& claimed_ec_levels = prop.valuelist();
        const auto& max_element =
            std::ranges::max_element(claimed_ec_levels, {}, [](const auto& level) { return std::stoi(level); });
        const auto ec_level = max_element != claimed_ec_levels.end() ? std::stoi(*max_element) : -1;

        int32_t current_level = emerald_chain_level.load();
        if (ec_level != current_level && emerald_chain_level.compare_exchange_strong(current_level, ec_level)) {
          auto ag_array = json::array();
          ag_array.push_back({{"type", SyncConfig::Type::EmeraldChain}, {"level", ec_level}});
          workers::queue_data(SyncConfig::Type::EmeraldChain, ag_array);
        }

        break;
      }
    }
  } else {
    spdlog::error("Failed to parse alliance games properties");
  }
}

static void starbase_modules(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::module_states;
  using trackers::module_states_mtx;

  if (auto response = Digit::PrimeServer::Models::StarbaseModulesResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "starbase modules", STR_FORMAT("Processing {} buildings", response.modulestates_size()));

    auto starbase_array = json::array();
    {
      std::scoped_lock lk(module_states_mtx);

      for (const auto& module : response.modulestates()) {
        if (const auto& it = module_states.find(module.id()); it == module_states.end() || it->second != module.level()) {
          module_states[module.id()] = module.level();
          starbase_array.push_back({{"type", SyncConfig::Type::Buildings}, {"bid", module.id()}, {"level", module.level()}});
        }
      }
    }

    if (!starbase_array.empty()) {
      workers::queue_data(SyncConfig::Type::Buildings, starbase_array);
    }
  } else {
    spdlog::error("Failed to parse resources");
  }
}

static void planetary_base_data(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::pieces_states;
  using trackers::pieces_states_mtx;

  if (auto response = Digit::PrimeServer::Models::PlanetaryBase(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "planetary base data", STR_FORMAT("Processing {} haven building pieces", response.pieces_size()));
    auto pieces_array = json::array();
    {
      std::scoped_lock lk(pieces_states_mtx);

      for (const auto& [piece, count] : response.pieces()) {
        if (const auto& it = pieces_states.find(piece); it == pieces_states.end() || it->second != count) {
          pieces_states[piece] = count;
          pieces_array.push_back({{"type", SyncConfig::Type::Haven + "_pieces"}, {"sid", piece}, {"count", count}});
        }
      }
    }

    if (!pieces_array.empty()) {
      workers::queue_data(SyncConfig::Type::Haven, pieces_array);
    }
  }
}

static void planetary_map_building_data(const google::protobuf::Map<int64_t, Digit::PrimeServer::Models::PlanetaryMapData_BuildingData>& buildings)
{
  // TODO: use PlanetaryMapBuildingDiff for updates during game session

  using trackers::structure_states;
  using trackers::structure_states_mtx;
  using trackers::types::PlanetaryBaseState;

  auto structure_array = nlohmann::json::array();
  {
    std::scoped_lock lk(structure_states_mtx);

    for (const auto& [id, building] : buildings) {
      const PlanetaryBaseState state{building.level(), building.status(), building.position()};

      if (const auto& it = structure_states.find(id); it == structure_states.end() || it->second != state) {
        structure_states[id] = state;
        structure_array.push_back({{"type", SyncConfig::Type::Haven}, {"sid", building.specid()}, {"id", id}, {"level", building.level()}, {"status", building.status()}, {"position", building.position()}});
      }
    }
  }

  if (!structure_array.empty()) {
    workers::queue_data(SyncConfig::Type::Haven, structure_array, true);
  }
}

static void planetary_map_data(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::PlanetaryMapData(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "planetary map data", STR_FORMAT("Processing {} buildings", response.buildings_size()));
    planetary_map_building_data(response.buildings());
  }
}

static void planetary_map_prosperity(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::PlanetaryMapProsperityResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "planetary map prosperity", STR_FORMAT("Processing prosperity={}", response.prosperity()));
    const auto prosperity_array = nlohmann::json::array(
        {{{"type", SyncConfig::Type::Haven + "_prosperity"}, {"prosperity", response.prosperity()}}});
    workers::queue_data(SyncConfig::Type::Haven, prosperity_array, true);
  }
}

static void planetary_map(std::unique_ptr<std::string>&& bytes, const std::string& local_user_id)
{
  if (auto response = Digit::PrimeServer::Models::PlanetaryMapResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "planetary map", STR_FORMAT("Processing {} haven structures", response.mapdata().buildings_size()));

    const auto& owner = response.owneruserid();
    if (!owner.empty() && (local_user_id.empty() || owner != local_user_id)) {
      spdlog::debug("Planetary map data is not confirmed for the local user; skipping processing");
      return;
    }

    planetary_map_building_data(response.mapdata().buildings());
  } else {
    spdlog::error("Failed to parse planetary map");
  }
}

static void player_inventories(std::unique_ptr<std::string>&& bytes)
{
  using json   = nlohmann::json;
  using trackers::types::pairhash;
  using item_t = std::underlying_type_t<Digit::PrimeServer::Models::InventoryItemType>;

  static std::unordered_map<std::pair<item_t, int64_t>, int64_t, pairhash> inventory_states;
  static std::mutex                                                        inventory_states_mtx;
  static std::atomic_bool                                                  is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::InventoryResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "player inventories",
                         STR_FORMAT("Processing {} inventories", response.inventories_size()));

    auto inventory_items = json::array();
    {
      std::scoped_lock lk(inventory_states_mtx);

      for (const auto& inventory : response.inventories() | std::views::values) {
        for (const auto& item : inventory.items()) {
          if (item.has_commonparams()) {
            const auto item_id = item.commonparams().refid();
            const auto count   = item.count();
            const auto key     = std::make_pair(item.type(), item_id);

            if (const auto& it = inventory_states.find(key); it == inventory_states.end() || it->second != count) {
              inventory_states[key] = count;
              inventory_items.push_back({{"type", SyncConfig::Type::Inventory},
                                         {"item_type", item.type()},
                                         {"refid", item_id},
                                         {"count", count}});
            }
          }
        }
      }
    }

    if (!inventory_items.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Inventory, inventory_items, first_sync);
    }
  } else {
    spdlog::error("Failed to parse player inventories");
  }
}

static void jobs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using job_state = std::tuple<int64_t, int64_t, int64_t>;
  static std::unordered_map<std::string, job_state> jobs_active;
  static std::mutex                                 jobs_active_mtx;
  static std::atomic_bool                           is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::JobResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "jobs", STR_FORMAT("Processing {} jobs", response.jobs_size()));

    std::unordered_set<std::string> uuids_in_response;
    uuids_in_response.reserve(response.jobs_size());
    auto job_array = json::array();

    for (const auto& job : response.jobs()) {
      const std::string& uuid = job.uuid();
      uuids_in_response.insert(uuid);

      const job_state state{job.starttime().seconds(), job.duration(), job.reductioninseconds()};

      bool emit = false;
      {
        std::scoped_lock lk(jobs_active_mtx);
        const auto [it, inserted] = jobs_active.try_emplace(uuid, state);
        emit                      = inserted || it->second != state;
        it->second                = state;
      }

      if (!emit) {
        continue;
      }

      json job_params;

      switch (job.type()) {
        case Digit::PrimeServer::Models::JOBTYPE_RESEARCH: {
          const auto& research = job.researchparams();
          job_params           = {{"rid", research.projectid()}, {"level", research.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_STARBASECONSTRUCTION: {
          const auto& construction = job.starbaseconstructionparams();
          job_params               = {{"bid", construction.moduleid()}, {"level", construction.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPTIERUP: {
          const auto& upgrade = job.tierupshipparams();
          job_params          = {{"psid", upgrade.shipid()}, {"tier", upgrade.newtier()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPSCRAP: {
          const auto& scrap = job.scrapyardparams();
          job_params        = {{"psid", scrap.shipid()}, {"hull_id", scrap.hullid()}, {"level", scrap.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_AWAYASSIGNMENT: {
          const auto& away_assignment = job.awayassignmentparams();
          job_params                  = {{"aid", away_assignment.awayassignmentinstanceid()}};
        } break;
        default:
          continue;
      }

      json job_data = json::object({{"type", SyncConfig::Type::Jobs},
                                    {"job_type", job.type()},
                                    {"uuid", job.uuid()},
                                    {"start_time", job.starttime().seconds()},
                                    {"duration", job.duration()},
                                    {"reduction", job.reductioninseconds()}});

      job_data.update(job_params);
      job_array.push_back(std::move(job_data));
    }

    // Prune entries that are no longer present to prevent unbounded growth
    {
      std::scoped_lock lk(jobs_active_mtx);
      for (auto it = jobs_active.begin(); it != jobs_active.end();) {
        if (!uuids_in_response.contains(it->first)) {
          job_array.push_back({{"type", "completed_" + SyncConfig::Type::Jobs}, {"uuid", it->first}});
          it = jobs_active.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!job_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Jobs, job_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse jobs");
  }
}

static void active_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<int64_t> active_mission_states;
  static std::mutex                  active_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ActiveMissionsResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "active missions",
                         STR_FORMAT("Processing {} active missions", response.activemissions_size()));

    std::unordered_set<int64_t> active_missions;
    for (const auto& mission : response.activemissions()) {
      active_missions.insert(mission.id());
    }

    bool changed = false;
    {
      std::scoped_lock lk(active_mission_states_mtx);
      if (active_mission_states != active_missions) {
        changed               = true;
        active_mission_states = active_missions;
      }
    }

    if (changed && !active_missions.empty()) {
      auto mission_array = json::array();

      for (const auto mission : active_missions) {
        mission_array.push_back({{"type", "active_" + SyncConfig::Type::Missions}, {"mid", mission}});
      }

      workers::queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse active missions");
  }
}

static void completed_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::vector<int64_t> completed_mission_states;
  static std::mutex           completed_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::CompletedMissionsResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "completed missions",
                         STR_FORMAT("Processing {} completed missions", response.completedmissions_size()));

    const auto&          missions = response.completedmissions();
    std::vector<int64_t> completed_missions{missions.begin(), missions.end()};
    std::vector<int64_t> diff;

    // Assume the completed missions list is append-only: new entries may be added, but existing ones are never removed.
    {
      std::scoped_lock lk(completed_mission_states_mtx);
      std::ranges::set_difference(completed_missions, completed_mission_states, std::back_inserter(diff));

      if (!diff.empty()) {
        completed_mission_states = std::move(completed_missions);
      }
    }

    if (!diff.empty()) {
      auto mission_array = json::array();

      for (const auto mission : diff) {
        mission_array.push_back({{"type", SyncConfig::Type::Missions}, {"mid", mission}});
      }

      workers::queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse completed missions");
  }
}

static void officers(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::types::RankLevelShardsState;

  static std::unordered_map<uint64_t, RankLevelShardsState> officer_states;
  static std::mutex                                         officer_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficersResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "officers", STR_FORMAT("Processing {} officers", response.officers_size()));

    auto officers_array = json::array();
    {
      std::scoped_lock lk(officer_states_mtx);

      for (const auto& officer : response.officers()) {
        const RankLevelShardsState officer_state{officer.rankindex(), officer.level(), officer.shardcount()};

        if (const auto& it = officer_states.find(officer.id());
            it == officer_states.end() || it->second != officer_state) {
          officer_states[officer.id()] = officer_state;
          officers_array.push_back({{"type", SyncConfig::Type::Officer},
                                    {"oid", officer.id()},
                                    {"rank", officer.rankindex()},
                                    {"level", officer.level()},
                                    {"shard_count", officer.shardcount()}});
            }
      }
    }

    if (!officers_array.empty()) {
      workers::queue_data(SyncConfig::Type::Officer, officers_array);
    }
  } else {
    spdlog::error("Failed to parse officers");
  }
}

static void research_trees_state(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> research_states;
  static std::mutex                           research_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ResearchTreesState(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "research trees state",
                         STR_FORMAT("Processing {} research projects", response.researchprojectlevels_size()));

    auto research_array = json::array();
    {
      std::scoped_lock lk(research_states_mtx);

      for (const auto& [id, level] : response.researchprojectlevels()) {
        if (const auto& it = research_states.find(id); it == research_states.end() || it->second != level) {
          research_states[id] = level;
          research_array.push_back({{"type", SyncConfig::Type::Research}, {"rid", id}, {"level", level}});
        }
      }
    }

    if (!research_array.empty()) {
      workers::queue_data(SyncConfig::Type::Research, research_array);
    }
  } else {
    spdlog::error("Failed to parse research trees state");
  }
}

static void resources(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::resource_states;
  using trackers::resource_states_mtx;
  static std::atomic_bool is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::ResourcesResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "resources", STR_FORMAT("Processing {} resources", response.resources_size()));

    auto resource_array = json::array();
    {
      std::scoped_lock lk(resource_states_mtx);

      for (const auto& resource : response.resources()) {
        if (const auto& it = resource_states.find(resource.id()); it == resource_states.end() || it->second != resource.currentamount()) {
          resource_states[resource.id()] = resource.currentamount();
          resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", resource.id()}, {"amount", resource.currentamount()}});
        }
      }
    }

    if (!resource_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Resources, resource_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse resources");
  }
}

static void resources_delta(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::resource_states;
  using trackers::resource_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ResourcesDeltaResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "resources_delta", STR_FORMAT("Processing {} resource changes", response.resources_size()));

    auto resource_array = json::array();
    {
      std::scoped_lock lk(resource_states_mtx);

      for (const auto& resource : response.resources()) {
        if (const auto& it = resource_states.find(resource.id()); it == resource_states.end() || it->second != resource.amount()) {
          resource_states[resource.id()] = resource.amount();
          resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", resource.id()}, {"amount", resource.amount()}});
        }
      }
    }

    if (!resource_array.empty()) {
      workers::queue_data(SyncConfig::Type::Resources, resource_array);
    }

  } else {
    spdlog::error("Failed to parse resources delta");
  }
}

static void alliance_resources(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::resource_states_alliance;
  using trackers::resource_states_alliance_mtx;
  static std::atomic_bool is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::AllianceGetBankResourcesResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "alliance_resources", STR_FORMAT("Processing {} alliance resources", response.items_size()));

    auto resource_array = json::array();
    {
      std::scoped_lock lk(resource_states_alliance_mtx);

      for (const auto& item : response.items()) {
        if (item.has_commonparams()) {
          const auto id = item.commonparams().refid();
          const auto amount   = item.count();

          if (const auto& it = resource_states_alliance.find(id); it == resource_states_alliance.end() || it->second != amount) {
            resource_states_alliance[id] = amount;
            resource_array.push_back({{"type", "alliance_" + SyncConfig::Type::Resources}, {"rid", id}, {"amount", amount}});
          }
        }
      }
    }

    if (!resource_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Resources, resource_array, first_sync);
    }
  } else {
    spdlog::error("Failed to parse alliance resources");
  }
}

static void alliance_resources_delta(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::resource_states_alliance;
  using trackers::resource_states_alliance_mtx;

  if (auto response = Digit::PrimeServer::Models::AllianceResourcesDeltaResponse(); response.ParseFromString(*bytes)) {
    http::logging::trace("PROCESS", "alliance_resources_delta", STR_FORMAT("Processing {} alliance resource changes", response.resources_size()));

    auto resource_array = json::array();
    {
      std::scoped_lock lk(resource_states_alliance_mtx);

      for (const auto& resource : response.resources()) {
        if (const auto& it = resource_states_alliance.find(resource.id()); it == resource_states_alliance.end() || it->second != resource.amount()) {
          resource_states_alliance[resource.id()] = resource.amount();
          resource_array.push_back({{"type", "alliance_" + SyncConfig::Type::Resources}, {"rid", resource.id()}, {"amount", resource.amount()}});
        }
      }
    }

    if (!resource_array.empty()) {
      workers::queue_data(SyncConfig::Type::Resources, resource_array);
    }
  } else {
    spdlog::error("Failed to parse alliance resources delta");
  }
}

static void single_slot_locked(const Digit::PrimeServer::Models::EntitySlot& slot, nlohmann::json& slot_array)
{
  using json = nlohmann::json;

  json    slot_params;
  int64_t state_value = slot.has_slotitemid() ? slot.slotitemid().value() : -1;

  switch (slot.slottype()) {
    case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
      if (slot.has_consumableslotparams()) {
        const auto& consumable = slot.consumableslotparams();
        slot_params["expiry_time"] =
            consumable.has_expirytime() ? json(consumable.expirytime().seconds()) : json(nullptr);
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
      if (slot.has_officerpresetslotparams()) {
        const auto& preset = slot.officerpresetslotparams();
        slot_params        = {{"name", preset.name()}, {"order", preset.order()}, {"officer_ids", preset.officerids()}};
        state_value        = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
      if (slot.has_fleetcommanderslotparams()) {
        slot_params["order"] = slot.fleetcommanderslotparams().order();
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
      if (slot.has_selectableskillslotparams()) {
        const auto& skill = slot.selectableskillslotparams();
        slot_params["cooldown_expiration"] =
            skill.has_cooldownexpiration() ? json(skill.cooldownexpiration().seconds()) : json(nullptr);
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FORBIDDENTECH:
      if (slot.has_forbiddentechslotparams()) {
        const auto& tech = slot.forbiddentechslotparams();
        slot_params      = {{"owner_id", tech.ownerid()}, {"tier", tech.tier()}, {"level", tech.level()}};
        state_value      = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
      if (slot.has_fleetpresetslotparams()) {
        const auto& preset     = slot.fleetpresetslotparams();
        auto        setup_json = json::array();

        for (const auto& setup : preset.setups()) {
          setup_json.push_back({{"drydock_id", setup.drydockid()},
                                {"ship_id", setup.shipids()[0]},
                                {"officer_ids", setup.officerids()}});
        }

        slot_params = {{"name", preset.name()}, {"order", preset.order()}, {"setup", setup_json}};
        state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    default:
      return;
  }

  // Lock is not held because caller already does
  using trackers::slot_states;

  if (const auto& it = slot_states.find(slot.id()); it == slot_states.end() || it->second != state_value) {
    slot_states[slot.id()] = state_value;
    slot_array.push_back({{"type", SyncConfig::Type::Slots},
                          {"sid", slot.id()},
                          {"slot_type", slot.slottype()},
                          {"spec_id", slot.slotspecid()},
                          {"item_id", slot.has_slotitemid() ? json(slot.slotitemid().value()) : json(nullptr)},
                          {"params", slot_params}});
  }
}

static void entity_slots(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlots(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "entity slots", STR_FORMAT("Processing {} slots", response.entityslots__size()));

    auto slot_array = json::array();
    {
      std::scoped_lock lk(trackers::slot_states_mtx);

      for (const auto& slot : response.entityslots_()) {
        single_slot_locked(slot, slot_array);
      }
    }

    if (!slot_array.empty()) {
      workers::queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots");
  }
}

static void entity_slots_data(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlotsData(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "entity slots data",
                         STR_FORMAT("Processing {} slots", response.entityslots_size()));

    auto slot_array = json::array();
    {
      std::scoped_lock lk(trackers::slot_states_mtx);

      for (const auto& slots : response.entityslots()) {
        http::logging::trace("PROCESS", "entity slots data",
                             STR_FORMAT("Processing {} slots for entity {}", slots.slots_size(),
                                        static_cast<int32_t>(slots.entitytype())));

        for (const auto& slot : slots.slots()) {
          single_slot_locked(slot, slot_array);
        }
      }
    }

    if (!slot_array.empty()) {
      workers::queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots data");
  }
}

static void forbidden_tech(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::types::RankLevelShardsState;

  static std::unordered_map<uint64_t, RankLevelShardsState> tech_states;
  static std::mutex                                         tech_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ForbiddenTechsResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "techs",
                         STR_FORMAT("Processing {} forbidden/chaos techs", response.forbiddentechs_size()));

    auto tech_array = json::array();
    {
      std::scoped_lock lk(tech_states_mtx);

      for (const auto& tech : response.forbiddentechs()) {
        const RankLevelShardsState tech_state{tech.tier(), tech.level(), tech.shardcount()};

        if (const auto& it = tech_states.find(tech.id()); it == tech_states.end() || it->second != tech_state) {
          tech_states[tech.id()] = tech_state;
          tech_array.push_back({{"type", SyncConfig::Type::Tech},
                                {"fid", tech.id()},
                                {"tier", tech.tier()},
                                {"level", tech.level()},
                                {"shard_count", tech.shardcount()}});
        }
      }
    }

    if (!tech_array.empty()) {
      workers::queue_data(SyncConfig::Type::Tech, tech_array);
    }
  } else {
    spdlog::error("Failed to parse forbidden techs");
  }
}

static void active_officer_traits(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  using trackers::types::pairhash;

  static std::unordered_map<std::pair<int64_t, int64_t>, int32_t, pairhash> trait_states;
  static std::mutex                                                         trait_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficerTraitsResponse(); response.ParseFromString(*bytes)) {

    http::logging::trace("PROCESS", "active officer traits",
                         STR_FORMAT("Processing {} active officer traits", response.activeofficertraits_size()));

    auto trait_array = json::array();
    {
      std::scoped_lock lk(trait_states_mtx);

      for (const auto& [officer_id, officer_traits] : response.activeofficertraits()) {
        for (const auto& trait : officer_traits.activetraits() | std::views::values) {
          const auto& key = std::make_pair(officer_id, trait.traitid());

          if (const auto& it = trait_states.find(key); it == trait_states.end() || it->second != trait.level()) {
            trait_states[key] = trait.level();
            trait_array.push_back({{"type", SyncConfig::Type::Traits},
                                   {"oid", officer_id},
                                   {"tid", trait.traitid()},
                                   {"level", trait.level()}});
          }
        }
      }
    }

    if (!trait_array.empty()) {
      workers::queue_data(SyncConfig::Type::Traits, trait_array);
    }
  } else {
    spdlog::error("Failed to parse active officer traits");
  }
}

namespace json
{
  static void battle_result_headers(const nlohmann::json& section)
  {
    http::logging::trace("PROCESS", "battle headers", STR_FORMAT("Processing {} battle headers", section.size()));

    std::vector<uint64_t> battle_ids;
    battle_ids.reserve(section.size());

    for (const auto& battle : section) {
      const auto id = battle["id"].get<uint64_t>();
      battle_ids.push_back(id);
    }

    queue_battle_ids(battle_ids);
  }

  static void resources(const nlohmann::json& section)
  {
    using json = nlohmann::json;
    using trackers::resource_states;
    using trackers::resource_states_mtx;
    static std::atomic_bool is_first_sync{true};

    http::logging::trace("PROCESS", "resources", STR_FORMAT("Processing {} resources (JSON)", section.size()));

    auto resource_array = json::array();
    {
      std::scoped_lock lk(resource_states_mtx);

      for (const auto& [str_id, resource] : section.get<json::object_t>()) {
        auto id     = std::stoll(str_id);
        auto amount = resource["current_amount"].get<int64_t>();

        if (const auto& it = resource_states.find(id); it == resource_states.end() || it->second != amount) {
          resource_states[id] = amount;
          resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", id}, {"amount", amount}});
        }
      }
    }

    if (!resource_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      workers::queue_data(SyncConfig::Type::Resources, resource_array, first_sync);
    }
  }

  static void starbase_modules(const nlohmann::json& section)
  {
    using json = nlohmann::json;
    using trackers::module_states;
    using trackers::module_states_mtx;

    http::logging::trace("PROCESS", "starbase modules", STR_FORMAT("Processing {} buildings (JSON)", section.size()));

    auto starbase_array = json::array();
    {
      std::scoped_lock lk(module_states_mtx);

      for (const auto& module : section.get<json::object_t>() | std::views::values) {
        const auto id    = module["id"].get<int64_t>();
        const auto level = module["level"].get<int32_t>();

        if (const auto& it = module_states.find(id); it == module_states.end() || it->second != level) {
          module_states[id] = level;
          starbase_array.push_back({{"type", SyncConfig::Type::Buildings}, {"bid", id}, {"level", level}});
        }
      }
    }

    if (!starbase_array.empty()) {
      workers::queue_data(SyncConfig::Type::Buildings, starbase_array);
    }
  }

  static void ships(const nlohmann::json& section)
  {
    using json = nlohmann::json;

    http::logging::trace("PROCESS", "ships", STR_FORMAT("Processing {} ships (JSON)", section.size()));

    std::vector<ShipRecord> records;
    records.reserve(section.size());

    for (const auto& ship : section.get<json::object_t>() | std::views::values) {
      records.push_back({ship["id"].get<int64_t>(), ship["hull_id"].get<int64_t>(), ship["tier"].get<int32_t>(),
                         ship["level"].get<int32_t>(), ship["level_percentage"].get<double_t>(),
                         ship["components"].get<std::vector<int64_t>>()});
    }

    sync_ships(records);
  }

  static void parse(std::unique_ptr<std::string>&& bytes)
  {
    using json = nlohmann::json;
    const auto& sync_options = Config::Get().sync_options;

    try {
      const auto result = json::parse(bytes->begin(), bytes->end());

      for (const auto& [key, section] : result.items()) {
        if (key == "battle_result_headers") {
          if (!sync_options.battlelogs) {
            continue;
          }

          battle_result_headers(section);

        } else if (key == "resources") {
          if (!sync_options.resources) {
            continue;
          }

          resources(section);

        } else if (key == "starbase_modules") {
          if (!sync_options.buildings) {
            continue;
          }

          starbase_modules(section);

        } else if (key == "ships") {
          if (!sync_options.ships) {
            continue;
          }

          ships(section);
        }
      }
    } catch (const json::exception& e) {
      spdlog::error("Error parsing json: {}", e.what());
    }
  }
} // namespace json

namespace rtc
{
  static void entity_slots(std::unique_ptr<std::string>&& json_payload)
  {
    using json = nlohmann::json;

    try {
      auto data = json::parse(*json_payload);
      http::logging::trace("PROCESS", "entity slots (RTC)", "Processing entity slot update");

      const auto item    = data["slot_item_id"];
      const auto item_id = item.is_null() ? -1 : item.get<int64_t>();

      json    slot_params;
      int64_t state_value = item_id;

      const auto type = data["slot_type"].get<int32_t>();
      switch (type) {
        case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
          if (const auto& expiry_time = data["consumable_slot_params"]["expiry_time"]; expiry_time.is_null()) {
            slot_params["expiry_time"] = nullptr;
          } else {
            const auto timestamp =
                parse_timestamp(data["consumable_slot_params"]["expiry_time"].get_ref<const std::string&>());
            if (timestamp.has_value()) {
              slot_params["expiry_time"] = timestamp.value().time_since_epoch().count();
            } else {
              slot_params["expiry_time"] = nullptr;
            }
          }
          break;
        case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
          if (const auto& preset = data["officer_preset_slot_params"]; !preset.is_null()) {
            slot_params = preset;
            state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
          }
          break;
        case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
          if (const auto& params = data["fleet_commander_slot_params"]; !params.is_null()) {
            slot_params["order"] = params["order"];
          }
          break;
        case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
          if (const auto& params = data["selectable_skill_slot_params"]; !params.is_null()) {
            if (const auto& expiry_time = params["cooldown_expiration"]; expiry_time.is_null()) {
              slot_params["cooldown_expiration"] = nullptr;
            } else {
              const auto timestamp = parse_timestamp(params["cooldown_expiration"].get_ref<const std::string&>());
              if (timestamp.has_value()) {
                slot_params["cooldown_expiration"] = timestamp.value().time_since_epoch().count();
              } else {
                slot_params["cooldown_expiration"] = nullptr;
              }
            }
          }
          break;
        case Digit::PrimeServer::Models::SLOTTYPE_FORBIDDENTECH:
          if (const auto& tech = data["forbidden_tech_slot_params"]; !tech.is_null()) {
            slot_params = tech;
            state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
          }
          break;
        case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
          if (const auto& params = data["fleet_preset_slot_params"]; !params.is_null()) {
            auto setup_json = json::array();
            for (const auto& setup : params["setups"]) {
              setup_json.push_back(
                  {{"drydock_id", setup["d"]}, {"ship_id", setup["s"][0]}, {"officer_ids", setup["o"]}});
            }

            slot_params = {{"name", params["name"]}, {"order", params["order"]}, {"setup", setup_json}};
            state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
          }
          break;
        default:
          return;
      }

      const auto id = data["slot_id"].get<int64_t>();
      {
        using trackers::slot_states;
        using trackers::slot_states_mtx;

        std::scoped_lock lk(slot_states_mtx);

        if (const auto& it = slot_states.find(id); it == slot_states.end() || it->second != state_value) {
          slot_states[id] = state_value;

          auto slot_array = json::array();
          slot_array.push_back({{"type", SyncConfig::Type::Slots},
                                {"sid", id},
                                {"slot_type", type},
                                {"spec_id", data["slot_spec_id"]},
                                {"item_id", item},
                                {"params", slot_params}});

          workers::queue_data(SyncConfig::Type::Slots, slot_array);
        }
      }
    } catch (json::exception& e) {
      spdlog::error("Failed to parse slots JSON: {}", e.what());
    }
  }
} // namespace rtc

} // namespace processors

namespace workers
{
using PayloadProcessor = void (*)(std::unique_ptr<std::string>&&);
struct PayloadTask {
  PayloadProcessor             process = nullptr;
  std::unique_ptr<std::string> bytes;
  std::string                  local_user_id;
  uint64_t                     generation = 0;
};

struct PayloadWorker {
  sync_queue::BoundedQueue<PayloadTask> queue{{128, 32 * queue_mib, 16 * queue_mib}};
  std::thread                           thread;
  PayloadWorker()
      : thread([this] { Run(); })
  {
  }

  void Run() noexcept
  {
    try {
      while (auto item = queue.WaitPop()) {
        report_overload();
        auto& task = item->value;
        if (task.bytes && !http::headers::IsCurrentSessionGeneration(task.generation)) {
          quarantine_ordinary_session();
          continue;
        }
        processing_generation = task.generation;
        try {
          if (task.bytes) {
            if (task.process)
              task.process(std::move(task.bytes));
            else
              processors::planetary_map(std::move(task.bytes), task.local_user_id);
          }
        } catch (...) {
          stop_ordinary_sync("owned payload processing failed; the stream may be incomplete");
        }
        processing_generation = 0;
        report_overload();
      }
      report_overload();
    } catch (...) {
      queue.Close();
      stop_ordinary_sync("owned payload worker terminated");
      report_overload();
    }
  }

  template <class Factory> bool Submit(uint64_t generation, size_t charge, Factory&& factory)
  {
    if (!ordinary_sync_accepting.load(std::memory_order_acquire))
      return false;
    if (!ordinary_session.Admit(generation, http::headers::CurrentSessionGeneration())) {
      quarantine_ordinary_session();
      return false;
    }
    if (queue.TryEmplace(charge, std::forward<Factory>(factory)) == sync_queue::Admission::accepted)
      return true;
    stop_ordinary_sync("owned payload queue exceeded its bounds or failed admission");
    // Close wakes an idle consumer without allocating a notice or logging on
    // the game caller. Existing leases and the admitted FIFO prefix still drain.
    queue.Close();
    return false;
  }
};

static PayloadWorker* payload_worker = nullptr;

static void quarantine_ordinary_session() noexcept
{
  stop_ordinary_sync("the login session changed after ordinary sync began; cached deltas require a fresh process");
  if (payload_worker)
    payload_worker->queue.Close();
}

// Called after the native login snapshot changes. Never log, serialize, wait
// for a worker or touch disk from the native callback; Close wakes its reporter.
static void on_session_change() noexcept
{
  if (ordinary_session.SessionChanged(http::headers::CurrentSessionGeneration()))
    quarantine_ordinary_session();
}

static bool start_workers()
{
  const auto& options  = Config::Get().sync_options;
  const bool  ordinary = std::ranges::any_of(SyncOptions, [&options](const auto& option) {
    return option.type != SyncConfig::Type::Diagnostics && options.*option.option;
  });
  if (!ordinary && !options.diagnostics)
    return true;
  try {
    dispatch_worker = new DispatchWorker;
    if (options.battlelogs)
      combat_worker = new CombatWorker;
    if (ordinary)
      payload_worker = new PayloadWorker;
    workers_ready.store(true, std::memory_order_release);
    return true;
  } catch (...) {
    stop_ordinary_sync("sync worker startup failed");
    report_overload();
    return false;
  }
}

static bool submit_payload(const char* bytes, size_t count, PayloadProcessor process, std::string local_user_id = {})
{
  if (!workers_ready.load(std::memory_order_acquire) || !payload_worker)
    return false;
  const auto generation = http::headers::CurrentSessionGeneration();
  const auto charge     = sizeof(PayloadTask) + sizeof(std::string) + count + local_user_id.size() + 2;
  return payload_worker->Submit(generation, charge, [&] {
    return PayloadTask{process, std::make_unique<std::string>(bytes, count), std::move(local_user_id), generation};
  });
}
} // namespace workers

static void HandleEntityGroup(EntityGroup* entity_group)
{
  if (!workers::workers_ready.load(std::memory_order_acquire) || !workers::payload_worker
      || !workers::ordinary_sync_accepting.load(std::memory_order_acquire) || entity_group == nullptr
      || entity_group->Group == nullptr || entity_group->Group->bytes == nullptr || entity_group->Group->Length <= 0) {
    return;
  }

  const auto  byteCount = static_cast<size_t>(entity_group->Group->Length);
  const auto* bytesPtr  = reinterpret_cast<const char*>(entity_group->Group->bytes->m_Items);

  // Admission precedes the native byte copy. One consumer preserves the order
  // of full snapshots, deltas, removals and first-sync markers across categories.
  auto submit = [bytesPtr, byteCount](workers::PayloadProcessor process) {
    return workers::submit_payload(bytesPtr, byteCount, process);
  };
  const auto& sync_options = Config::Get().sync_options;

  switch (entity_group->Type_) {
    // away assignments
    case EntityGroup::Type::AwayAssignmentsList:
      if (sync_options.away_assignments) {
        submit(processors::away_assignments_list);
      }
      break;
    case EntityGroup::Type::AwayAssignmentsInstance:
      if (sync_options.away_assignments) {
        submit(processors::away_assignment_instance);
      }
      break;

    // battlelogs
    case EntityGroup::Type::BattleResultHeaders:
      if (sync_options.battlelogs) {
        submit(processors::battle_result_headers);
      }
      break;
    case EntityGroup::Type::BattleReport:
      if (sync_options.battlelogs) {
        submit(processors::battle_report);
      }
      break;
    case EntityGroup::Type::UserProfiles:
      if (sync_options.battlelogs) {
        submit(trackers::names::cache_player_names);
      }
      break;
    case EntityGroup::Type::AllianceProfiles:
      if (sync_options.battlelogs) {
        submit(trackers::names::cache_alliance_names);
      }
      break;

    // buffs
    case EntityGroup::Type::GlobalActiveBuffs:
      if (sync_options.buffs) {
        submit(processors::global_active_buffs);
      }
      break;
    case EntityGroup::Type::AllianceGetGameProperties:
      if (sync_options.buffs) {
        submit(processors::alliance_games_props);
      }
      break;

    // buildings
    case EntityGroup::Type::StarbaseModules:
      if (sync_options.buildings) {
        submit(processors::starbase_modules);
      }
      break;

    // haven
    case EntityGroup::Type::PlanetaryBaseData:
      if (sync_options.haven) {
        static std::once_flag haven_init_base_flag;
        std::call_once(haven_init_base_flag, [&] { submit(processors::planetary_base_data); });
      }
      break;

    case EntityGroup::Type::PlanetaryMapData:
      if (sync_options.haven) {
        static std::once_flag haven_init_map_flag;
        std::call_once(haven_init_map_flag, [&] { submit(processors::planetary_map_data); });
      }
      break;

    case EntityGroup::Type::PlanetaryMapProsperity:
      if (sync_options.haven) {
        static std::once_flag haven_init_prosperity_flag;
        std::call_once(haven_init_prosperity_flag, [&] { submit(processors::planetary_map_prosperity); });
      }
      break;

    case EntityGroup::Type::PlanetaryMapResponse:
      if (sync_options.haven) {
        // Read managed state on the native callback thread. The fixed payload
        // worker receives only owned strings, never Unity/IL2CPP objects.
        std::string local_user_id;
        static auto manager_parent = UserProfileManager::get_class_helper().GetParent("MonoSingleton`1");
        // Closed generic singleton getters are inflated; the detour resolver
        // intentionally rejects them. Managed invocation supports this getter.
        static const auto* manager_instance = []() -> const MethodInfo* {
          const auto* method = manager_parent.GetMethodInfo("get_Instance", 0);
          return method && !method->is_generic && (method->flags & METHOD_ATTRIBUTE_STATIC)
                         && method->parameters_count == 0
                         && method_contract::Type(method->return_type, "Digit.Prime.PlayerProfile.UserProfileManager")
                     ? method : nullptr;
        }();
        static const auto* local_profile = method_contract::Resolve(
            UserProfileManager::get_class_helper().get_cls(), "GetLocalUserProfile", false,
            "Digit.PrimeServer.Models.UserProfile", {});
        static auto profile_class = il2cpp_get_class_helper(
            "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "UserProfile");
        static const auto* user_id = method_contract::Resolve(
            profile_class.get_cls(), "get_UserId", false, "System.String", {});
        Il2CppObject* profile = nullptr;
        Il2CppObject* id = nullptr;
        Il2CppObject* manager = nullptr;
        if (Il2CppRuntime::TryInvoke(manager_instance, nullptr, nullptr, &manager) && manager
            && Il2CppRuntime::TryInvoke(local_profile, manager, nullptr, &profile) && profile
            && Il2CppRuntime::TryInvoke(user_id, profile, nullptr, &id) && id) {
          local_user_id = to_string(reinterpret_cast<Il2CppString*>(id));
        }
        workers::submit_payload(bytesPtr, byteCount, nullptr, std::move(local_user_id));
      }
      break;

    // inventory
    case EntityGroup::Type::PlayerInventories:
      if (sync_options.inventory) {
        submit(processors::player_inventories);
      }
      break;

    // jobs
    case EntityGroup::Type::Jobs:
      if (sync_options.jobs) {
        submit(processors::jobs);
      }
      break;

    // missions
    case EntityGroup::Type::ActiveMissions:
      if (sync_options.missions) {
        submit(processors::active_missions);
      }
      break;
    case EntityGroup::Type::CompletedMissions:
      if (sync_options.missions) {
        submit(processors::completed_missions);
      }
      break;

    // officer
    case EntityGroup::Type::Officers:
      if (sync_options.officers) {
        submit(processors::officers);
      }
      break;

    // research
    case EntityGroup::Type::ResearchTreesState:
      if (sync_options.research) {
        submit(processors::research_trees_state);
      }
      break;

    // resources
    case EntityGroup::Type::Resources:
      if (sync_options.resources) {
        submit(processors::resources);
      }
      break;
    case EntityGroup::Type::ResourcesDelta:
      if (sync_options.resources) {
        submit(processors::resources_delta);
      }
      break;
    case EntityGroup::Type::AllianceGetBankResources:
      if (sync_options.resources) {
        submit(processors::alliance_resources);
      }
      break;
    case EntityGroup::Type::ResourcesDeltaAlliance:
      if (sync_options.resources) {
        submit(processors::alliance_resources_delta);
      }
      break;

    // ships
    case EntityGroup::Type::Ships:
      if (sync_options.ships) {
        submit(processors::player_ships);
      }
      break;

    // slots
    case EntityGroup::Type::EntitySlots:
      if (sync_options.slots) {
        submit(processors::entity_slots);
      }
      break;
    case EntityGroup::Type::EntitySlotsData:
      if (sync_options.slots) {
        submit(processors::entity_slots_data);
      }
      break;

    // tech
    case EntityGroup::Type::ForbiddenTechs:
      if (sync_options.tech) {
        submit(processors::forbidden_tech);
      }
      break;

    // traits
    case EntityGroup::Type::ActiveOfficerTraits:
      if (sync_options.traits) {
        submit(processors::active_officer_traits);
      }
      break;

    // misc
    case EntityGroup::Type::Json:
      if (sync_options.battlelogs || sync_options.resources || sync_options.buildings || sync_options.ships)
        submit(processors::json::parse);
      break;

    default:
      break;
  }
}

namespace hooks
{
// IL2CPP value type: keep this parameter by value so following arguments retain their platform ABI positions.
struct CentrifugoInfo {
  Il2CppString* Channel;
  int32_t       Offset;
};

static_assert(sizeof(CentrifugoInfo) == 16);

static void* RtcParser_ParseFinalPayload(auto original, void* _this, CentrifugoInfo centrifugoInfo,
                                         RealtimeDataPayload* realtimeDataPayload, void* finalPayload)
{
  if (workers::workers_ready.load(std::memory_order_acquire) && workers::payload_worker
      && workers::ordinary_sync_accepting.load(std::memory_order_acquire) && Config::Get().sync_options.slots
      && realtimeDataPayload) {
    // Read getters once on their native caller. Admission uses a conservative
    // UTF-8 bound before conversion; the factory copies only these borrowed
    // characters and never calls a native getter or retains a managed pointer.
    const auto* type = realtimeDataPayload->DataType;
    if (!type || type->length != 1 || type->chars[0] != u'0')
      return original(_this, centrifugoInfo, realtimeDataPayload, finalPayload);
    const auto* data = realtimeDataPayload->Data;
    if (data && data->length > 0) {
      const auto charge =
          sizeof(workers::PayloadTask) + sizeof(std::string) + static_cast<size_t>(data->length) * 3 + 1;
      const auto generation = http::headers::CurrentSessionGeneration();
      workers::payload_worker->Submit(generation, charge, [data, generation] {
        return workers::PayloadTask{
            processors::rtc::entity_slots, std::make_unique<std::string>(to_string(data)), {}, generation};
      });
    }
  }

  return original(_this, centrifugoInfo, realtimeDataPayload, finalPayload);
}

static void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, void* parsing_context,
                                                          ServiceResponse* service_response, MethodInfo* method)
{
  if (!workers::workers_ready.load(std::memory_order_acquire) || !workers::payload_worker
      || !workers::ordinary_sync_accepting.load(std::memory_order_acquire))
    return original(_this, parsing_context, service_response, method);
  auto* const entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    auto* const entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, parsing_context, service_response, method);
}

static void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl,
                                     Il2CppString* gatewayServerUrl, Il2CppString* sessionId,
                                     Il2CppString* serverRegion)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId, serverRegion);
  http::headers::state.SetServer(to_string(gameServerUrl), to_string(sessionId));
  workers::on_session_change();
}

static void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                                  bool encryptRequests, Il2CppString* serverRegion)
{
  original(_this, sessionId, gameVersion, encryptRequests, serverRegion);
  http::headers::state.SetVersion(to_string(gameVersion));
}

static void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::state.SetInstance(instanceId);
}

} // namespace hooks

void InstallSyncPatches()
{
  if (!workers::start_workers())
    return;
  if (Config::Get().sync_options.battlelogs)
    trackers::load_previously_sent_logs();

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
  } else {
    if (auto *const ptr = game_server_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInternal");
    } else {
      SPUD_STATIC_DETOUR(ptr, hooks::GameServerModelRegistry_ProcessResultInternal);
    }
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    if (auto *const ptr = prime_app.GetMethod("InitPrimeServer"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
    } else {
      SPUD_STATIC_DETOUR(ptr, hooks::PrimeApp_InitPrimeServer);
    }
  }

  if (auto game_server =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
      !game_server.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
  } else {
    // This hook only captures the game version for the X-PRIME-VERSION sync header
    // (the session snapshot has a version fallback). It is optional so users can turn
    // it off when another mod detours the same method — a native and a managed detour
    // on one prologue corrupt each other and crash at login.
    if (!Config::Get().installGameVersionHook) {
      spdlog::info("Sync: skipping GameServer::Initialise hook (patches.game_version = false)");
    } else if (auto *const ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
    } else {
      SPUD_STATIC_DETOUR(ptr, hooks::GameServer_Initialise);
    }

    if (auto *const ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, hooks::GameServer_SetInstanceIdHeader);
    }
  }

  if (auto slot_assign_parser = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotAssignRtcParser");
    !slot_assign_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotAssignRtcParser");
    } else {
      if (auto* const ptr = slot_assign_parser.GetMethod("ParseFinalPayload"); ptr == nullptr) {
        ErrorMsg::MissingMethod("SlotAssignRtcParser", "ParseFinalPayload");
      } else {
        SPUD_STATIC_DETOUR(ptr, hooks::RtcParser_ParseFinalPayload);
      }
    }

  if (auto slot_clear_parser = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Parsers", "SlotClearRtcParser");
    !slot_clear_parser.isValidHelper()) {
    ErrorMsg::MissingHelper("Parsers", "SlotClearRtcParser");
    } else {
      if (auto* const ptr = slot_clear_parser.GetMethod("ParseFinalPayload"); ptr == nullptr) {
        ErrorMsg::MissingMethod("SlotClearRtcParser", "ParseFinalPayload");
      } else {
        SPUD_STATIC_DETOUR(ptr, hooks::RtcParser_ParseFinalPayload);
      }
    }

}
