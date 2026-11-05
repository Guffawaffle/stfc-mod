#pragma once

// Local, observation-only capture plumbing. Producers submit owned scalar snapshots; one worker owns all sinks.
// Controllers (eventually STFCkitUI) own Service lifetime. No game bindings, configuration reloads or uploads here.

#include "storage.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace diagnostics
{
using Scalar = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;
struct Field {
  std::string name;
  Scalar      value;
};
struct Event {
  std::string        type;
  std::vector<Field> fields;
};

struct Recorder {
  std::string id;
  std::string name;
  uint32_t    schema_version = 1;
  bool        available      = true;
  bool        experimental   = false;
  std::string unavailable_reason;
};
struct Identity {
  std::string source;
  std::string build;
  std::string client_version;
  std::string platform;
};
struct QueueLimits {
  uint64_t recorder_events = 128;
  uint64_t recorder_bytes  = 2 * 1024 * 1024;
  uint64_t total_events    = 1024;
  uint64_t total_bytes     = 8 * 1024 * 1024;
  uint64_t event_bytes     = 64 * 1024;
};
struct Options {
  // An explicit absolute, machine-local folder. Storage creates its own captures-v1 subtree beneath it.
  std::filesystem::path     destination;
  StorageLimits             storage;
  QueueLimits               queue;
  std::chrono::milliseconds flush_interval{250};
};

enum class State { kUnavailable, kOff, kStarting, kRecording, kStopping, kFailed };
enum class SubmitResult { kAccepted, kNotRecording, kBusy, kQueueFull, kTooLarge, kInvalid };
struct CaptureTicket {
  size_t   recorder;
  uint64_t generation;
  uint64_t service;
};
struct Status {
  Recorder              recorder;
  State                 state = State::kOff;
  std::string           session;
  std::filesystem::path directory;
  std::string           error;
  uint64_t              accepted = 0;
  uint64_t              written  = 0;
  uint64_t              flushed  = 0;
  // Drop counters cover this Service's lifetime, across capture restarts. This avoids losing or reattributing
  // a concurrent producer's refusal while a controller starts another session. Accepted/written/flushed above
  // describe the current (or last completed) session.
  uint64_t dropped_busy    = 0;
  uint64_t dropped_full    = 0;
  uint64_t dropped_invalid = 0;
  uint64_t dropped_failed  = 0;
  // Includes a snapshot currently being written, until its write finishes.
  uint64_t pending_events = 0;
  uint64_t pending_bytes  = 0;
};

// The only storage seam: called exclusively on the writer thread. Useful for alternate local sinks and fault
// fixtures; neither a sink nor its factory may call game APIs. Close must tolerate a partially failed Open.
class Sink
{
public:
  virtual ~Sink()                                                                             = default;
  virtual std::filesystem::path Open(std::string_view recorder, std::string_view session,
                                     std::string_view manifest_json)                          = 0;
  virtual void                  Append(std::string_view recorder, std::string_view json_line) = 0;
  virtual void                  Flush(std::string_view recorder)                              = 0;
  virtual void                  Close(std::string_view recorder)                              = 0;
};
using SinkFactory = std::function<std::unique_ptr<Sink>(const Options&)>;

class Service
{
public:
  // A fixed registry: inclusion is a build fact, availability is an observed client capability. All start Off.
  // A custom factory is optional; the normal implementation is the bounded local-file Storage backend.
  Service(std::vector<Recorder> recorders, Identity identity, Options options, SinkFactory factory = {});
  ~Service();
  Service(const Service&)            = delete;
  Service& operator=(const Service&) = delete;

  // Controller operations. Start/Stop enqueue state changes; disk work never runs on the caller. A zero duration
  // records until stopped. A destination change applies only to a new Service, after the old one is drained.
  bool Start(size_t recorder, std::chrono::milliseconds duration = {});
  bool Stop(size_t recorder);
  bool IsRecording(size_t recorder) const noexcept;
  // Take a ticket before gathering an asynchronous snapshot. Restarted recorders refuse old-session tickets.
  std::optional<CaptureTicket> BeginSnapshot(size_t recorder) const noexcept;
  // Producers should check IsRecording before constructing a snapshot. TrySubmit never waits for the worker or
  // queue lock. It accepts only bounded, owned scalars; a null/unknown observation can use std::monostate.
  SubmitResult TrySubmit(size_t recorder, Event event) noexcept;
  SubmitResult TrySubmit(CaptureTicket ticket, Event event) noexcept;
  SubmitResult Mark(size_t recorder, std::string note) noexcept;
  // Controller-only snapshots; no disk reads. Counters distinguish accepted, written and stream-flushed records,
  // none of which asserts fsync durability, semantic completeness or upload delivery.
  Status              GetStatus(size_t recorder) const;
  std::vector<Status> GetStatus() const;
  // Drains accepted snapshots and joins the worker. Call during controller teardown, never from a game hook.
  void Shutdown();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace diagnostics
