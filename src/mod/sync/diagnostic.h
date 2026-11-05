#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>

namespace data_sync
{
inline constexpr size_t kDiagnosticCount     = 64;
inline constexpr size_t kDiagnosticBytes     = 4 * 1024 * 1024;
inline constexpr size_t kDiagnosticItemBytes = 256 * 1024;

// Bounds concurrent dispatch ownership. Destination queues independently retain
// their own charges through HTTP; shared bodies are not claimed as freed here.
class DiagnosticBudget
{
public:
  bool Reserve(size_t bytes) noexcept
  {
    if (!bytes || bytes > kDiagnosticBytes)
      return false;
    auto count = count_.load(std::memory_order_acquire);
    do {
      if (count >= kDiagnosticCount)
        return false;
    } while (!count_.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel));
    auto retained = bytes_.load(std::memory_order_acquire);
    do {
      if (retained > kDiagnosticBytes - bytes) {
        count_.fetch_sub(1, std::memory_order_release);
        return false;
      }
    } while (!bytes_.compare_exchange_weak(retained, retained + bytes, std::memory_order_acq_rel));
    return true;
  }

  void Release(size_t bytes) noexcept
  {
    bytes_.fetch_sub(bytes, std::memory_order_release);
    count_.fetch_sub(1, std::memory_order_release);
  }

  size_t Count() const noexcept
  { return count_.load(std::memory_order_acquire); }
  size_t Bytes() const noexcept
  { return bytes_.load(std::memory_order_acquire); }

private:
  std::atomic_size_t count_{0}, bytes_{0};
};

struct DiagnosticCharge {
  DiagnosticBudget* budget;
  size_t            bytes;
  DiagnosticCharge(DiagnosticBudget& owner, size_t charge) noexcept
      : budget(&owner)
      , bytes(charge)
  {
  }
  DiagnosticCharge(const DiagnosticCharge&)            = delete;
  DiagnosticCharge& operator=(const DiagnosticCharge&) = delete;
  DiagnosticCharge(DiagnosticCharge&& other) noexcept
      : budget(std::exchange(other.budget, nullptr))
      , bytes(other.bytes)
  {
  }
  ~DiagnosticCharge()
  {
    if (budget)
      budget->Release(bytes);
  }
};

struct DiagnosticPayload {
  std::string body;
  explicit DiagnosticPayload(std::string bytes) noexcept
      : body(std::move(bytes))
  {
  }
};

namespace diagnostic_detail
{
  // Stop serialization at the wire-size bound instead of copying an arbitrarily
  // large payload into an envelope and checking only after dump() has expanded it.
  class Buffer : public std::streambuf
  {
  public:
    std::string Take()
    { return std::move(text_); }

  protected:
    std::streamsize xsputn(const char* text, std::streamsize count) override
    {
      if (count < 0 || static_cast<size_t>(count) > kDiagnosticItemBytes - text_.size())
        throw std::length_error("diagnostic wire limit");
      text_.append(text, static_cast<size_t>(count));
      return count;
    }

    int_type overflow(int_type value) override
    {
      if (traits_type::eq_int_type(value, traits_type::eof()))
        return traits_type::not_eof(value);
      const char character = traits_type::to_char_type(value);
      xsputn(&character, 1);
      return value;
    }

  private:
    std::string text_;
  };

  inline bool BoundedString(const nlohmann::json& event, const char* field)
  {
    const auto found = event.find(field);
    return found == event.end()
           || (found->is_string() && found->get_ref<const std::string&>().size() <= kDiagnosticItemBytes);
  }
} // namespace diagnostic_detail

// Legacy schema1, exact unsigned sequence and integer UTC rendered as decimal
// strings. This adapter activates no producer and retains the original payload.
// Inputs remain borrowed only for this synchronous serialization operation.
inline std::optional<std::string> DiagnosticEnvelope(std::string_view stream, const nlohmann::json& event,
                                                     std::string_view source_id, std::string_view version) noexcept
{
  using json = nlohmann::json;
  try {
    if (!event.is_object() || stream.size() > kDiagnosticItemBytes || source_id.size() > kDiagnosticItemBytes
        || version.size() > kDiagnosticItemBytes || !diagnostic_detail::BoundedString(event, "session")
        || !diagnostic_detail::BoundedString(event, "event") || !diagnostic_detail::BoundedString(event, "phase"))
      return std::nullopt;
    const auto sequence = event.find("seq");
    if (sequence == event.end() || !sequence->is_number_unsigned())
      return std::nullopt;
    json       metadata = {{"type", "diagnostic"},
                           {"schema", 1},
                           {"source_id", source_id},
                           {"producer_version", version},
                           {"session", event.value("session", std::string{})},
                           {"stream", stream},
                           {"sequence", sequence->dump()},
                           {"event", event.value("event", event.value("phase", std::string{"observation"}))},
                           {"payload", nullptr}};
    const auto utc      = event.find("utc_ms");
    metadata["utc_ms"]  = utc != event.end() && utc->is_number_integer() ? json(utc->dump()) : json(nullptr);
    diagnostic_detail::Buffer buffer;
    std::ostream              output(&buffer);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << "[{";
    bool first = true;
    for (const auto& [key, value] : metadata.items()) {
      if (!first)
        output << ',';
      first = false;
      output << json(key) << ':';
      if (key == "payload")
        output << event;
      else
        output << value;
    }
    output << "}]";
    return buffer.Take();
  } catch (...) {
    return std::nullopt;
  }
}
} // namespace data_sync
