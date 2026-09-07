#include "dev/diagnostics.h"

#ifdef _MODDBG

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace dev::diagnostics
{
namespace
{
  constexpr size_t kCapacity        = 128;
  constexpr size_t kMaxMessageBytes = 2048;

  void bound_message(std::string& message)
  {
    if (message.size() <= kMaxMessageBytes) {
      return;
    }

    auto length = kMaxMessageBytes - 3;
    while (length && (static_cast<unsigned char>(message[length]) & 0xC0U) == 0x80U) {
      --length;
    }
    message.resize(length);
    message += "...";
  }

  struct SourceRecord {
    std::string owner;
    bool        enabled = true;
  };

  struct State {
    std::atomic_bool                      awake{false};
    std::mutex                            mutex;
    std::vector<SourceRecord>             sources{{"invalid", false}};
    std::deque<Entry>                     entries;
    uint64_t                              next_sequence = 1;
    uint64_t                              dropped       = 0;
    std::chrono::steady_clock::time_point started_at    = std::chrono::steady_clock::now();
  };

  State& state()
  {
    static State instance;
    return instance;
  }
} // namespace

SourceId RegisterSource(std::string_view owner, bool enabled)
{
  if (owner.empty()) {
    return invalid_source;
  }

  auto&            current = state();
  std::scoped_lock lock{current.mutex};
  for (SourceId source = 1; source < current.sources.size(); ++source) {
    if (current.sources[source].owner == owner) {
      return source;
    }
  }

  current.sources.push_back({std::string{owner}, enabled});
  return static_cast<SourceId>(current.sources.size() - 1);
}

bool SetSourceEnabled(SourceId source, bool enabled)
{
  auto&            current = state();
  std::scoped_lock lock{current.mutex};
  if (source == invalid_source || source >= current.sources.size()) {
    return false;
  }
  current.sources[source].enabled = enabled;
  return true;
}

bool IsAwake(SourceId source)
{
  auto& current = state();
  if (!current.awake.load(std::memory_order_acquire)) {
    return false;
  }
  if (source == invalid_source) {
    return true;
  }

  std::scoped_lock lock{current.mutex};
  return source < current.sources.size() && current.sources[source].enabled;
}

void SetAwake(bool awake)
{ state().awake.store(awake, std::memory_order_release); }

void Clear()
{
  auto&            current = state();
  std::scoped_lock lock{current.mutex};
  current.entries.clear();
  current.dropped = 0;
}

void Publish(SourceId source, Severity severity, std::string message)
{
  auto& current = state();
  if (!current.awake.load(std::memory_order_acquire)) {
    return;
  }

  std::scoped_lock lock{current.mutex};
  if (source == invalid_source || source >= current.sources.size() || !current.sources[source].enabled) {
    return;
  }

  bound_message(message);

  if (current.entries.size() == kCapacity) {
    current.entries.pop_front();
    ++current.dropped;
  }

  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - current.started_at)
          .count();
  current.entries.push_back(
      {current.next_sequence++, elapsed, source, severity, current.sources[source].owner, std::move(message)});
}

Snapshot ReadSnapshot()
{
  auto& current = state();
  if (!current.awake.load(std::memory_order_acquire)) {
    return {};
  }

  std::scoped_lock lock{current.mutex};
  return {{current.entries.begin(), current.entries.end()}, current.dropped};
}
} // namespace dev::diagnostics

#endif
