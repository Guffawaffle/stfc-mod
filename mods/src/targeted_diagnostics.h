/**
 * @file targeted_diagnostics.h
 * @brief Bounded process-wide capture substrate for disposable diagnostics.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace targeted_diagnostics
{
struct Version {
  int major    = 0;
  int minor    = 0;
  int revision = 0;
};

constexpr bool operator==(const Version& lhs, const Version& rhs)
{ return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.revision == rhs.revision; }

constexpr bool operator<(const Version& lhs, const Version& rhs)
{
  if (lhs.major != rhs.major) {
    return lhs.major < rhs.major;
  }
  if (lhs.minor != rhs.minor) {
    return lhs.minor < rhs.minor;
  }
  return lhs.revision < rhs.revision;
}

constexpr bool operator>=(const Version& lhs, const Version& rhs)
{ return !(lhs < rhs); }

enum class ConcernStatus : uint8_t {
  Temporary,
  Permanent,
};

struct ConcernSpec {
  std::string_view id;
  std::string_view owner;
  std::string_view tracking_issue;
  ConcernStatus    status = ConcernStatus::Temporary;
  Version          introduced_in;
  Version          sunset_at;
  std::string_view remove_or_revise_when;
  std::string_view promotion_criteria;
};

enum class RegistryValidationError : uint8_t {
  None,
  InvalidSpec,
  DuplicateId,
  ExpiredTemporaryConcern,
};

[[nodiscard]] constexpr bool IsValidConcernId(const std::string_view id)
{
  if (id.empty()) {
    return false;
  }

  for (const char ch : id) {
    const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
    if (!valid) {
      return false;
    }
  }
  return id.front() != '-' && id.back() != '-';
}

[[nodiscard]] constexpr bool IsExpired(const ConcernSpec& spec, const Version current_version)
{ return spec.status == ConcernStatus::Temporary && current_version >= spec.sunset_at; }

[[nodiscard]] constexpr bool IsValidConcernSpec(const ConcernSpec& spec)
{
  if (!IsValidConcernId(spec.id) || spec.owner.empty() || spec.tracking_issue.empty()
      || spec.remove_or_revise_when.empty()) {
    return false;
  }

  if (spec.status == ConcernStatus::Temporary) {
    return spec.introduced_in < spec.sunset_at && !spec.promotion_criteria.empty();
  }

  return true;
}

[[nodiscard]] constexpr RegistryValidationError ValidateConcernSpecs(const std::span<const ConcernSpec* const> specs,
                                                                     const Version current_version,
                                                                     const bool    fail_on_expired)
{
  for (size_t index = 0; index < specs.size(); ++index) {
    const auto* spec = specs[index];
    if (!spec || !IsValidConcernSpec(*spec)) {
      return RegistryValidationError::InvalidSpec;
    }
    if (fail_on_expired && IsExpired(*spec, current_version)) {
      return RegistryValidationError::ExpiredTemporaryConcern;
    }

    for (size_t other = index + 1; other < specs.size(); ++other) {
      if (specs[other] && spec->id == specs[other]->id) {
        return RegistryValidationError::DuplicateId;
      }
    }
  }

  return RegistryValidationError::None;
}

enum class ConcernResolution : uint8_t {
  Active,
  Expired,
  Unknown,
};

enum class SubmitStatus : uint8_t {
  Accepted,
  Disabled,
  Expired,
  LockBusy,
  QueueFull,
  RecordTooLarge,
  WriterUnavailable,
};

struct ConcernStats {
  uint64_t accepted             = 0;
  uint64_t dropped_lock_busy    = 0;
  uint64_t dropped_queue_full   = 0;
  uint64_t dropped_shutdown     = 0;
  uint64_t rejected_record_size = 0;
  uint64_t writer_failures      = 0;
  uint64_t rotations            = 0;
};

class Concern
{
public:
  explicit Concern(const ConcernSpec& spec)
      : spec_(&spec)
  {
  }

  Concern(const Concern&)            = delete;
  Concern& operator=(const Concern&) = delete;

  [[nodiscard]] const ConcernSpec& spec() const
  { return *spec_; }
  [[nodiscard]] bool enabled() const
  { return enabled_.load(std::memory_order_relaxed); }
  [[nodiscard]] ConcernStats stats() const;

private:
  friend class Capture;

  const ConcernSpec*   spec_ = nullptr;
  std::atomic_bool     enabled_{false};
  std::atomic_uint64_t sequence_{0};
  std::atomic_uint64_t accepted_{0};
  std::atomic_uint64_t dropped_lock_busy_{0};
  std::atomic_uint64_t dropped_queue_full_{0};
  std::atomic_uint64_t dropped_shutdown_{0};
  std::atomic_uint64_t rejected_record_size_{0};
  std::atomic_uint64_t writer_failures_{0};
  std::atomic_uint64_t rotations_{0};
};

struct CaptureLimits {
  size_t                    global_queue_bytes      = 1024 * 1024;
  size_t                    per_concern_queue_bytes = 256 * 1024;
  size_t                    max_record_bytes        = 32 * 1024;
  std::uintmax_t            max_file_bytes          = 16 * 1024 * 1024;
  int                       total_files             = 4;
  std::chrono::milliseconds shutdown_drain_timeout{750};
};

struct CaptureIdentity {
  std::string downstream_version;
  std::string source_state_id;
  std::string build_class;
};

struct CaptureOptions {
  std::filesystem::path fallback_root;
  std::string           configured_root;
  Version               current_version;
  CaptureLimits         limits;
  CaptureIdentity       identity;
  bool                  start_writer = true;
};

class Payload
{
public:
  virtual ~Payload() = default;

  [[nodiscard]] virtual std::string_view EventType() const noexcept                            = 0;
  [[nodiscard]] virtual uint32_t         SchemaVersion() const noexcept                        = 0;
  [[nodiscard]] virtual size_t           EstimatedQueueBytes() const noexcept                  = 0;
  virtual void                           SerializeFields(nlohmann::ordered_json& fields) const = 0;
};

template <typename Event> struct EventTraits;

template <typename Event>
concept DiagnosticEvent = requires(const std::remove_cvref_t<Event>& event, nlohmann::ordered_json& fields) {
  { EventTraits<std::remove_cvref_t<Event>>::event_type } -> std::convertible_to<std::string_view>;
  { EventTraits<std::remove_cvref_t<Event>>::schema_version } -> std::convertible_to<uint32_t>;
  { EventTraits<std::remove_cvref_t<Event>>::owns_queued_data } -> std::convertible_to<bool>;
  requires EventTraits<std::remove_cvref_t<Event>>::owns_queued_data;
  { EventTraits<std::remove_cvref_t<Event>>::EstimatedQueueBytes(event) } -> std::convertible_to<size_t>;
  EventTraits<std::remove_cvref_t<Event>>::SerializeFields(event, fields);
};

template <DiagnosticEvent Event> class OwnedPayload final : public Payload
{
public:
  explicit OwnedPayload(Event event)
      : event_(std::move(event))
  {
  }

  [[nodiscard]] std::string_view EventType() const noexcept override
  { return EventTraits<Event>::event_type; }
  [[nodiscard]] uint32_t SchemaVersion() const noexcept override
  { return EventTraits<Event>::schema_version; }
  [[nodiscard]] size_t EstimatedQueueBytes() const noexcept override
  { return EventTraits<Event>::EstimatedQueueBytes(event_); }
  void SerializeFields(nlohmann::ordered_json& fields) const override
  { EventTraits<Event>::SerializeFields(event_, fields); }

private:
  Event event_;
};

class Capture
{
public:
  Capture(std::span<Concern* const> registry, std::span<const std::string> enabled_ids, CaptureOptions options);
  ~Capture();

  Capture(const Capture&)            = delete;
  Capture& operator=(const Capture&) = delete;

  [[nodiscard]] ConcernResolution Resolve(std::string_view id) const;
  [[nodiscard]] SubmitStatus      Submit(Concern& concern, std::unique_ptr<Payload> payload);
  void                            Shutdown();
  [[nodiscard]] bool              WaitUntilIdle(std::chrono::milliseconds timeout) const;

  template <DiagnosticEvent Event> [[nodiscard]] SubmitStatus TryWrite(Concern& concern, Event&& event)
  {
    if (!concern.enabled()) {
      return Resolve(concern.spec().id) == ConcernResolution::Expired ? SubmitStatus::Expired : SubmitStatus::Disabled;
    }

    using OwnedEvent = std::remove_cvref_t<Event>;
    return Submit(concern, std::make_unique<OwnedPayload<OwnedEvent>>(std::forward<Event>(event)));
  }

private:
  struct State;
  std::shared_ptr<State> state_;
};

void Initialize(std::span<Concern* const> registry, std::span<const std::string> enabled_ids,
                const CaptureOptions& options);
void Shutdown();

[[nodiscard]] std::shared_ptr<Capture> CurrentCapture();

template <DiagnosticEvent Event> [[nodiscard]] SubmitStatus TryWrite(Concern& concern, Event&& event)
{
  if (!concern.enabled()) {
    return SubmitStatus::Disabled;
  }

  auto capture = CurrentCapture();
  if (!capture) {
    return SubmitStatus::WriterUnavailable;
  }
  return capture->TryWrite(concern, std::forward<Event>(event));
}
} // namespace targeted_diagnostics

#define TARGET_DIAGNOSTIC_ENABLED(concern) ((concern).enabled())
#define TARGET_DIAGNOSTIC_WRITE(concern, ...)                                                                          \
  ([&]() -> ::targeted_diagnostics::SubmitStatus {                                                                     \
    auto& target_diagnostic_concern_ = (concern);                                                                      \
    if (!target_diagnostic_concern_.enabled()) {                                                                       \
      return ::targeted_diagnostics::SubmitStatus::Disabled;                                                           \
    }                                                                                                                  \
    return ::targeted_diagnostics::TryWrite(target_diagnostic_concern_, (__VA_ARGS__));                                \
  }())
#define TARGET_DIAGNOSTIC_REGISTER(concern) &(concern)
