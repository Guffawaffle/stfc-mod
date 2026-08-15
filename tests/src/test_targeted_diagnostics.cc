#include <doctest/doctest.h>

#include "bounded_mpsc_queue.h"
#include "diagnostics_file_policy.h"
#include "patches/fleet_notification_diagnostics.h"
#include "patches/runtime_impact_diagnostics.h"
#include "patches/runtime_impact_monitor.h"
#include "targeted_diagnostics.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
struct TestEvent {
  int value = 0;
};

constexpr targeted_diagnostics::ConcernSpec kActiveSpec{
    .id                    = "test-concern",
    .owner                 = "tests",
    .tracking_issue        = "#255",
    .status                = targeted_diagnostics::ConcernStatus::Temporary,
    .introduced_in         = {1, 0, 0},
    .sunset_at             = {2, 0, 0},
    .remove_or_revise_when = "The test completes.",
    .promotion_criteria    = "Not applicable outside the fixture.",
};

constexpr targeted_diagnostics::ConcernSpec kSecondSpec{
    .id                    = "second-concern",
    .owner                 = "tests",
    .tracking_issue        = "#255",
    .status                = targeted_diagnostics::ConcernStatus::Temporary,
    .introduced_in         = {1, 0, 0},
    .sunset_at             = {2, 0, 0},
    .remove_or_revise_when = "The test completes.",
    .promotion_criteria    = "Not applicable outside the fixture.",
};

class ScopedTempDir
{
public:
  ScopedTempDir()
  {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_          = std::filesystem::temp_directory_path() / ("stfc-targeted-diagnostics-" + std::to_string(now));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir()
  { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const
  { return path_; }

private:
  std::filesystem::path path_;
};

std::vector<nlohmann::json> read_jsonl(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::in | std::ios::binary);
  REQUIRE(file.is_open());

  std::vector<nlohmann::json> rows;
  std::string                 line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      rows.push_back(nlohmann::json::parse(line));
    }
  }
  return rows;
}

targeted_diagnostics::CaptureOptions options_for(const ScopedTempDir& temp_dir)
{
  return {.fallback_root   = temp_dir.path() / "fallback",
          .configured_root = (temp_dir.path() / "configured").string(),
          .current_version = {1, 1, 0},
          .identity        = {.downstream_version = "v-test", .source_state_id = "git:test", .build_class = "test"}};
}
} // namespace

namespace targeted_diagnostics
{
template <> struct EventTraits<TestEvent> {
  static constexpr std::string_view event_type       = "test-event";
  static constexpr uint32_t         schema_version   = 7;
  static constexpr bool             owns_queued_data = true;
  static void                       SerializeFields(const TestEvent& event, nlohmann::ordered_json& fields)
  { fields = {{"value", event.value}}; }
};
} // namespace targeted_diagnostics

TEST_SUITE("targeted_diagnostics")
{
  TEST_CASE("production file retention defaults stay within the reviewed global ceiling")
  {
    const targeted_diagnostics::CaptureLimits limits;
    CHECK(limits.max_file_bytes == 1024 * 1024);
    CHECK(limits.total_files == 2);
  }

  TEST_CASE("MPSC publication tolerates producers completing out of reservation order")
  {
    BoundedMpscQueue<int>   queue(BoundedMpscQueue<int>::slot_bytes() * 4);
    std::mutex              gate_mutex;
    std::condition_variable gate_condition;
    bool                    first_reserved = false;
    bool                    release_first  = false;
    std::atomic_bool        first_enqueued{false};
    std::atomic_bool        second_enqueued{false};

    std::thread first([&] {
      first_enqueued.store(queue.try_emplace([&](int& destination) noexcept {
        std::unique_lock lock(gate_mutex);
        first_reserved = true;
        gate_condition.notify_all();
        gate_condition.wait(lock, [&] { return release_first; });
        destination = 1;
      }));
    });
    {
      std::unique_lock lock(gate_mutex);
      REQUIRE(gate_condition.wait_for(lock, std::chrono::seconds(2), [&] { return first_reserved; }));
    }

    std::thread second([&] { second_enqueued.store(queue.try_enqueue(2)); });
    second.join();
    int value = 0;
    CHECK_FALSE(queue.try_dequeue(value));

    {
      std::lock_guard lock(gate_mutex);
      release_first = true;
    }
    gate_condition.notify_all();
    first.join();

    CHECK(first_enqueued.load());
    CHECK(second_enqueued.load());
    REQUIRE(queue.try_dequeue(value));
    CHECK(value == 1);
    REQUIRE(queue.try_dequeue(value));
    CHECK(value == 2);
  }

  TEST_CASE("validates lifecycle metadata, duplicate IDs, and sunset boundaries")
  {
    static constexpr targeted_diagnostics::ConcernSpec invalid{
        .id                    = "../bad",
        .owner                 = "tests",
        .tracking_issue        = "#255",
        .status                = targeted_diagnostics::ConcernStatus::Temporary,
        .introduced_in         = {1, 0, 0},
        .sunset_at             = {2, 0, 0},
        .remove_or_revise_when = "Done.",
        .promotion_criteria    = "Measured consumer.",
    };
    constexpr std::array valid_specs{&kActiveSpec, &kSecondSpec};
    constexpr std::array duplicate_specs{&kActiveSpec, &kActiveSpec};
    constexpr std::array invalid_specs{&invalid};

    CHECK(targeted_diagnostics::ValidateConcernSpecs(valid_specs, {1, 1, 0}, true)
          == targeted_diagnostics::RegistryValidationError::None);
    CHECK(targeted_diagnostics::ValidateConcernSpecs(duplicate_specs, {1, 1, 0}, true)
          == targeted_diagnostics::RegistryValidationError::DuplicateId);
    CHECK(targeted_diagnostics::ValidateConcernSpecs(invalid_specs, {1, 1, 0}, true)
          == targeted_diagnostics::RegistryValidationError::InvalidSpec);
    CHECK(targeted_diagnostics::ValidateConcernSpecs(valid_specs, {2, 0, 0}, true)
          == targeted_diagnostics::RegistryValidationError::ExpiredTemporaryConcern);
  }

  TEST_CASE("resolves active, expired, and removed IDs without tombstones")
  {
    ScopedTempDir                  temp_dir;
    targeted_diagnostics::Concern  concern{kActiveSpec};
    std::array                     registry{&concern};
    const std::vector<std::string> enabled{"removed-concern", "test-concern"};
    auto                           options = options_for(temp_dir);
    targeted_diagnostics::Capture  active_capture(registry, enabled, options);

    CHECK(active_capture.Resolve("test-concern") == targeted_diagnostics::ConcernResolution::Active);
    CHECK(active_capture.Resolve("removed-concern") == targeted_diagnostics::ConcernResolution::Unknown);
    CHECK(concern.enabled());
    active_capture.Shutdown();

    targeted_diagnostics::Concern expired_concern{kActiveSpec};
    std::array                    expired_registry{&expired_concern};
    options.current_version = {2, 0, 0};
    targeted_diagnostics::Capture expired_capture(expired_registry, std::vector<std::string>{"test-concern"}, options);
    CHECK(expired_capture.Resolve("test-concern") == targeted_diagnostics::ConcernResolution::Expired);
    CHECK_FALSE(expired_concern.enabled());
  }

  TEST_CASE("writes typed owning records to an isolated JSONL file")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options = options_for(temp_dir);
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    CHECK(capture.TryWrite(concern, TestEvent{42}) == targeted_diagnostics::SubmitStatus::Accepted);
    CHECK(capture.WaitUntilIdle(std::chrono::seconds(2)));
    capture.Shutdown();

    const auto path = temp_dir.path() / "configured" / "community_patch_target_test-concern.jsonl";
    const auto rows = read_jsonl(path);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0]["envelope_version"] == 2);
    CHECK(rows[0]["schema_version"] == 7);
    CHECK(rows[0]["concern_id"] == "test-concern");
    CHECK(rows[0]["sequence"] == 1);
    CHECK(rows[0]["event_type"] == "test-event");
    CHECK(rows[0]["build"]["source"] == "git:test");
    CHECK(rows[0]["fields"]["value"] == 42);
  }

  TEST_CASE("keeps disabled event construction behind the canonical marker")
  {
    targeted_diagnostics::Concern concern{kActiveSpec};
    int                           constructed = 0;
    const auto                    make_event  = [&] {
      ++constructed;
      return TestEvent{1};
    };

    CHECK(TARGET_DIAGNOSTIC_WRITE(concern, make_event()) == targeted_diagnostics::SubmitStatus::Disabled);
    CHECK(constructed == 0);
  }

  TEST_CASE("enforces queue bytes without blocking when the writer cannot drain")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options  = options_for(temp_dir);
    options.start_writer                   = false;
    options.limits.global_queue_bytes      = 1'024;
    options.limits.per_concern_queue_bytes = 1'024;
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    bool saw_queue_full = false;
    for (int index = 0; index < 100; ++index) {
      const auto status = capture.TryWrite(concern, TestEvent{index});
      if (status == targeted_diagnostics::SubmitStatus::QueueFull) {
        saw_queue_full = true;
        break;
      }
    }
    CHECK(saw_queue_full);
    CHECK(concern.stats().dropped_queue_full > 0);
    capture.Shutdown();
    CHECK(concern.stats().dropped_shutdown > 0);
    CHECK(concern.stats().dropped_queue_full == 1);
  }

  TEST_CASE("rotates concern files and marks the first record in the new generation")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options = options_for(temp_dir);
    options.limits.max_file_bytes         = 1'200;
    options.limits.total_files            = 2;
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    for (int index = 0; index < 12; ++index) {
      CHECK(capture.TryWrite(concern, TestEvent{index}) == targeted_diagnostics::SubmitStatus::Accepted);
    }
    CHECK(capture.WaitUntilIdle(std::chrono::seconds(2)));
    capture.Shutdown();

    const auto path = temp_dir.path() / "configured" / "community_patch_target_test-concern.jsonl";
    CHECK(std::filesystem::exists(DiagnosticsRotatedPath(path, 1)));
    const auto rows = read_jsonl(path);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows.front()["capture"]["file_rotated"] == true);
    CHECK(concern.stats().rotations > 0);
  }

  TEST_CASE("keeps concurrent producer accounting bounded without waiting for queue space")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options  = options_for(temp_dir);
    options.start_writer                   = false;
    options.limits.global_queue_bytes      = 2 * 1024 * 1024;
    options.limits.per_concern_queue_bytes = 2 * 1024 * 1024;
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    constexpr int            kThreadCount = 4;
    constexpr int            kWrites      = 100;
    std::vector<std::thread> producers;
    producers.reserve(kThreadCount);
    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
      producers.emplace_back([&capture, &concern, thread_index] {
        for (int index = 0; index < kWrites; ++index) {
          (void)capture.TryWrite(concern, TestEvent{thread_index * kWrites + index});
        }
      });
    }
    for (auto& producer : producers) {
      producer.join();
    }

    const auto stats = concern.stats();
    CHECK(stats.accepted == kThreadCount * kWrites);
    CHECK(stats.dropped_queue_full == 0);
    capture.Shutdown();
    CHECK(concern.stats().dropped_shutdown == stats.accepted);
  }

  TEST_CASE("preserves paired evidence and file sequence under producer contention")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options  = options_for(temp_dir);
    options.limits.global_queue_bytes      = 2 * 1024 * 1024;
    options.limits.per_concern_queue_bytes = 2 * 1024 * 1024;
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    constexpr int            kThreadCount = 4;
    constexpr int            kPairs       = 100;
    std::atomic_bool         all_accepted{true};
    std::vector<std::thread> producers;
    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
      producers.emplace_back([&, thread_index] {
        for (int pair_index = 0; pair_index < kPairs; ++pair_index) {
          const auto pair_id = thread_index * kPairs + pair_index;
          if (capture.TryWrite(concern, TestEvent{pair_id * 2}) != targeted_diagnostics::SubmitStatus::Accepted) {
            all_accepted.store(false);
          }
          if (capture.TryWrite(concern, TestEvent{pair_id * 2 + 1}) != targeted_diagnostics::SubmitStatus::Accepted) {
            all_accepted.store(false);
          }
        }
      });
    }
    for (auto& producer : producers) {
      producer.join();
    }
    REQUIRE(all_accepted.load());
    REQUIRE(capture.WaitUntilIdle(std::chrono::seconds(5)));
    capture.Shutdown();

    const auto rows = read_jsonl(temp_dir.path() / "configured" / "community_patch_target_test-concern.jsonl");
    REQUIRE(rows.size() == kThreadCount * kPairs * 2);
    std::vector<int> occurrences(rows.size(), 0);
    for (size_t index = 0; index < rows.size(); ++index) {
      CHECK(rows[index]["sequence"] == index + 1);
      const auto value = rows[index]["fields"]["value"].get<size_t>();
      REQUIRE(value < occurrences.size());
      ++occurrences[value];
    }
    for (const auto count : occurrences) {
      CHECK(count == 1);
    }
  }

  TEST_CASE("aggregates complete fleet diagnostic producer overhead")
  {
    ScopedTempDir temp_dir;
    auto&         concern = fleet_notification_diagnostics::Concern();
    std::array    registry{&concern};
    auto          options   = options_for(temp_dir);
    options.current_version = {2, 1, 0};
    const std::vector<std::string> enabled{"fleet-notification-scan"};
    targeted_diagnostics::Initialize(registry, enabled, options);
    auto capture = targeted_diagnostics::CurrentCapture();
    REQUIRE(capture);

    fleet_notification_diagnostics::Reset();
    fleet_notification_diagnostics::ScanWasRequested(7, 1'000);
    const auto scan_id = fleet_notification_diagnostics::BeginScan(1'000);
    REQUIRE(scan_id != 0);
    fleet_notification_diagnostics::CompleteScan(scan_id, 1'001, 100, 2, 1, {}, nullptr);
    fleet_notification_diagnostics::CompleteTick(scan_id, 11'000, 180, 100);
    fleet_notification_diagnostics::EndScanSession(fleet_notification_diagnostics::EndReason::Settled, 11'001);
    REQUIRE(capture->WaitUntilIdle(std::chrono::seconds(2)));
    targeted_diagnostics::Shutdown();

    const auto rows =
        read_jsonl(temp_dir.path() / "configured" / "community_patch_target_fleet-notification-scan.jsonl");
    const auto summary =
        std::ranges::find_if(rows, [](const auto& row) { return row["event_type"] == "scan-summary"; });
    REQUIRE(summary != rows.end());
    CHECK((*summary)["schema_version"] == 2);
    CHECK((*summary)["fields"]["producer_elapsed_total_us"] == 180);
    CHECK((*summary)["fields"]["producer_elapsed_max_us"] == 180);
    CHECK((*summary)["fields"]["diagnostic_overhead_total_us"] == 80);
    CHECK((*summary)["fields"]["diagnostic_overhead_max_us"] == 80);
    fleet_notification_diagnostics::Reset();
  }

  TEST_CASE("writes runtime impact records only to the registered concern file")
  {
    ScopedTempDir temp_dir;
    auto&         concern = runtime_impact_diagnostics::Concern();
    std::array    registry{&concern};
    auto          options   = options_for(temp_dir);
    options.current_version = {2, 1, 0};
    const std::vector<std::string> enabled{"runtime-impact"};
    targeted_diagnostics::Initialize(registry, enabled, options);
    auto capture = targeted_diagnostics::CurrentCapture();
    REQUIRE(capture);

    runtime_impact_diagnostics::RecordProbeWindow({
        .probe       = static_cast<uint8_t>(RuntimeImpactProbe::HotkeySpaceAction),
        .window_ms   = 5'000,
        .samples     = 3,
        .total_ns    = 900'000,
        .max_ns      = 500'000,
        .over_250us  = 2,
        .over_1000us = 0,
    });
    runtime_impact_diagnostics::RecordSpaceActionTiming({
        .outcome              = runtime_impact_diagnostics::CopyOutcome("engage-prescan"),
        .duration_us          = 1'200,
        .context_us           = 150,
        .outcome_execution_us = 900,
        .input_flags          = 1 | (1 << 4),
        .context_flags        = 1 << 3,
        .handled              = true,
        .slow                 = true,
    });
    REQUIRE(capture->WaitUntilIdle(std::chrono::seconds(2)));
    targeted_diagnostics::Shutdown();

    const auto rows = read_jsonl(temp_dir.path() / "configured" / "community_patch_target_runtime-impact.jsonl");
    REQUIRE(rows.size() == 2);
    CHECK(rows[0]["event_type"] == "probe-window");
    CHECK(rows[0]["fields"]["probe"] == "hotkey.space_action");
    CHECK(rows[0]["fields"]["average_us"] == 300.0);
    CHECK(rows[1]["event_type"] == "space-action-timing");
    CHECK(rows[1]["fields"]["outcome"] == "engage-prescan");
    CHECK(rows[1]["fields"]["outcome_execution_us"] == 900);
    CHECK(rows[1]["fields"]["inputs"]["physical_primary"] == true);
    CHECK(rows[1]["fields"]["inputs"]["queue"] == true);
    CHECK(rows[1]["fields"]["inputs"]["secondary"] == false);
    CHECK(rows[1]["fields"]["context"]["pre_scan_fallback_used"] == true);
    CHECK(rows[1]["fields"]["context"]["navigation_visible"] == false);
  }

  TEST_CASE("isolates two concerns into deterministic files")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern first{kActiveSpec};
    targeted_diagnostics::Concern second{kSecondSpec};
    std::array                    registry{&first, &second};
    auto                          options = options_for(temp_dir);
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern", "second-concern"},
                                          options);

    CHECK(capture.TryWrite(first, TestEvent{1}) == targeted_diagnostics::SubmitStatus::Accepted);
    CHECK(capture.TryWrite(second, TestEvent{2}) == targeted_diagnostics::SubmitStatus::Accepted);
    CHECK(capture.WaitUntilIdle(std::chrono::seconds(2)));
    capture.Shutdown();

    const auto root        = temp_dir.path() / "configured";
    const auto first_rows  = read_jsonl(root / "community_patch_target_test-concern.jsonl");
    const auto second_rows = read_jsonl(root / "community_patch_target_second-concern.jsonl");
    REQUIRE(first_rows.size() == 1);
    REQUIRE(second_rows.size() == 1);
    CHECK(first_rows[0]["concern_id"] == "test-concern");
    CHECK(first_rows[0]["fields"]["value"] == 1);
    CHECK(second_rows[0]["concern_id"] == "second-concern");
    CHECK(second_rows[0]["fields"]["value"] == 2);
  }

  TEST_CASE("rejects serialized records whose output exceeds the record budget")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options = options_for(temp_dir);
    options.limits.max_record_bytes       = 1;
    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);

    CHECK(capture.TryWrite(concern, TestEvent{1}) == targeted_diagnostics::SubmitStatus::Accepted);
    CHECK(capture.WaitUntilIdle(std::chrono::seconds(2)));
    CHECK(concern.stats().rejected_record_size == 1);
    capture.Shutdown();
    CHECK_FALSE(std::filesystem::exists(temp_dir.path() / "configured" / "community_patch_target_test-concern.jsonl"));
  }

  TEST_CASE("shutdown drops queued records after its deadline but waits for the active writer call")
  {
    ScopedTempDir                 temp_dir;
    targeted_diagnostics::Concern concern{kActiveSpec};
    std::array                    registry{&concern};
    auto                          options = options_for(temp_dir);
    options.limits.shutdown_drain_timeout = std::chrono::milliseconds(40);

    std::mutex              gate_mutex;
    std::condition_variable gate_condition;
    bool                    writer_blocked = false;
    bool                    release_writer = false;
    options.before_record_write            = [&] {
      std::unique_lock lock(gate_mutex);
      writer_blocked = true;
      gate_condition.notify_all();
      gate_condition.wait(lock, [&] { return release_writer; });
    };

    targeted_diagnostics::Capture capture(registry, std::vector<std::string>{"test-concern"}, options);
    CHECK(capture.TryWrite(concern, TestEvent{1}) == targeted_diagnostics::SubmitStatus::Accepted);
    {
      std::unique_lock lock(gate_mutex);
      REQUIRE(gate_condition.wait_for(lock, std::chrono::seconds(2), [&] { return writer_blocked; }));
    }
    for (int index = 2; index <= 10; ++index) {
      CHECK(capture.TryWrite(concern, TestEvent{index}) == targeted_diagnostics::SubmitStatus::Accepted);
    }

    bool        shutdown_returned = false;
    std::thread shutdown_thread([&] {
      capture.Shutdown();
      std::lock_guard lock(gate_mutex);
      shutdown_returned = true;
      gate_condition.notify_all();
    });
    {
      std::unique_lock lock(gate_mutex);
      CHECK_FALSE(gate_condition.wait_for(lock, std::chrono::milliseconds(100), [&] { return shutdown_returned; }));
      release_writer = true;
    }
    gate_condition.notify_all();
    shutdown_thread.join();

    CHECK(shutdown_returned);
    CHECK(concern.stats().shutdown_overruns == 1);
    CHECK(concern.stats().dropped_shutdown == 9);
  }
}
