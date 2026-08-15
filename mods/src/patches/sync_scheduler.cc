/**
 * @file sync_scheduler.cc
 * @brief Main sync queue scheduler and consumer thread.
 */
#include "patches/sync_scheduler.h"

#include "errormsg.h"
#include "patches/async_work_queue.h"
#include "patches/sync_transport.h"

#include <spdlog/spdlog.h>

#if _WIN32
#include <winrt/Windows.Foundation.h>
#endif

#include <chrono>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

#if _WIN32
struct WinRtApartmentGuard {
  WinRtApartmentGuard()
  { winrt::init_apartment(); }
  ~WinRtApartmentGuard()
  { winrt::uninit_apartment(); }
};
#endif

namespace
{
using SyncQueueItem = std::tuple<SyncConfig::Type, std::string, bool>;

constexpr size_t              kSyncDataQueueMaxDepth = 256;
AsyncWorkQueue<SyncQueueItem> s_sync_data_queue(kSyncDataQueueMaxDepth);
std::once_flag                s_sync_worker_once;
std::thread                   s_sync_worker_thread;
constexpr auto                kSyncWorkerSlowJoinThreshold = std::chrono::seconds(5);

void log_worker_join_time(const std::string_view worker_name, const std::chrono::steady_clock::duration elapsed)
{
  if (elapsed > kSyncWorkerSlowJoinThreshold) {
    spdlog::warn("[SyncQueue] {} join waited {} ms during shutdown", worker_name,
                 std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
  }
}
} // namespace

void queue_data(SyncConfig::Type type, const std::string& data, bool is_first_sync)
{
  if (!s_sync_data_queue.enqueue({type, data, is_first_sync})) {
    const auto diagnostics = s_sync_data_queue.diagnostics();
    http::sync_log_warn("QUEUE", to_string(type),
                        diagnostics.shutdown_requested ? "Dropping data because sync scheduler shutdown is in progress"
                                                       : "Dropping data because sync scheduler queue is full");
    return;
  }

  http::sync_log_trace("QUEUE", to_string(type), "Added data to sync queue");
}

void queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync)
{
  if (!s_sync_data_queue.enqueue({type, data.dump(), is_first_sync})) {
    const auto diagnostics = s_sync_data_queue.diagnostics();
    http::sync_log_warn("QUEUE", to_string(type),
                        diagnostics.shutdown_requested ? "Dropping data because sync scheduler shutdown is in progress"
                                                       : "Dropping data because sync scheduler queue is full");
    return;
  }

  http::sync_log_trace("QUEUE", to_string(type), "Added " + std::to_string(data.size()) + " entries to sync queue");
}

static void ship_sync_data()
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  try {
    SyncQueueItem sync_data;
    while (s_sync_data_queue.wait_pop(sync_data)) {

      try {
        auto& [type, data, is_first_sync] = sync_data;
        http::send_data(type, data, is_first_sync);
      } catch (const std::runtime_error& exception) {
        ErrorMsg::SyncRuntime("ship", exception);
      } catch (const std::exception& exception) {
        ErrorMsg::SyncMsg("ship", exception.what());
      } catch (const std::wstring& message) {
        ErrorMsg::SyncMsg("ship", message);
#if _WIN32
      } catch (winrt::hresult_error const& exception) {
        ErrorMsg::SyncWinRT("ship", exception);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("ship", "Unknown error during sending of sync data");
      }
    }
  } catch (const std::exception& exception) {
    spdlog::critical("ship_sync_data thread terminated: {}", exception.what());
  } catch (...) {
    spdlog::critical("ship_sync_data thread terminated: unknown exception");
  }

  spdlog::debug("[SyncQueue] worker stopped");
}

void StartSyncSchedulerWorker()
{
  std::call_once(s_sync_worker_once, [] { s_sync_worker_thread = std::thread(ship_sync_data); });
}

void ShutdownSyncSchedulerWorker()
{
  s_sync_data_queue.request_shutdown();

  if (!s_sync_worker_thread.joinable()) {
    return;
  }

  const auto join_started_at = std::chrono::steady_clock::now();
  s_sync_worker_thread.join();
  log_worker_join_time("sync scheduler worker", std::chrono::steady_clock::now() - join_started_at);
}
