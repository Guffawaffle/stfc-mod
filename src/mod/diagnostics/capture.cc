// Owns independent recorder queues and their single, fair background writer. It never calls game APIs.
#include "capture.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace diagnostics
{
namespace
{
  using Clock = std::chrono::steady_clock;
  using Json  = nlohmann::json;

  uint64_t NextService()
  {
    static std::atomic<uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
  }

  class FileSink final : public Sink
  {
  public:
    explicit FileSink(const Options& options)
        : storage_(options.destination, options.storage)
    {
    }
    std::filesystem::path Open(std::string_view id, std::string_view session, std::string_view manifest) override
    { return storage_.Open(id, session, manifest); }
    void Append(std::string_view id, std::string_view line) override
    { storage_.Append(id, line); }
    void Flush(std::string_view id) override
    { storage_.Flush(id); }
    void Close(std::string_view id) override
    { storage_.Close(id); }

  private:
    Storage storage_;
  };

  bool Identifier(std::string_view name, bool recorder = false)
  {
    if (name.empty() || name.size() > 80)
      return false;
    for (const unsigned char c : name)
      if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_' && c != '-'
          && (recorder || (!(c >= 'A' && c <= 'Z') && c != '.')))
        return false;
    return true;
  }

  uint64_t UtcMilliseconds()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  // Charge retained capacity rather than just visible text: a tiny string with a huge reserve must not evade the
  // queue budget. These native snapshots contain no managed pointers or callbacks.
  uint64_t Charge(const Event& event)
  {
    uint64_t   bytes = sizeof(Event);
    const auto add   = [&](uint64_t count) {
      if (count > std::numeric_limits<uint64_t>::max() - bytes)
        bytes = std::numeric_limits<uint64_t>::max();
      else
        bytes += count;
    };
    add(event.type.capacity() + 1);
    if (event.fields.capacity() > std::numeric_limits<uint64_t>::max() / sizeof(Field))
      return std::numeric_limits<uint64_t>::max();
    add(event.fields.capacity() * sizeof(Field));
    for (const auto& field : event.fields) {
      add(field.name.capacity() + 1);
      if (const auto* value = std::get_if<std::string>(&field.value))
        add(value->capacity() + 1);
    }
    return bytes;
  }

  bool Valid(const Event& event)
  {
    if (!Identifier(event.type) || event.type == "capture_end" || event.fields.size() > 64)
      return false;
    for (size_t i = 0; i < event.fields.size(); ++i) {
      if (!Identifier(event.fields[i].name))
        return false;
      if (const auto* number = std::get_if<double>(&event.fields[i].value); number && !std::isfinite(*number))
        return false;
      for (size_t j = 0; j < i; ++j)
        if (event.fields[i].name == event.fields[j].name)
          return false;
    }
    return true;
  }
} // namespace

struct Service::Impl {
  struct Packet {
    Event    event;
    uint64_t bytes;
    uint64_t sequence;
    uint64_t utc_ms;
    int64_t  monotonic_us;
  };
  struct Slot {
    explicit Slot(Recorder item)
        : recorder(std::move(item))
    { state.store(recorder.available ? State::kOff : State::kUnavailable); }
    Recorder                  recorder;
    std::atomic<State>        state{State::kOff};
    std::atomic<uint64_t>     generation{0};
    std::atomic<uint64_t>     busy{0}, full{0}, invalid{0};
    std::deque<Packet>        queue;
    std::string               session, error;
    std::filesystem::path     directory;
    std::chrono::milliseconds duration{};
    Clock::time_point         deadline   = Clock::time_point::max();
    Clock::time_point         flushed_at = Clock::now();
    bool                      opened     = false;
    uint64_t                  accepted = 0, written = 0, flushed = 0, failed = 0, events = 0, bytes = 0;
  };
  enum class Work { kNone, kOpen, kWrite, kFlush, kClose };

  const uint64_t                     instance = NextService();
  Identity                           identity;
  Options                            options;
  SinkFactory                        factory;
  std::vector<std::unique_ptr<Slot>> slots;
  mutable std::mutex                 mutex;
  std::condition_variable            wake;
  std::thread                        worker;
  std::mutex                         shutdown_mutex;
  bool                               closing        = false;
  uint64_t                           pending_events = 0, pending_bytes = 0;
  size_t                             cursor = 0;
  std::unique_ptr<Sink>              sink;

  Impl(std::vector<Recorder> recorders, Identity description, Options settings, SinkFactory make_sink)
      : identity(std::move(description))
      , options(std::move(settings))
      , factory(std::move(make_sink))
  {
    const auto& q = options.queue;
    if (options.destination.empty() || !options.destination.is_absolute())
      throw std::invalid_argument("Capture destination must be an absolute machine-local folder");
    if (!q.event_bytes || q.event_bytes > q.recorder_bytes || q.recorder_bytes > q.total_bytes || !q.recorder_events
        || q.recorder_events > q.total_events || options.flush_interval.count() < 1
        || options.flush_interval > std::chrono::minutes(1) || recorders.size() > 64)
      throw std::invalid_argument("Invalid capture queue or flush limits");
    for (auto& recorder : recorders) {
      if (!Identifier(recorder.id, true) || recorder.name.empty() || recorder.name.size() > 256
          || !recorder.schema_version || recorder.unavailable_reason.size() > 1024)
        throw std::invalid_argument("Invalid capture recorder descriptor");
      for (const auto& existing : slots)
        if (existing->recorder.id == recorder.id)
          throw std::invalid_argument("Duplicate capture recorder ID");
      slots.push_back(std::make_unique<Slot>(std::move(recorder)));
    }
    for (const auto* value : {&identity.source, &identity.build, &identity.client_version, &identity.platform})
      if (value->size() > 1024)
        throw std::invalid_argument("Capture identity is too large");
    if (!factory)
      factory = [](const Options& selected) { return std::make_unique<FileSink>(selected); };
  }

  Status Snapshot(const Slot& slot) const
  {
    Status result;
    result.recorder        = slot.recorder;
    result.state           = slot.state.load();
    result.session         = slot.session;
    result.directory       = slot.directory;
    result.error           = slot.error;
    result.accepted        = slot.accepted;
    result.written         = slot.written;
    result.flushed         = slot.flushed;
    result.dropped_busy    = slot.busy.load();
    result.dropped_full    = slot.full.load();
    result.dropped_invalid = slot.invalid.load();
    result.dropped_failed  = slot.failed;
    result.pending_events  = slot.events;
    result.pending_bytes   = slot.bytes;
    return result;
  }

  std::string Manifest(const Slot& slot)
  {
    Json compiled = Json::array();
    for (const auto& item : slots)
      compiled.push_back({{"id", item->recorder.id},
                          {"schema_version", item->recorder.schema_version},
                          {"experimental", item->recorder.experimental},
                          {"available", item->recorder.available}});
    const auto& q = options.queue;
    const auto& s = options.storage;
    return Json{{"format_version", 1},
                {"session", slot.session},
                {"recorder", slot.recorder.id},
                {"schema_version", slot.recorder.schema_version},
                {"started_utc_ms", UtcMilliseconds()},
                {"identity",
                 {{"source", identity.source},
                  {"build", identity.build},
                  {"client_version", identity.client_version},
                  {"platform", identity.platform}}},
                {"compiled_recorders", compiled},
                {"queue_limits",
                 {{"recorder_events", q.recorder_events},
                  {"recorder_bytes", q.recorder_bytes},
                  {"total_events", q.total_events},
                  {"total_bytes", q.total_bytes},
                  {"event_bytes", q.event_bytes}}},
                {"storage_limits",
                 {{"file_bytes", s.file_bytes},
                  {"recorder_bytes", s.recorder_bytes},
                  {"total_bytes", s.total_bytes},
                  {"min_free_bytes", s.min_free_bytes}}},
                {"local_only", true}}
        .dump();
  }

  std::string Encode(const Slot& slot, const Packet& packet)
  {
    Json fields = Json::object();
    for (const auto& field : packet.event.fields)
      std::visit(
          [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>)
              fields[field.name] = nullptr;
            else
              fields[field.name] = value;
          },
          field.value);
    return Json{{"format_version", 1},
                {"recorder", slot.recorder.id},
                {"schema_version", slot.recorder.schema_version},
                {"session", slot.session},
                {"sequence", packet.sequence},
                {"utc_ms", packet.utc_ms},
                {"monotonic_us", packet.monotonic_us},
                {"type", packet.event.type},
                {"fields", fields}}
               .dump()
           + '\n';
  }

  void Release(Slot& slot, uint64_t bytes)
  {
    --slot.events;
    slot.bytes -= bytes;
    --pending_events;
    pending_bytes -= bytes;
  }

  void Fail(Slot& slot, const std::string& message, Work work, const Packet& packet)
  {
    // Only this recorder fails. The sink is worker-owned, including best-effort close of partial opens.
    try {
      if (sink)
        sink->Close(slot.recorder.id);
    } catch (...) {
    }
    std::lock_guard lock(mutex);
    if (work == Work::kWrite) {
      Release(slot, packet.bytes);
      ++slot.failed;
    }
    for (const auto& queued : slot.queue) {
      Release(slot, queued.bytes);
      ++slot.failed;
    }
    slot.queue.clear();
    slot.opened = false;
    slot.error  = message;
    slot.state.store(State::kFailed, std::memory_order_release);
  }

  void Run() noexcept
  {
    // All service-owned disk access and serialization happens here. One item per turn prevents a busy recorder
    // starving another. Queue charges include the item outside the lock while its sink writes.
    try {
      for (;;) {
        Work   work     = Work::kNone;
        Slot*  selected = nullptr;
        Packet packet{};
        {
          std::unique_lock lock(mutex);
          const auto       now = Clock::now();
          for (auto& item : slots)
            if (item->state.load() == State::kRecording && now >= item->deadline)
              item->state.store(State::kStopping, std::memory_order_release);
          for (size_t n = 0; n < slots.size(); ++n) {
            const size_t index = (cursor + n) % slots.size();
            auto&        slot  = *slots[index];
            const auto   state = slot.state.load();
            if (state == State::kStarting || (state == State::kStopping && !slot.opened))
              work = Work::kOpen;
            else if (state == State::kRecording && slot.written != slot.flushed
                     && now - slot.flushed_at >= options.flush_interval)
              work = Work::kFlush;
            else if (!slot.queue.empty()) {
              work   = Work::kWrite;
              packet = std::move(slot.queue.front());
              slot.queue.pop_front();
            } else if (state == State::kStopping && slot.opened)
              work = Work::kClose;
            if (work != Work::kNone) {
              selected = &slot;
              cursor   = (index + 1) % slots.size();
              break;
            }
          }
          if (!selected) {
            if (closing)
              break;
            wake.wait_for(lock, options.flush_interval);
            continue;
          }
        }
        auto& slot = *selected;
        try {
          if (work == Work::kOpen) {
            if (!sink) {
              sink = factory(options);
              if (!sink)
                throw std::runtime_error("Capture sink factory returned no sink");
            }
            static std::atomic<uint64_t> session_counter{0};
            std::random_device           entropy;
            const auto session = std::to_string(UtcMilliseconds()) + "-" + std::to_string(entropy()) + "-"
                                 + std::to_string(++session_counter);
            {
              std::lock_guard lock(mutex);
              slot.session = session;
            }
            const auto      directory = sink->Open(slot.recorder.id, session, Manifest(slot));
            std::lock_guard lock(mutex);
            slot.directory  = directory;
            slot.opened     = true;
            slot.deadline   = slot.duration.count() ? Clock::now() + slot.duration : Clock::time_point::max();
            slot.flushed_at = Clock::now();
            if (slot.state.load() != State::kStopping)
              slot.state.store(State::kRecording, std::memory_order_release);
          } else if (work == Work::kWrite) {
            std::string line;
            try {
              line = Encode(slot, packet);
            } catch (const Json::exception&) {
              std::lock_guard lock(mutex);
              Release(slot, packet.bytes);
              ++slot.invalid;
              continue;
            }
            sink->Append(slot.recorder.id, line);
            std::lock_guard lock(mutex);
            Release(slot, packet.bytes);
            ++slot.written;
          } else if (work == Work::kFlush) {
            sink->Flush(slot.recorder.id);
            std::lock_guard lock(mutex);
            slot.flushed    = slot.written;
            slot.flushed_at = Clock::now();
          } else if (work == Work::kClose) {
            Json ending;
            {
              std::lock_guard lock(mutex);
              ending = {{"type", "capture_end"},
                        {"session", slot.session},
                        {"accepted", slot.accepted},
                        {"written", slot.written},
                        {"dropped_busy", slot.busy.load()},
                        {"dropped_full", slot.full.load()},
                        {"dropped_invalid", slot.invalid.load()},
                        {"dropped_failed", slot.failed},
                        {"drop_counter_scope", "service_lifetime"}};
            }
            sink->Append(slot.recorder.id, ending.dump() + '\n');
            sink->Flush(slot.recorder.id);
            sink->Close(slot.recorder.id);
            std::lock_guard lock(mutex);
            slot.opened  = false;
            slot.flushed = slot.written;
            slot.state.store(State::kOff, std::memory_order_release);
          }
        } catch (const std::exception& error) {
          Fail(slot, error.what(), work, packet);
        } catch (...) {
          Fail(slot, "Unknown capture sink failure", work, packet);
        }
      }
    } catch (...) {
      // An unexpected worker failure must not terminate the game or leave a recorder claiming to be healthy.
      std::lock_guard lock(mutex);
      for (auto& item : slots) {
        item->failed += item->events;
        item->queue.clear();
        item->events = item->bytes = 0;
        if (item->state.load() != State::kUnavailable)
          item->state.store(State::kFailed, std::memory_order_release);
      }
      pending_events = pending_bytes = 0;
      closing                        = true;
    }
    sink.reset();
  }
};

Service::Service(std::vector<Recorder> recorders, Identity identity, Options options, SinkFactory factory)
    : impl_(std::make_unique<Impl>(std::move(recorders), std::move(identity), std::move(options), std::move(factory)))
{
}
Service::~Service()
{ Shutdown(); }

bool Service::Start(size_t recorder, std::chrono::milliseconds duration)
{
  auto&           impl = *impl_;
  std::lock_guard lock(impl.mutex);
  if (recorder >= impl.slots.size() || impl.closing || duration.count() < 0 || duration > std::chrono::hours(24 * 7))
    return false;
  auto&      slot  = *impl.slots[recorder];
  const auto state = slot.state.load();
  if (state != State::kOff && state != State::kFailed)
    return false;
  slot.session.clear();
  slot.directory.clear();
  slot.error.clear();
  slot.duration = duration;
  slot.deadline = Clock::time_point::max();
  slot.accepted = slot.written = slot.flushed = 0;
  slot.generation.fetch_add(1, std::memory_order_release);
  slot.state.store(State::kStarting, std::memory_order_release);
  try {
    if (!impl.worker.joinable())
      impl.worker = std::thread([&impl] { impl.Run(); });
  } catch (const std::exception& error) {
    slot.error = error.what();
    slot.state.store(State::kFailed, std::memory_order_release);
    return false;
  }
  impl.wake.notify_one();
  return true;
}

bool Service::Stop(size_t recorder)
{
  auto&           impl = *impl_;
  std::lock_guard lock(impl.mutex);
  if (recorder >= impl.slots.size())
    return false;
  auto& slot = *impl.slots[recorder];
  if (slot.state.load() != State::kRecording && slot.state.load() != State::kStarting)
    return false;
  slot.state.store(State::kStopping, std::memory_order_release);
  impl.wake.notify_one();
  return true;
}

bool Service::IsRecording(size_t recorder) const noexcept
{
  return recorder < impl_->slots.size()
         && impl_->slots[recorder]->state.load(std::memory_order_acquire) == State::kRecording;
}

SubmitResult Service::TrySubmit(size_t recorder, Event event) noexcept
{
  const auto ticket = BeginSnapshot(recorder);
  if (!ticket)
    return SubmitResult::kNotRecording;
  return TrySubmit(*ticket, std::move(event));
}

std::optional<CaptureTicket> Service::BeginSnapshot(size_t recorder) const noexcept
{
  if (recorder >= impl_->slots.size())
    return {};
  const auto generation = impl_->slots[recorder]->generation.load(std::memory_order_acquire);
  if (!IsRecording(recorder))
    return {};
  return CaptureTicket{recorder, generation, impl_->instance};
}

SubmitResult Service::TrySubmit(CaptureTicket ticket, Event event) noexcept
{
  const auto recorder = ticket.recorder;
  if (ticket.service != impl_->instance || !IsRecording(recorder)
      || impl_->slots[recorder]->generation.load(std::memory_order_acquire) != ticket.generation)
    return SubmitResult::kNotRecording;
  auto& impl = *impl_;
  auto& slot = *impl.slots[recorder];
  if (!Valid(event)) {
    ++slot.invalid;
    return SubmitResult::kInvalid;
  }
  const auto bytes = Charge(event);
  if (bytes > impl.options.queue.event_bytes) {
    ++slot.invalid;
    return SubmitResult::kTooLarge;
  }
  std::unique_lock lock(impl.mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    ++slot.busy;
    return SubmitResult::kBusy;
  }
  if (slot.state.load() != State::kRecording || impl.closing || slot.generation.load() != ticket.generation)
    return SubmitResult::kNotRecording;
  const auto& q = impl.options.queue;
  if (slot.events >= q.recorder_events || impl.pending_events >= q.total_events || bytes > q.recorder_bytes - slot.bytes
      || bytes > q.total_bytes - impl.pending_bytes) {
    ++slot.full;
    return SubmitResult::kQueueFull;
  }
  try {
    impl.slots[recorder]->queue.push_back(
        {std::move(event), bytes, slot.accepted + 1, UtcMilliseconds(),
         std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count()});
  } catch (...) {
    ++slot.full;
    return SubmitResult::kQueueFull;
  }
  ++slot.accepted;
  ++slot.events;
  slot.bytes += bytes;
  ++impl.pending_events;
  impl.pending_bytes += bytes;
  lock.unlock();
  impl.wake.notify_one();
  return SubmitResult::kAccepted;
}

SubmitResult Service::Mark(size_t recorder, std::string note) noexcept
{
  try {
    if (!IsRecording(recorder))
      return SubmitResult::kNotRecording;
    if (note.size() > 1024) {
      ++impl_->slots[recorder]->invalid;
      return SubmitResult::kTooLarge;
    }
    return TrySubmit(recorder, {"marker", {{"note", std::move(note)}}});
  } catch (...) {
    return SubmitResult::kQueueFull;
  }
}

Status Service::GetStatus(size_t recorder) const
{
  std::lock_guard lock(impl_->mutex);
  if (recorder >= impl_->slots.size())
    throw std::out_of_range("Unknown capture recorder");
  return impl_->Snapshot(*impl_->slots[recorder]);
}

std::vector<Status> Service::GetStatus() const
{
  std::lock_guard     lock(impl_->mutex);
  std::vector<Status> result;
  for (const auto& slot : impl_->slots)
    result.push_back(impl_->Snapshot(*slot));
  return result;
}

void Service::Shutdown()
{
  if (!impl_)
    return;
  auto&           impl = *impl_;
  std::lock_guard shutdown_lock(impl.shutdown_mutex);
  {
    std::lock_guard lock(impl.mutex);
    impl.closing = true;
    for (const auto& slot : impl.slots)
      if (slot->state.load() == State::kRecording || slot->state.load() == State::kStarting)
        slot->state.store(State::kStopping, std::memory_order_release);
  }
  impl.wake.notify_one();
  if (impl.worker.joinable())
    impl.worker.join();
}
} // namespace diagnostics
