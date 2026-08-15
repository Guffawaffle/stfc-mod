/**
 * @file targeted_diagnostics.cc
 * @brief Bounded asynchronous capture implementation for targeted diagnostics.
 */
#include "targeted_diagnostics.h"

#include "diagnostics_file_policy.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

namespace targeted_diagnostics
{
namespace
{
  using json = nlohmann::ordered_json;

  constexpr uint32_t kEnvelopeVersion = 1;

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

  std::mutex                            g_capture_mutex;
  std::atomic<std::shared_ptr<Capture>> g_capture;
} // namespace

struct Capture::State {
  struct RuntimeConcern {
    Concern*              concern = nullptr;
    std::filesystem::path path;
    size_t                queued_bytes   = 0;
    bool                  warning_logged = false;
  };

  struct PendingRecord {
    Concern*                 concern                = nullptr;
    uint64_t                 sequence               = 0;
    int64_t                  timestamp_utc_ms       = 0;
    int64_t                  monotonic_timestamp_ms = 0;
    size_t                   queue_bytes            = 0;
    std::unique_ptr<Payload> payload;
  };

  explicit State(std::span<Concern* const> registry_value, CaptureOptions options_value)
      : registry(registry_value.begin(), registry_value.end())
      , options(std::move(options_value))
  {
    runtimes.reserve(registry.size());
    for (auto* concern : registry) {
      runtimes.push_back({.concern = concern});
    }
  }

  RuntimeConcern* runtime_for(Concern* concern)
  {
    const auto it =
        std::ranges::find_if(runtimes, [concern](const auto& runtime) { return runtime.concern == concern; });
    return it == runtimes.end() ? nullptr : &*it;
  }

  const RuntimeConcern* runtime_for(const Concern* concern) const
  {
    const auto it =
        std::ranges::find_if(runtimes, [concern](const auto& runtime) { return runtime.concern == concern; });
    return it == runtimes.end() ? nullptr : &*it;
  }

  void warn_once(RuntimeConcern& runtime, const std::string_view message)
  {
    if (runtime.warning_logged) {
      return;
    }
    runtime.warning_logged = true;
    spdlog::warn("[TargetDiagnostic] concern={} {}", runtime.concern->spec().id, message);
  }

  json build_record(const PendingRecord& pending, const bool rotated) const
  {
    json fields = json::object();
    pending.payload->SerializeFields(fields);
    const auto stats = pending.concern->stats();

    return json{{"envelope_version", kEnvelopeVersion},
                {"schema_version", pending.payload->SchemaVersion()},
                {"concern_id", pending.concern->spec().id},
                {"sequence", pending.sequence},
                {"timestamp_utc_ms", pending.timestamp_utc_ms},
                {"monotonic_timestamp_ms", pending.monotonic_timestamp_ms},
                {"event_type", pending.payload->EventType()},
                {"build",
                 {{"version", options.identity.downstream_version},
                  {"source", options.identity.source_state_id},
                  {"class", options.identity.build_class}}},
                {"capture",
                 {{"accepted", stats.accepted},
                  {"dropped_lock_busy", stats.dropped_lock_busy},
                  {"dropped_queue_full", stats.dropped_queue_full},
                  {"dropped_shutdown", stats.dropped_shutdown},
                  {"rejected_record_size", stats.rejected_record_size},
                  {"writer_failures", stats.writer_failures},
                  {"rotations", stats.rotations},
                  {"file_rotated", rotated}}},
                {"fields", std::move(fields)}};
  }

  bool write_record(PendingRecord& pending)
  {
    auto* runtime = runtime_for(pending.concern);
    if (!runtime) {
      return false;
    }

    std::string payload;
    try {
      payload = build_record(pending, false).dump();
      payload.push_back('\n');
    } catch (const std::exception& exception) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(*runtime, std::string("serialization failed: ") + exception.what());
      return false;
    } catch (...) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(*runtime, "serialization failed: unknown error");
      return false;
    }

    if (payload.size() > options.limits.max_record_bytes) {
      pending.concern->rejected_record_size_.fetch_add(1, std::memory_order_relaxed);
      warn_once(*runtime, "serialized record exceeded the maximum record size");
      return false;
    }

    auto prepare = PrepareDiagnosticsFileForAppend(runtime->path, options.limits.max_file_bytes,
                                                   options.limits.total_files, payload.size());
    if (prepare.warning.has_value()) {
      warn_once(*runtime, *prepare.warning);
    }
    if (!prepare.append_allowed) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    if (prepare.rotated) {
      pending.concern->rotations_.fetch_add(1, std::memory_order_relaxed);
      payload = build_record(pending, true).dump();
      payload.push_back('\n');
    }

    const auto path_text = runtime->path.string();
    auto*      file      = std::fopen(path_text.c_str(), "ab");
    if (!file) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(*runtime, "failed to open targeted diagnostics file");
      return false;
    }

    const auto written = std::fwrite(payload.data(), sizeof(char), payload.size(), file);
    const auto flushed = std::fflush(file);
    const auto closed  = std::fclose(file);
    if (written != payload.size() || flushed != 0 || closed != 0) {
      pending.concern->writer_failures_.fetch_add(1, std::memory_order_relaxed);
      warn_once(*runtime, "partial write or flush failure");
      return false;
    }

    return true;
  }

  void drop_queued_locked()
  {
    for (auto& pending : queue) {
      if (auto* runtime = runtime_for(pending.concern)) {
        runtime->queued_bytes -= std::min(runtime->queued_bytes, pending.queue_bytes);
      }
      queued_bytes -= std::min(queued_bytes, pending.queue_bytes);
      pending.concern->dropped_shutdown_.fetch_add(1, std::memory_order_relaxed);
    }
    queue.clear();
  }

  void writer_main()
  {
    for (;;) {
      PendingRecord pending;
      {
        std::unique_lock lock(mutex);
        condition.wait(lock, [this] { return shutdown_requested || !queue.empty(); });

        if (shutdown_requested && std::chrono::steady_clock::now() >= shutdown_deadline) {
          drop_queued_locked();
        }
        if (queue.empty()) {
          if (shutdown_requested) {
            break;
          }
          continue;
        }

        pending = std::move(queue.front());
        queue.pop_front();
        queued_bytes -= std::min(queued_bytes, pending.queue_bytes);
        if (auto* runtime = runtime_for(pending.concern)) {
          runtime->queued_bytes -= std::min(runtime->queued_bytes, pending.queue_bytes);
        }
        ++in_flight;
      }

      write_record(pending);

      {
        std::lock_guard lock(mutex);
        --in_flight;
        if (queue.empty() && in_flight == 0) {
          idle_condition.notify_all();
        }
      }
    }

    {
      std::lock_guard lock(mutex);
      writer_exited = true;
      idle_condition.notify_all();
    }
  }

  std::vector<Concern*>                 registry;
  std::vector<RuntimeConcern>           runtimes;
  CaptureOptions                        options;
  mutable std::mutex                    mutex;
  std::condition_variable               condition;
  mutable std::condition_variable       idle_condition;
  std::deque<PendingRecord>             queue;
  size_t                                queued_bytes       = 0;
  size_t                                in_flight          = 0;
  bool                                  accepting          = true;
  bool                                  shutdown_requested = false;
  bool                                  writer_exited      = false;
  std::chrono::steady_clock::time_point shutdown_deadline{};
  std::thread                           writer;
};

ConcernStats Concern::stats() const
{
  return {.accepted             = accepted_.load(std::memory_order_relaxed),
          .dropped_lock_busy    = dropped_lock_busy_.load(std::memory_order_relaxed),
          .dropped_queue_full   = dropped_queue_full_.load(std::memory_order_relaxed),
          .dropped_shutdown     = dropped_shutdown_.load(std::memory_order_relaxed),
          .rejected_record_size = rejected_record_size_.load(std::memory_order_relaxed),
          .writer_failures      = writer_failures_.load(std::memory_order_relaxed),
          .rotations            = rotations_.load(std::memory_order_relaxed)};
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
    state_->accepting = false;
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

    const auto it =
        std::ranges::find_if(state_->runtimes, [&id](const auto& runtime) { return runtime.concern->spec().id == id; });
    if (it == state_->runtimes.end()) {
      continue;
    }

    const auto filename = concern_filename(id);
    const auto fallback = state_->options.fallback_root / filename;
    const auto target   = ResolveDiagnosticsFileTarget(filename, fallback, state_->options.configured_root);
    it->path            = target.path;
    if (target.warning.has_value()) {
      state_->warn_once(*it, *target.warning);
    }
    it->concern->enabled_.store(true, std::memory_order_release);
    spdlog::info("[TargetDiagnostic] concern={} status=enabled path='{}' issue={} sunset={}.{}.{}", id,
                 it->path.string(), it->concern->spec().tracking_issue, it->concern->spec().sunset_at.major,
                 it->concern->spec().sunset_at.minor, it->concern->spec().sunset_at.revision);
  }

  const auto has_enabled =
      std::ranges::any_of(state_->runtimes, [](const auto& runtime) { return runtime.concern->enabled(); });
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

SubmitStatus Capture::Submit(Concern& concern, std::unique_ptr<Payload> payload)
{
  if (!payload) {
    return SubmitStatus::WriterUnavailable;
  }
  if (!concern.enabled()) {
    return Resolve(concern.spec().id) == ConcernResolution::Expired ? SubmitStatus::Expired : SubmitStatus::Disabled;
  }

  const auto queue_bytes = sizeof(State::PendingRecord) + payload->EstimatedQueueBytes();
  if (queue_bytes > state_->options.limits.max_record_bytes) {
    concern.rejected_record_size_.fetch_add(1, std::memory_order_relaxed);
    return SubmitStatus::RecordTooLarge;
  }

  std::unique_lock lock(state_->mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    concern.dropped_lock_busy_.fetch_add(1, std::memory_order_relaxed);
    return SubmitStatus::LockBusy;
  }
  if (!state_->accepting) {
    return SubmitStatus::WriterUnavailable;
  }

  auto* runtime = state_->runtime_for(&concern);
  if (!runtime) {
    return SubmitStatus::WriterUnavailable;
  }
  if (state_->queued_bytes + queue_bytes > state_->options.limits.global_queue_bytes
      || runtime->queued_bytes + queue_bytes > state_->options.limits.per_concern_queue_bytes) {
    concern.dropped_queue_full_.fetch_add(1, std::memory_order_relaxed);
    return SubmitStatus::QueueFull;
  }

  State::PendingRecord pending;
  pending.concern                = &concern;
  pending.sequence               = concern.sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
  pending.timestamp_utc_ms       = utc_now_ms();
  pending.monotonic_timestamp_ms = monotonic_now_ms();
  pending.queue_bytes            = queue_bytes;
  pending.payload                = std::move(payload);
  state_->queue.emplace_back(std::move(pending));
  state_->queued_bytes += queue_bytes;
  runtime->queued_bytes += queue_bytes;
  concern.accepted_.fetch_add(1, std::memory_order_relaxed);
  lock.unlock();
  state_->condition.notify_one();
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

  {
    std::lock_guard lock(state_->mutex);
    if (state_->shutdown_requested) {
      return;
    }
    state_->accepting          = false;
    state_->shutdown_requested = true;
    state_->shutdown_deadline  = std::chrono::steady_clock::now() + state_->options.limits.shutdown_drain_timeout;
    if (!state_->writer.joinable()) {
      state_->drop_queued_locked();
      state_->writer_exited = true;
    }
  }
  state_->condition.notify_all();

  if (state_->writer.joinable()) {
    state_->writer.join();
  }
}

bool Capture::WaitUntilIdle(const std::chrono::milliseconds timeout) const
{
  std::unique_lock lock(state_->mutex);
  return state_->idle_condition.wait_for(lock, timeout,
                                         [this] { return state_->queue.empty() && state_->in_flight == 0; });
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
