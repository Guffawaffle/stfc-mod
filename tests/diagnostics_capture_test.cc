#include "diagnostics/capture.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
namespace fs = std::filesystem;
using Json   = nlohmann::json;
using namespace diagnostics;

namespace
{
unsigned checks = 0;

void Check(bool condition, std::string_view message)
{
  ++checks;
  if (!condition)
    throw std::runtime_error(std::string(message));
}

template <typename F> void Throws(F&& operation, std::string_view message)
{
  bool threw = false;
  try {
    operation();
  } catch (const std::exception&) {
    threw = true;
  }
  Check(threw, message);
}

template <typename F> void Eventually(F&& predicate, std::string_view message)
{
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  do {
    if (predicate()) {
      Check(true, message);
      return;
    }
    std::this_thread::sleep_for(1ms);
  } while (std::chrono::steady_clock::now() < deadline);
  Check(false, message);
}

struct FakeState {
  std::mutex                               mutex;
  std::condition_variable                  changed;
  std::thread::id                          controller = std::this_thread::get_id();
  std::set<std::thread::id>                workers;
  std::map<std::string, Json>              manifests;
  std::map<std::string, std::vector<Json>> events;
  std::vector<std::string>                 order;
  std::string                              block;
  std::string                              fail;
  bool                                     entered       = false;
  bool                                     released      = false;
  bool                                     controller_io = false;
  unsigned                                 factories     = 0;
};

class FakeSink final : public Sink
{
public:
  explicit FakeSink(std::shared_ptr<FakeState> state)
      : state_(std::move(state))
  {
  }

  fs::path Open(std::string_view recorder, std::string_view session, std::string_view manifest) override
  {
    std::lock_guard lock(state_->mutex);
    Observe();
    state_->manifests[std::string(recorder)] = Json::parse(manifest);
    return fs::path("fixture") / recorder / session;
  }

  void Append(std::string_view recorder, std::string_view line) override
  {
    const auto       event = Json::parse(line);
    std::unique_lock lock(state_->mutex);
    Observe();
    if (event.value("type", "") != "capture_end") {
      if (recorder == state_->block && !state_->released) {
        state_->entered = true;
        state_->changed.notify_all();
        state_->changed.wait(lock, [&] { return state_->released; });
      }
      if (recorder == state_->fail)
        throw std::runtime_error("injected recorder write failure");
      state_->events[std::string(recorder)].push_back(event);
      state_->order.emplace_back(recorder);
    }
    state_->changed.notify_all();
  }

  void Flush(std::string_view) override
  {
    std::lock_guard lock(state_->mutex);
    Observe();
  }

  void Close(std::string_view) override
  {
    std::lock_guard lock(state_->mutex);
    Observe();
  }

private:
  void Observe()
  {
    state_->workers.insert(std::this_thread::get_id());
    state_->controller_io |= std::this_thread::get_id() == state_->controller;
  }
  std::shared_ptr<FakeState> state_;
};

SinkFactory Factory(const std::shared_ptr<FakeState>& state)
{
  return [state](const Options&) {
    std::lock_guard lock(state->mutex);
    ++state->factories;
    state->controller_io |= std::this_thread::get_id() == state->controller;
    return std::make_unique<FakeSink>(state);
  };
}

// Always release a fake slow disk before Service teardown, even if an assertion throws.
struct ReleaseGate {
  std::shared_ptr<FakeState> state;
  ~ReleaseGate()
  { Release(); }
  void Release()
  {
    std::lock_guard lock(state->mutex);
    state->released = true;
    state->changed.notify_all();
  }
  void Wait()
  {
    std::unique_lock lock(state->mutex);
    Check(state->changed.wait_for(lock, 5s, [&] { return state->entered; }), "fake slow writer entered");
  }
};

Options Config(const fs::path& root)
{
  Options options;
  options.destination            = root;
  options.flush_interval         = 5ms;
  options.storage.min_free_bytes = 0;
  return options;
}

Event Sample(uint64_t number = 1)
{ return {"sample", {{"number", number}, {"readable", true}}}; }

void Submit(Service& service, size_t recorder, uint64_t number = 1)
{
  Eventually(
      [&] {
        const auto result = service.TrySubmit(recorder, Sample(number));
        return result == SubmitResult::kAccepted;
      },
      "snapshot admitted");
}

void Start(Service& service, size_t recorder)
{
  Check(service.Start(recorder), "start accepted");
  Eventually([&] { return service.IsRecording(recorder); }, "recorder becomes ready");
}

void Stop(Service& service, size_t recorder)
{
  Check(service.Stop(recorder), "stop accepted");
  Eventually([&] { return service.GetStatus(recorder).state == State::kOff; }, "stop drains and closes");
}

void RegistryAndScalars(const fs::path& root)
{
  auto     state = std::make_shared<FakeState>();
  Recorder unavailable{"missing", "Missing", 1, false, false, "client member unavailable"};
  Service  service({{"claims", "Claims", 3}, unavailable}, {"source", "build", "273", "fixture"}, Config(root),
                   Factory(state));
  Check(!service.IsRecording(0), "included recorder begins off");
  Check(service.TrySubmit(0, Sample()) == SubmitResult::kNotRecording, "off recorder refuses snapshots");
  Check(service.GetStatus(1).state == State::kUnavailable, "unavailable distinguished from off");
  Check(!service.Start(1), "unavailable recorder cannot start");
  {
    std::lock_guard lock(state->mutex);
    Check(state->factories == 0, "off service performs no sink construction or IO");
  }
  Start(service, 0);
  Check(!service.Start(0), "duplicate start refused");
  Eventually(
      [&] {
        return service.TrySubmit(0, {"observation", {{"unknown", std::monostate{}}, {"negative", false}}})
               == SubmitResult::kAccepted;
      },
      "unknown observation admitted");
  Eventually([&] { return service.GetStatus(0).flushed == 1; }, "stream flush visible");
  Stop(service, 0);
  Check(service.TrySubmit(0, Sample()) == SubmitResult::kNotRecording, "stopped recorder refuses snapshots");
  const auto status = service.GetStatus(0);
  Check(status.accepted == 1 && status.written == 1 && status.flushed == 1,
        "accepted written and flushed distinguished");
  Check(status.pending_events == 0 && status.pending_bytes == 0, "stop releases queue budgets");
  {
    std::lock_guard lock(state->mutex);
    const auto&     event = state->events.at("claims").at(0);
    Check(event.at("fields").at("unknown").is_null(), "unreadable value remains null");
    Check(event.at("fields").at("negative") == false, "observed false remains false");
    Check(event.at("schema_version") == 3 && event.at("sequence") == 1, "event retains schema and sequence");
    Check(!event.at("session").get<std::string>().empty(), "event has capture identity");
    const auto manifest = state->manifests.at("claims").dump();
    Check(manifest.find("source") != std::string::npos && manifest.find("compiled_recorders") != std::string::npos,
          "capture identifies build and compiled capabilities");
    Check(state->workers.size() == 1 && !state->controller_io, "one worker exclusively owns all sink operations");
  }
  Check(service.Mark(0, "off") == SubmitResult::kNotRecording, "problem marker does not implicitly start capture");
  const auto previous_session = status.session;
  Start(service, 0);
  Check(service.Mark(0, std::string(2048, 'x')) == SubmitResult::kTooLarge, "problem marker note is bounded");
  Eventually([&] { return service.Mark(0, "missed enter") == SubmitResult::kAccepted; }, "problem marker admitted");
  Stop(service, 0);
  Check(service.GetStatus(0).session != previous_session, "restarted capture receives fresh session identity");
  {
    std::lock_guard lock(state->mutex);
    const auto&     marker = state->events.at("claims").at(1);
    Check(marker.at("type") == "marker" && marker.at("fields").at("note") == "missed enter",
          "problem marker records owned note");
  }
}

void SlowWriterAndBudgets(const fs::path& root)
{
  auto state                    = std::make_shared<FakeState>();
  state->block                  = "alpha";
  auto options                  = Config(root);
  options.queue.recorder_events = 3;
  options.queue.total_events    = 4;
  Service     service({{"alpha", "Alpha"}, {"beta", "Beta"}}, {}, options, Factory(state));
  ReleaseGate gate{state};
  Start(service, 0);
  Start(service, 1);
  Submit(service, 0);
  gate.Wait();

  // A producer must finish while the disk is still deliberately blocked. A blocking queue implementation is
  // released before the assertion, so fixture failure does not leave Service destruction hanging.
  bool                    finished = false;
  std::mutex              finished_mutex;
  std::condition_variable finished_changed;
  std::thread             producer([&] {
    for (unsigned i = 0; i != 50; ++i)
      service.TrySubmit(0, Sample(i));
    {
      std::lock_guard lock(finished_mutex);
      finished = true;
    }
    finished_changed.notify_all();
  });
  bool                    nonblocking;
  {
    std::unique_lock lock(finished_mutex);
    nonblocking = finished_changed.wait_for(lock, 2s, [&] { return finished; });
  }
  if (!nonblocking)
    gate.Release();
  producer.join();
  Check(nonblocking, "producer never waits for blocked sink");
  const auto blocked = service.GetStatus(0);
  Check(blocked.pending_events <= 3 && blocked.pending_bytes <= options.queue.recorder_bytes,
        "per-recorder queue stays bounded including inflight");
  Check(blocked.dropped_full + blocked.dropped_busy > 0, "overload is reported rather than unbounded");
  Submit(service, 1);
  Check(service.GetStatus(1).pending_events == 1, "one recorder cannot consume another recorder's reservation");
  SubmitResult global_limit = SubmitResult::kBusy;
  Eventually(
      [&] {
        global_limit = service.TrySubmit(1, Sample(999));
        return global_limit != SubmitResult::kBusy;
      },
      "global budget checked without waiting on the blocked writer");
  Check(global_limit == SubmitResult::kQueueFull, "global queue cap applies before the second recorder fills");
  Check(service.GetStatus(0).pending_events + service.GetStatus(1).pending_events == 4,
        "global pending count includes inflight snapshots");
  gate.Release();
  Stop(service, 0);
  Stop(service, 1);
  const auto a = service.GetStatus(0), b = service.GetStatus(1);
  Check(a.accepted == a.written && b.accepted == b.written, "all accepted snapshots drain after slow IO");
  Check(a.pending_bytes == 0 && b.pending_bytes == 0, "both recorder budgets released");
}

void SessionTickets(const fs::path& root)
{
  auto    state = std::make_shared<FakeState>();
  Service service({{"claims", "Claims"}}, {}, Config(root), Factory(state));
  Check(!service.BeginSnapshot(0), "off recorder does not issue snapshot tickets");
  Check(!service.BeginSnapshot(99), "unknown recorder does not issue snapshot tickets");
  Start(service, 0);
  const auto old_ticket = service.BeginSnapshot(0);
  Check(old_ticket.has_value(), "recording session issues snapshot ticket");
  Check(service.TrySubmit(*old_ticket, {"", {}}) == SubmitResult::kInvalid,
        "first capture records an invalid-observation count");
  const auto previous_drops = service.GetStatus(0).dropped_invalid;
  Stop(service, 0);
  Start(service, 0);
  Check(service.GetStatus(0).dropped_invalid == previous_drops, "restart preserves service-lifetime drop counters");
  const auto current_ticket = service.BeginSnapshot(0);
  Check(current_ticket && current_ticket->generation != old_ticket->generation, "restart advances snapshot generation");
  Check(service.TrySubmit(*old_ticket, Sample(100)) == SubmitResult::kNotRecording,
        "old asynchronous snapshot cannot enter restarted capture");
  Check(service.TrySubmit(CaptureTicket{99, current_ticket->generation, current_ticket->service}, Sample(200))
            == SubmitResult::kNotRecording,
        "unknown recorder ticket rejected");
  Eventually([&] { return service.TrySubmit(*current_ticket, Sample(300)) == SubmitResult::kAccepted; },
             "current session accepts its ticket");
  Check(service.TrySubmit(*current_ticket, {"capture_end", {}}) == SubmitResult::kInvalid,
        "recorder cannot forge the service capture-end record");
  Stop(service, 0);
  const auto status = service.GetStatus(0);
  Check(status.accepted == 1 && status.written == 1, "stale snapshot does not affect new-session admission");
  Check(status.dropped_invalid >= previous_drops + 1, "drop counts accumulate across capture sessions");
  {
    std::lock_guard lock(state->mutex);
    Check(state->events.at("claims").size() == 1 && state->events.at("claims").front().at("fields").at("number") == 300,
          "new session contains only its current-ticket observation");
  }

  auto    replacement_state = std::make_shared<FakeState>();
  Service replacement({{"claims", "Claims"}}, {}, Config(root / "replacement"), Factory(replacement_state));
  Start(replacement, 0);
  const auto replacement_ticket = replacement.BeginSnapshot(0);
  Check(replacement_ticket && replacement_ticket->generation == old_ticket->generation
            && replacement_ticket->service != old_ticket->service,
        "replacement service has distinct identity despite matching recorder generation");
  Check(replacement.TrySubmit(*old_ticket, Sample(400)) == SubmitResult::kNotRecording,
        "destination replacement refuses tickets from the previous service");
  Eventually([&] { return replacement.TrySubmit(*replacement_ticket, Sample(500)) == SubmitResult::kAccepted; },
             "replacement service admits its own snapshot ticket");
  Stop(replacement, 0);
  Check(replacement.GetStatus(0).accepted == 1, "foreign-service snapshot never reaches replacement queue");
}

void FairScheduling(const fs::path& root)
{
  auto state   = std::make_shared<FakeState>();
  state->block = "alpha";
  Service     service({{"alpha", "Alpha"}, {"beta", "Beta"}}, {}, Config(root), Factory(state));
  ReleaseGate gate{state};
  Start(service, 0);
  Start(service, 1);
  Submit(service, 0);
  gate.Wait();
  for (unsigned i = 0; i != 20; ++i)
    Submit(service, 0, i);
  Submit(service, 1);
  gate.Release();
  Stop(service, 0);
  Stop(service, 1);
  std::lock_guard lock(state->mutex);
  const auto      beta = std::find(state->order.begin(), state->order.end(), "beta");
  Check(beta != state->order.end() && std::distance(state->order.begin(), beta) <= 2,
        "quiet recorder serviced before busy recorder backlog drains");
}

void FailureAndValidation(const fs::path& root)
{
  auto state                = std::make_shared<FakeState>();
  state->fail               = "broken";
  auto options              = Config(root);
  options.queue.event_bytes = 2048;
  Service service({{"broken", "Broken"}, {"healthy", "Healthy"}}, {}, options, Factory(state));
  Start(service, 0);
  Start(service, 1);
  Submit(service, 0);
  Eventually([&] { return service.GetStatus(0).state == State::kFailed; }, "sink error becomes failed status");
  Check(!service.GetStatus(0).error.empty(), "sink failure retains actionable reason");
  Check(service.GetStatus(0).pending_bytes == 0 && service.GetStatus(0).dropped_failed >= 1,
        "failure releases rejected backlog");
  Submit(service, 1);
  Eventually([&] { return service.GetStatus(1).written == 1; }, "healthy recorder survives other sink error");
  Check(service.TrySubmit(1, {"", {}}) == SubmitResult::kInvalid, "empty event type rejected");
  Check(service.TrySubmit(1, {"sample", {{"same", true}, {"same", false}}}) == SubmitResult::kInvalid,
        "duplicate fields rejected");
  Check(service.TrySubmit(1, {"sample", {{"large", std::string(4096, 'x')}}}) == SubmitResult::kTooLarge,
        "oversized snapshot rejected before queue");
  std::string reserved = "small";
  reserved.reserve(8192);
  Event retained{"sample", {}};
  retained.fields.push_back({"reserved", std::move(reserved)});
  Check(service.TrySubmit(1, std::move(retained)) == SubmitResult::kTooLarge,
        "retained allocation capacity counts against admission");
  Stop(service, 1);
  Check(service.GetStatus(1).dropped_invalid >= 4, "invalid observations counted");

  Throws([&] { Service invalid({{"../escape", "Bad"}}, {}, options, Factory(state)); }, "unsafe recorder ID rejected");
  Throws([&] { Service duplicate({{"same", "First"}, {"same", "Second"}}, {}, options, Factory(state)); },
         "duplicate recorder IDs rejected");
  Throws([&] { Service invalid({{"bad", "Bad", 0}}, {}, options, Factory(state)); }, "zero schema version rejected");
}

void ConcurrentAndDuration(const fs::path& root)
{
  auto    state = std::make_shared<FakeState>();
  Service service({{"alpha", "Alpha"}, {"beta", "Beta"}}, {}, Config(root), Factory(state));
  Start(service, 0);
  Start(service, 1);
  std::atomic<bool>        failed = false;
  std::vector<std::thread> producers;
  for (unsigned thread = 0; thread != 4; ++thread)
    producers.emplace_back([&, thread] {
      for (unsigned event = 0; event != 20; ++event) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        for (;;) {
          if (service.TrySubmit(thread % 2, Sample(thread * 100 + event)) == SubmitResult::kAccepted)
            break;
          if (std::chrono::steady_clock::now() >= deadline) {
            failed = true;
            return;
          }
          std::this_thread::yield();
        }
      }
    });
  for (auto& producer : producers)
    producer.join();
  Check(!failed, "concurrent producers all finish");
  Stop(service, 0);
  Stop(service, 1);
  {
    std::lock_guard    lock(state->mutex);
    std::set<uint64_t> identifiers;
    for (const auto& [recorder, events] : state->events) {
      Check(events.size() == 40, "each recorder retains all accepted concurrent events");
      for (const auto& event : events)
        identifiers.insert(event.at("fields").at("number").get<uint64_t>());
    }
    Check(identifiers.size() == 80, "concurrent snapshots retain owned values without duplication");
    Check(state->workers.size() == 1, "multiple producers and recorders share one writer");
  }
  Check(service.Start(0, 25ms), "duration-limited session accepted");
  Eventually([&] { return service.GetStatus(0).state == State::kOff; }, "capture automatically stops after duration");
  service.Shutdown();
  Check(!service.Start(0), "shutdown refuses new sessions");
}

uint64_t ManagedBytes(const fs::path& root)
{
  uint64_t bytes = 0;
  for (const auto& entry : fs::recursive_directory_iterator(root))
    if (entry.is_regular_file()
        && (entry.path().extension() == ".jsonl" || entry.path().filename() == "manifest.json"
            || entry.path().filename() == "capture.owner"))
      bytes += entry.file_size();
  return bytes;
}

void FileStorage(const fs::path& root)
{
  fs::create_directories(root);
  const auto report = root / "saved-report.zip";
  {
    std::ofstream output(report);
    output << "KEEP THIS REPORT";
  }
  StorageLimits limits;
  limits.file_bytes          = 4096;
  limits.recorder_bytes      = 12288;
  limits.total_bytes         = 20480;
  limits.min_free_bytes      = 0;
  const std::string manifest = "{\"format_version\":1}\n";
  const std::string line     = Json{{"payload", std::string(1300, 'x')}}.dump() + '\n';
  {
    Storage storage(root, limits);
    Throws([&] { Storage competing(root, limits); }, "selected destination exclusively leased");
    Throws([&] { storage.Open("../outside", "session", manifest); }, "recorder traversal refused");
    Throws([&] { storage.Open("valid", "../outside", manifest); }, "session traversal refused");
    const auto a = storage.Open("alpha", "session-a", manifest);
    const auto b = storage.Open("beta", "session-b", manifest);
    Check(a != b && fs::exists(a / "manifest.json"), "separate session paths and manifests");
    for (unsigned i = 0; i != 24; ++i) {
      storage.Append(i % 2 ? "alpha" : "beta", line);
      Check(ManagedBytes(root / "captures-v1") <= limits.total_bytes, "retention obeys global cap during rotation");
    }
    Throws([&] { storage.Append("alpha", std::string(5000, 'x') + '\n'); }, "oversized serialized event refused");
    storage.Flush("alpha");
    storage.Close("alpha");
    storage.Close("beta");
    Check(ManagedBytes(root / "captures-v1") <= limits.total_bytes, "fully flushed capture files obey global cap");
    Check(ManagedBytes(root / "captures-v1" / "alpha") <= limits.recorder_bytes, "alpha retention obeys recorder cap");
    Check(ManagedBytes(root / "captures-v1" / "beta") <= limits.recorder_bytes, "beta retention obeys recorder cap");
    for (const auto& entry : fs::recursive_directory_iterator(root / "captures-v1"))
      if (entry.path().extension() == ".jsonl")
        Check(entry.file_size() <= limits.file_bytes, "rotated file obeys individual cap");
  }
  {
    Storage reopened(root, limits);
  }
  std::ifstream input(report);
  std::string   text((std::istreambuf_iterator<char>(input)), {});
  Check(text == "KEEP THIS REPORT", "saved report never pruned");
  Throws([&] { Storage relative(fs::path("relative-captures"), limits); }, "relative destination rejected");
}

void StorageCaseAliases(const fs::path& root)
{
  StorageLimits limits;
  limits.file_bytes     = 4096;
  limits.recorder_bytes = 12288;
  limits.total_bytes    = 20480;
  limits.min_free_bytes = 0;
  Storage    storage(root, limits);
  const auto managed = root / "captures-v1";
  fs::create_directory(managed / "ALPHA");
  const auto directory = storage.Open("alpha", "first", "{\"format_version\":1}");
#if _WIN32
  // Windows preserves directory spelling while resolving alpha and ALPHA to the same physical location.
  // Retention must recognize this live capture even when iterator paths differ lexically from Open's path.
  Check(fs::equivalent(directory.parent_path(), managed / "ALPHA"),
        "uppercase physical directory aliases active recorder");
#endif
  Throws([&] { storage.Open("Alpha", "second", "{}"); }, "case alias cannot open another active capture");
  const std::string line = Json{{"payload", std::string(1300, 'x')}}.dump() + '\n';
  for (unsigned i = 0; i != 30; ++i)
    storage.Append("ALPHA", line);
  storage.Flush("AlPhA");
  Check(fs::is_directory(directory) && fs::exists(directory / "capture.owner"),
        "retention keeps case-aliased active session metadata");
  Check(ManagedBytes(managed) <= limits.recorder_bytes, "case aliases share recorder quota during rotation");
  storage.Close("Alpha");

  const auto next = storage.Open("AlPhA", "second", "{\"format_version\":1}");
  for (unsigned i = 0; i != 30; ++i)
    storage.Append("alpha", line);
  storage.Flush("ALPHA");
  Check(fs::exists(next / "capture.owner"), "reopened alias retains active ownership during history pruning");
  storage.Close("aLpHa");
  Check(ManagedBytes(managed) <= limits.recorder_bytes, "case-aliased histories remain within one recorder quota");
}
} // namespace

int main(int argc, char** argv)
{
  try {
    if (argc != 2)
      throw std::runtime_error("usage: diagnostics-capture-tests <new-fixture-directory>");
    const auto root = fs::absolute(fs::path(argv[1]));
    fs::create_directories(root);
    RegistryAndScalars(root / "registry");
    SessionTickets(root / "tickets");
    SlowWriterAndBudgets(root / "slow");
    FairScheduling(root / "fair");
    FailureAndValidation(root / "failure");
    ConcurrentAndDuration(root / "concurrent");
    FileStorage(root / "storage");
    StorageCaseAliases(root / "case-aliases");
    std::cout << "diagnostics capture: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "diagnostics capture failed after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
