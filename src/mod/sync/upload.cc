// Each sync target's upload thread: takes the next upload from its Delivery and POSTs it, outside the state lock.

#include "sync/sync.h"
#include "version.h"

#include <cpr/cpr.h>
#include <curl/curl.h>

#include <memory>

namespace data_sync
{
namespace
{
  // Set up once per target configuration; a reload that changes the target makes a new one. Owned by the target's
  // thread alone.
  std::unique_ptr<cpr::Session> NewTargetSession(const SyncTargetConfig& config)
  {
    auto http = std::make_unique<cpr::Session>();
    http->SetUrl(config.url);
    http->SetUserAgent("stfc community mod " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
    http->SetAcceptEncoding(cpr::AcceptEncoding{});
    http->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
    http->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});
    http->SetConnectTimeout(cpr::ConnectTimeout{3'000});
    http->SetTimeout(cpr::Timeout{10'000});
    if (!config.proxy.empty())
      http->SetProxies({{"http", config.proxy}, {"https", config.proxy}});
    if (!config.verify_ssl)
      http->SetSslOptions(cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true}));
    http->SetHeader({{"Content-Type", "application/json"},
                     {"X-Powered-By", "stfc community mod/" VER_FILE_VERSION_STR},
                     {"stfc-sync-token", config.token}});
    return http;
  }
} // namespace

// The registry owns the target; this thread only borrows it per upload, so a removed target finishes its request
// and then the thread ends, without the game thread joining it.
void UploadTarget(std::weak_ptr<Destination> target, std::string name)
{
  auto&                         state = State();
  std::unique_ptr<cpr::Session> http;
  uint64_t                      configured        = 0;
  bool                          prefer_diagnostic = false;
  try {
    for (;;) {
      std::unique_lock lock(state.mutex);
      const auto       destination = target.lock();
      if (!destination || destination->unavailable)
        return;
      auto& delivery             = destination->delivery;
      using DiagnosticLease      = sync_queue::BoundedQueue<DiagnosticObservation>::Lease;
      const auto take_diagnostic = [&]() -> std::optional<DiagnosticLease> {
        while (auto observation = destination->diagnostics.TryPop()) {
          if (DiagnosticCurrent(observation->value, state.generation.load(), *destination,
                                state.diagnostics_ready.load()))
            return observation;
          // Retired/configuration/session-stale observations release their own
          // charge without a POST, retry, coalescing or ordinary acknowledgement.
          state.diagnostic_drops.fetch_add(1);
        }
        return std::nullopt;
      };
      std::optional<DiagnosticLease> diagnostic;
      if (prefer_diagnostic)
        if (auto next = take_diagnostic())
          diagnostic.emplace(std::move(*next));
      std::optional<Upload> upload;
      if (!diagnostic && OrdinarySessionOpen(state.records.Generation()))
        upload = delivery.Next(state.records, Clock::now());
      if (!diagnostic && !upload)
        if (auto next = take_diagnostic())
          diagnostic.emplace(std::move(*next));
      if (!diagnostic && !upload) {
        state.ready.wait_for(lock, std::chrono::seconds(1));
        continue;
      }
      // Alternate when both lanes have work. Neither a diagnostic burst nor a
      // busy ordinary stream can starve the other, and diagnostics never wait
      // for an unfinished producer while holding the uploader.
      prefer_diagnostic   = !diagnostic.has_value();
      const auto config   = delivery.Config();
      const auto revision = delivery.Revision();
      lock.unlock();

      const auto label   = fmt::format("{} ({})", name, diagnostic ? "diagnostic" : upload->Type());
      bool       success = false;
      try {
        if (!http || configured != revision) {
          http       = NewTargetSession(config);
          configured = revision;
        }
        auto& headers = http->GetHeader();
        if (upload && upload->first_sync)
          headers.insert_or_assign("X-PRIME-SYNC", "2");
        else
          headers.erase("X-PRIME-SYNC");
        struct ClearBody {
          cpr::Session& session;
          ~ClearBody()
          {
            try {
              session.SetBody(cpr::Body{});
            } catch (...) {
            }
          }
        } clear_body{*http};
        size_t count = 1;
        if (diagnostic) {
          http->SetBody(cpr::Body{diagnostic->value.payload->body});
          std::scoped_lock current_lock(state.mutex);
          if (!DiagnosticCurrent(diagnostic->value, state.generation.load(), *destination,
                                 state.diagnostics_ready.load()))
            continue;
        } else {
          const auto records = upload->Records();
          count              = records.size();
          http->SetBody(cpr::Body{records.dump(-1, ' ', false, json::error_handler_t::replace)});
          if (!OrdinarySessionOpen(upload->generation))
            continue;
        }
        // Authorizing an attempt cannot retract HTTP already issued if its
        // generation/revision changes immediately afterward.
        const auto response = http->Post();
        success             = response.status_code >= 200 && response.status_code < 300;
        if (success)
          logging::Debug("upload", label, "sent {} records in {:.0f} ms", count, response.elapsed * 1'000);
        else
          logging::Error("upload", label, "failed ({}): {}; {}", response.status_code, response.error.message,
                         diagnostic ? "best-effort observation dropped" : "kept for retry");
      } catch (const std::exception& e) {
        spdlog::error("[Sync] upload {} failed: {}; {}", label, e.what(),
                      diagnostic ? "best-effort observation dropped" : "kept for retry");
      }
      lock.lock();
      if (upload && OrdinarySessionOpen(upload->generation))
        delivery.Complete(std::move(*upload), success, state.records.Generation(), Clock::now());
      if (diagnostic && !success)
        state.diagnostic_drops.fetch_add(1);
    }
  } catch (...) {
    // A dead destination is an explicit tombstone, never a process-terminating
    // detached-thread exception or a silent replacement worker on each update.
    try {
      std::scoped_lock lock(state.mutex);
      if (const auto destination = target.lock()) {
        destination->unavailable = true;
        destination->delivery.Retire();
        destination->diagnostics.Close();
        while (destination->diagnostics.TryPop()) {}
      }
    } catch (...) {
    }
    try {
      spdlog::error("[Sync] destination '{}' uploader stopped; restart the game to resume it.", name);
    } catch (...) {
    }
  }
}
} // namespace data_sync
