/**
 * @file sync_scheduler.cc
 * @brief Main sync queue scheduler and consumer thread.
 */
#include "patches/sync_scheduler.h"

#include "errormsg.h"
#include "patches/sync_transport.h"

#include <spdlog/spdlog.h>

#if _WIN32
#include <winrt/Windows.Foundation.h>
#endif

#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>
#include <string>
#include <tuple>

#if _WIN32
struct WinRtApartmentGuard {
  WinRtApartmentGuard() { winrt::init_apartment(); }
  ~WinRtApartmentGuard() { winrt::uninit_apartment(); }
};
#endif

std::mutex sync_data_mtx;
std::condition_variable sync_data_cv;
std::queue<std::tuple<SyncConfig::Type, std::string, bool>> sync_data_queue;

void queue_data(SyncConfig::Type type, const std::string& data, bool is_first_sync)
{
  {
    std::lock_guard lk(sync_data_mtx);
    sync_data_queue.emplace(type, data, is_first_sync);
    http::sync_log_debug("QUEUE", to_string(type), "Added data to sync queue");
  }

  sync_data_cv.notify_all();
}

void queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync)
{
  {
    std::lock_guard lk(sync_data_mtx);
    sync_data_queue.emplace(type, data.dump(), is_first_sync);
    http::sync_log_debug("QUEUE", to_string(type), "Added " + std::to_string(data.size()) + " entries to sync queue");
  }

  sync_data_cv.notify_all();
}

void ship_sync_data()
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  try {
    for (;;) {
      std::tuple<SyncConfig::Type, std::string, bool> sync_data;
      {
        std::unique_lock lock(sync_data_mtx);
        sync_data_cv.wait(lock, [] { return !sync_data_queue.empty(); });
        sync_data = std::move(sync_data_queue.front());
        sync_data_queue.pop();
      }

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
}