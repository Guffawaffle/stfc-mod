/**
 * @file targeted_diagnostics.cc
 * @brief Bounded asynchronous capture implementation for targeted diagnostics.
 */
#include "targeted_diagnostics.h"

#include "bounded_mpsc_queue.h"
#include "diagnostics_file_policy.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <semaphore>
#include <set>
#include <thread>
#include <unordered_map>

namespace targeted_diagnostics
{
namespace
{
  using json = nlohmann::ordered_json;

  constexpr uint32_t                  kEnvelopeVersion = 2;
  constexpr std::chrono::milliseconds kWriterBurstIdle{2};

  int64_t utc_now_ms()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  int64_t monotonic_now_ms()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  std::string concern_filename(const std::string_view id)
  { return "community_patch_target_" + std::string(id) + ".jsonl"; }

  const char* registry_error_name(const RegistryValidationError error)
  {
    switch (error) {
      case RegistryValidationError::None:
        return "none";
      case RegistryValidationError::InvalidSpec:
        return "invalid-spec";
      case RegistryValidationError::DuplicateId:
        return "duplicate-id";
      case RegistryValidationError::ExpiredTemporaryConcern:
        return "expired-temporary-concern";
    }
    return "unknown";
  }

  bool try_reserve_bytes(std::atomic_size_t& value, const size_t amount, const size_t limit)
  {
    auto current = value.load(std::memory_order_relaxed);
    for (;;) {
      if (amount > limit || current > limit - amount) {
        return false;
      }
      if (value.compare_exchange_weak(current, current + amount, std::memory_order_acq_rel,
                                      std::memory_order_relaxed)) {
        return true;
      }
    }
  }

  std::mutex                            g_capture_mutex;
  std::atomic<std::shared_ptr<Capture>> g_capture;
} // namespace

struct Capture::State {
  enum class WarningCategory : uint8_t {
    Path,
    Serialization,
    RecordSize,
    Open,
    Write,
    Close,
    Count,
  };

  struct RuntimeConcern {
    Concern*                                                      concern = nullptr;
    std::filesystem::path                                         path;
    std::atomic_size_t                                            queued_bytes{0};
    std::array<bool, static_cast<size_t>(WarningCategory::Count)> warnings{};
    std::FILE*                                                    file         = nullptr;
    std::uintmax_t                                                current_size = 0;
  };

  struct PendingRecord {
    RuntimeConcern* runtime                = nullptr;
    Concern*        concern                = nullptr;
    uint64_t        sequence               = 0;
    int64_t         timestamp_utc_ms       = 0;
    int64_t         monotonic_timestamp_ms = 0;
    InlinePayload   payload;

    PendingRecord()                                    = default;
    PendingRecord(PendingRecord&&) noexcept            = default;
    PendingRecord& operator=(PendingRecord&&) noexcept = default;
  };

  explicit State(std::span<Concern* const> registry_value, CaptureOptions options_value)
      : registry(registry_value.begin(), registry_value.end())
      , options(std::move(options_value))
      , queue(options.limits.global_queue_bytes)
  {
    runtimes.reserve(registry.size());
    runtime_lookup.reserve(registry.size());
    for (auto* concern : registry) {
      auto runtime     = std::make_unique<RuntimeConcern>();
      runtime->concern = concern;
      runtime_lookup.emplace(concern, runtime.get());
      runtimes.push_back(std::move(runtime));
    }
  }

  ~State()
  { close_all_files(); }

  RuntimeConcern* runtime_for(Concern* concern) const
  {
    const auto it = runtime_lookup.find(concern);
    return it == runtime_lookup.end() ? nullptr : it->second;
  }

  void warn_once(RuntimeConcern& runtime, const WarningCategory category, const std::string_view message)
  {
    auto& warned = runtime.warnings[static_cast<size_t>(category)];
    if (warned) {
      return;
    }
    warned = true;
    spdlog::warn("[TargetDiagnostic] concern={} {}", runtime.concern->spec().id, message);
  }

  json build_record(const PendingRecord& pending, const bool rotated) const
  {
    json fields = json::object();
    pending.payload.SerializeFields(fields);
    const auto stats = pending.concern->stats();

    return json{{"envelope_version", kEnvelopeVersion},
                {"schema_version", pending.payload.SchemaVersion()},
                {"concern_id", pending.concern->spec().id},
                {"sequence", pending.sequence},
                {"timestamp_utc_ms", pending.timestamp_utc_ms},
                {"monotonic_timestamp_ms", pending.monotonic_timestamp_ms},
                {"event_type", pending.payload.EventType()},
                {"build",
                 {{"version", options.identity.downstream_version},
                  {"source", options.identity.source_state_id},
                  {"class", options.identity.build_class}}},
                {"capture",
                 {{"accepted", stats.accepted},
                  {"dropped_queue_full", stats.dropped_queue_full},
                  {"dropped_shutdown", stats.dropped_shutdown},
                  {"rejected_record_size", stats.rejected_record_size},
                  {"writer_failures", stats.writer_failures},
                  {"rotations", stats.rotations},
                  {"shutdown_overruns", stats.shutdown_overruns},
                  {"file_rotated", rotated}}},
                {"fields", std::move(fields)}};
  }

  void close_runtime_file(RuntimeConcern& runtime)
  {
    if (!runtime.file) {
      return;
    }
    if (std::fclose(runtime.file) != 0) {
      runtime.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Close, "failed to close targeted diagnostics file");
    }
    runtime.file = nullptr;
  }

  void close_all_files()
  {
    for (auto& runtime : runtimes) {
      close_runtime_file(*runtime);
    }
  }

  bool open_for_record(RuntimeConcern& runtime, const size_t payload_size, bool& rotated)
  {
    if (runtime.file && runtime.current_size < options.limits.max_file_bytes
        && payload_size <= options.limits.max_file_bytes - runtime.current_size) {
      return true;
    }

    close_runtime_file(runtime);
    auto prepare = PrepareDiagnosticsFileForAppend(runtime.path, options.limits.max_file_bytes,
                                                   options.limits.total_files, payload_size);
    if (prepare.warning.has_value()) {
      warn_once(runtime, WarningCategory::Path, *prepare.warning);
    }
    if (!prepare.append_allowed) {
      runtime.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    rotated = prepare.rotated;
    std::error_code error;
    runtime.current_size =
        std::filesystem::exists(runtime.path, error) ? std::filesystem::file_size(runtime.path, error) : 0;
    if (error) {
      runtime.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Path, "failed to determine targeted diagnostics file size");
      return false;
    }

    const auto path_text = runtime.path.string();
    runtime.file         = std::fopen(path_text.c_str(), "ab");
    if (!runtime.file) {
      runtime.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Open, "failed to open targeted diagnostics file");
      return false;
    }
    return true;
  }

  bool write_record(PendingRecord& pending)
  {
    auto& runtime = *pending.runtime;

    std::string payload;
    try {
      payload = build_record(pending, false).dump();
      payload.push_back('\n');
    } catch (const std::exception& exception) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Serialization, std::string("serialization failed: ") + exception.what());
      return false;
    } catch (...) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Serialization, "serialization failed: unknown error");
      return false;
    }

    if (payload.size() > options.limits.max_record_bytes) {
      pending.concern->rejected_record_size_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::RecordSize, "serialized record exceeded the maximum record size");
      return false;
    }

    if (options.before_record_write) {
      try {
        options.before_record_write();
      } catch (...) {
        pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
        warn_once(runtime, WarningCategory::Write, "before-write callback failed");
        return false;
      }
    }

    bool rotated = false;
    if (!open_for_record(runtime, payload.size(), rotated)) {
      return false;
    }
    if (rotated) {
      pending.concern->rotations_.fetch_add(1, std::memory_order_relaxed);
      payload = build_record(pending, true).dump();
      payload.push_back('\n');
    }

    const auto written = std::fwrite(payload.data(), sizeof(char), payload.size(), runtime.file);
    if (written != payload.size() || std::ferror(runtime.file) != 0) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(runtime, WarningCategory::Write, "partial write failure");
      close_runtime_file(runtime);
      return false;
    }
    runtime.current_size += written;
    return true;
  }

  void release_admission(PendingRecord& pending)
  {
    pending.runtime->queued_bytes.fetch_sub(BoundedMpscQueue<PendingRecord>::slot_bytes(), std::memory_order_acq_rel);
    queued_count.fetch_sub(1, std::memory_order_acq_rel);
  }

  void drop_queued()
  {
    PendingRecord pending;
    while (queue.try_dequeue(pending)) {
      release_admission(pending);
      pending.concern->dropped_shutdown_.fetch_add(1, std::memory_order_relaxed);
    }
    while (ready_records.try_acquire()) {}
  }

  void process_record(PendingRecord& pending)
  {
    in_flight.fetch_add(1, std::memory_order_acq_rel);
    release_admission(pending);
    pending.sequence = pending.concern->sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    write_record(pending);
    if (shutdown_requested.load(std::memory_order_acquire) && std::chrono::steady_clock::now() > shutdown_deadline) {
      pending.concern->shutdown_overruns_.fetch_add(1, std::memory_order_relaxed);
    }
    in_flight.fetch_sub(1, std::memory_order_acq_rel);
    idle_condition.notify_all();
  }

  void writer_main()
  {
    for (;;) {
      ready_records.acquire();

      for (;;) {
        if (shutdown_requested.load(std::memory_order_acquire)
            && std::chrono::steady_clock::now() >= shutdown_deadline) {
          drop_queued();
        }

        PendingRecord pending;
        while (queue.try_dequeue(pending)) {
          process_record(pending);
          if (shutdown_requested.load(std::memory_order_acquire)
              && std::chrono::steady_clock::now() >= shutdown_deadline) {
            drop_queued();
            break;
          }
        }

        while (ready_records.try_acquire()) {}
        if (!ready_records.try_acquire_for(kWriterBurstIdle)) {
          break;
        }
      }
      close_all_files();

      if (shutdown_requested.load(std::memory_order_acquire) && queued_count.load(std::memory_order_acquire) == 0
          && active_submitters.load(std::memory_order_acquire) == 0) {
        break;
      }
    }
    idle_condition.notify_all();
  }

  void submitter_finished()
  {
    if (active_submitters.fetch_sub(1, std::memory_order_acq_rel) == 1
        && shutdown_requested.load(std::memory_order_acquire)) {
      ready_records.release();
    }
  }

  std::vector<Concern*>                         registry;
  CaptureOptions                                options;
  BoundedMpscQueue<PendingRecord>               queue;
  std::vector<std::unique_ptr<RuntimeConcern>>  runtimes;
  std::unordered_map<Concern*, RuntimeConcern*> runtime_lookup;
  std::counting_semaphore<>                     ready_records{0};
  std::atomic_size_t                            queued_count{0};
  std::atomic_size_t                            in_flight{0};
  std::atomic_size_t                            active_submitters{0};
  std::atomic_bool                              accepting{true};
  std::atomic_bool                              shutdown_started{false};
  std::atomic_bool                              shutdown_requested{false};
  std::chrono::steady_clock::time_point         shutdown_deadline{};
  mutable std::mutex                            idle_mutex;
  mutable std::condition_variable               idle_condition;
  std::thread                                   writer;
};

ConcernStats Concern::stats() const
{
  return {.accepted             = accepted_.load(std::memory_order_relaxed),
          .dropped_queue_full   = dropped_queue_full_.load(std::memory_order_relaxed),
          .dropped_shutdown     = dropped_shutdown_.load(std::memory_order_relaxed),
          .rejected_record_size = rejected_record_size_.load(std::memory_order_relaxed),
          .writer_failures      = writer_failures_.load(std::memory_order_relaxed),
          .rotations            = rotations_.load(std::memory_order_relaxed),
          .shutdown_overruns    = shutdown_overruns_.load(std::memory_order_relaxed)};
}

Capture::Capture(const std::span<Concern* const> registry, const std::span<const std::string> enabled_ids,
                 CaptureOptions options)
    : state_(std::make_shared<State>(registry, std::move(options)))
{
  std::vector<const ConcernSpec*> specs;
  specs.reserve(registry.size());
  for (auto* concern : registry) {
    if (concern) {
      concern->enabled_.store(false, std::memory_order_relaxed);
      specs.push_back(&concern->spec());
    }
  }

  const auto validation = ValidateConcernSpecs(specs, state_->options.current_version, false);
  if (validation != RegistryValidationError::None) {
    state_->accepting.store(false, std::memory_order_release);
    spdlog::error("[TargetDiagnostic] registry rejected reason={}", registry_error_name(validation));
    return;
  }

  std::set<std::string> requested_ids;
  for (const auto& id : enabled_ids) {
    if (!requested_ids.emplace(id).second) {
      continue;
    }

    const auto resolution = Resolve(id);
    if (resolution == ConcernResolution::Unknown) {
      spdlog::warn("[TargetDiagnostic] concern={} status=unknown-or-unsupported", id);
      continue;
    }
    if (resolution == ConcernResolution::Expired) {
      spdlog::warn("[TargetDiagnostic] concern={} status=expired", id);
      continue;
    }

    const auto concern_it = std::ranges::find_if(
        state_->registry, [&id](const auto* concern) { return concern && concern->spec().id == id; });
    if (concern_it == state_->registry.end()) {
      continue;
    }
    auto* runtime = state_->runtime_for(*concern_it);
    if (!runtime) {
      continue;
    }

    const auto filename = concern_filename(id);
    const auto fallback = state_->options.fallback_root / filename;
    const auto target   = ResolveDiagnosticsFileTarget(filename, fallback, state_->options.configured_root);
    runtime->path       = target.path;
    if (target.warning.has_value()) {
      state_->warn_once(*runtime, State::WarningCategory::Path, *target.warning);
    }
    runtime->concern->enabled_.store(true, std::memory_order_release);
    spdlog::info("[TargetDiagnostic] concern={} status=enabled path='{}' issue={} sunset={}.{}.{} queueSlots={}", id,
                 runtime->path.string(), runtime->concern->spec().tracking_issue,
                 runtime->concern->spec().sunset_at.major, runtime->concern->spec().sunset_at.minor,
                 runtime->concern->spec().sunset_at.revision, state_->queue.capacity());
  }

  const auto has_enabled =
      std::ranges::any_of(state_->runtimes, [](const auto& runtime) { return runtime->concern->enabled(); });
  if (has_enabled && state_->options.start_writer) {
    auto state     = state_;
    state_->writer = std::thread([state] { state->writer_main(); });
  }
}

Capture::~Capture()
{ Shutdown(); }

ConcernResolution Capture::Resolve(const std::string_view id) const
{
  for (auto* concern : state_->registry) {
    if (!concern || concern->spec().id != id) {
      continue;
    }
    return IsExpired(concern->spec(), state_->options.current_version) ? ConcernResolution::Expired
                                                                       : ConcernResolution::Active;
  }
  return ConcernResolution::Unknown;
}

SubmitStatus Capture::Submit(Concern& concern, InlinePayload payload)
{
  if (!concern.enabled()) {
    return Resolve(concern.spec().id) == ConcernResolution::Expired ? SubmitStatus::Expired : SubmitStatus::Disabled;
  }

  state_->active_submitters.fetch_add(1, std::memory_order_acq_rel);
  if (!state_->accepting.load(std::memory_order_acquire)) {
    state_->submitter_finished();
    return SubmitStatus::WriterUnavailable;
  }

  auto* runtime = state_->runtime_for(&concern);
  if (!runtime) {
    state_->submitter_finished();
    return SubmitStatus::WriterUnavailable;
  }

  constexpr auto slot_bytes = BoundedMpscQueue<State::PendingRecord>::slot_bytes();
  if (!try_reserve_bytes(runtime->queued_bytes, slot_bytes, state_->options.limits.per_concern_queue_bytes)) {
    concern.dropped_queue_full_.fetch_add(1, std::memory_order_relaxed);
    state_->submitter_finished();
    return SubmitStatus::QueueFull;
  }

  const auto enqueued = state_->queue.try_emplace([&](State::PendingRecord& pending) noexcept {
    pending.runtime                = runtime;
    pending.concern                = &concern;
    pending.timestamp_utc_ms       = utc_now_ms();
    pending.monotonic_timestamp_ms = monotonic_now_ms();
    pending.payload                = std::move(payload);
    concern.accepted_.fetch_add(1, std::memory_order_relaxed);
  });
  if (!enqueued) {
    runtime->queued_bytes.fetch_sub(slot_bytes, std::memory_order_acq_rel);
    concern.dropped_queue_full_.fetch_add(1, std::memory_order_relaxed);
    state_->submitter_finished();
    return SubmitStatus::QueueFull;
  }

  state_->queued_count.fetch_add(1, std::memory_order_release);
  state_->ready_records.release();
  state_->submitter_finished();
  return SubmitStatus::Accepted;
}

void Capture::Shutdown()
{
  if (!state_) {
    return;
  }

  for (auto* concern : state_->registry) {
    if (concern) {
      concern->enabled_.store(false, std::memory_order_release);
    }
  }

  if (state_->shutdown_started.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  state_->accepting.store(false, std::memory_order_release);
  const auto started        = std::chrono::steady_clock::now();
  state_->shutdown_deadline = started + state_->options.limits.shutdown_drain_timeout;
  state_->shutdown_requested.store(true, std::memory_order_release);
  state_->ready_records.release();

  if (!state_->writer.joinable()) {
    state_->drop_queued();
    return;
  }

  state_->writer.join();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
  if (elapsed > state_->options.limits.shutdown_drain_timeout) {
    spdlog::warn("[TargetDiagnostic] shutdown queue-drain target exceeded elapsedMs={} targetMs={} reason=in-flight-io",
                 elapsed.count(), state_->options.limits.shutdown_drain_timeout.count());
  }
}

bool Capture::WaitUntilIdle(const std::chrono::milliseconds timeout) const
{
  std::unique_lock lock(state_->idle_mutex);
  return state_->idle_condition.wait_for(lock, timeout, [this] {
    return state_->queued_count.load(std::memory_order_acquire) == 0
           && state_->in_flight.load(std::memory_order_acquire) == 0
           && state_->active_submitters.load(std::memory_order_acquire) == 0;
  });
}

void Initialize(const std::span<Concern* const> registry, const std::span<const std::string> enabled_ids,
                const CaptureOptions& options)
{
  std::lock_guard lock(g_capture_mutex);
  if (g_capture.load(std::memory_order_acquire)) {
    return;
  }

  g_capture.store(std::make_shared<Capture>(registry, enabled_ids, options), std::memory_order_release);
}

void Shutdown()
{
  std::shared_ptr<Capture> capture;
  {
    std::lock_guard lock(g_capture_mutex);
    capture = g_capture.exchange(nullptr, std::memory_order_acq_rel);
  }
  if (capture) {
    capture->Shutdown();
  }
}

std::shared_ptr<Capture> CurrentCapture()
{ return g_capture.load(std::memory_order_acquire); }
} // namespace targeted_diagnostics
