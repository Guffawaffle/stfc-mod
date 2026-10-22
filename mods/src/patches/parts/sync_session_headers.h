#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace sync_session
{
struct Snapshot {
  std::string gameServerUrl;
  std::string instanceSessionId;
  std::string primeVersion{"1.000.51336"};
  int32_t     instanceId{};
  uint64_t    generation{1};
};

class Headers
{
public:
  Snapshot Read() const
  {
    std::scoped_lock lock(mutex_);
    return state_;
  }

  uint64_t Generation() const noexcept
  { return generation_.load(std::memory_order_acquire); }

  bool IsCurrent(uint64_t generation) const noexcept
  { return generation == Generation(); }

  void SetServer(std::string url, std::string session)
  {
    std::scoped_lock lock(mutex_);
    if (state_.gameServerUrl == url && state_.instanceSessionId == session)
      return;
    state_.gameServerUrl     = std::move(url);
    state_.instanceSessionId = std::move(session);
    ++state_.generation;
    generation_.store(state_.generation, std::memory_order_release);
  }

  void SetVersion(std::string version)
  {
    std::scoped_lock lock(mutex_);
    state_.primeVersion = std::move(version);
  }

  void SetInstance(int32_t instance)
  {
    std::scoped_lock lock(mutex_);
    state_.instanceId = instance;
  }

private:
  mutable std::mutex   mutex_;
  Snapshot             state_;
  std::atomic_uint64_t generation_{1};
};

// Ordinary processors have process-lifetime de-dup/first-sync state. Once that
// stream is bound, a login change cannot safely resume it without a full reset.
// This policy reports quarantine; the owner closes queues and exposes the gap.
class OrdinarySessionBinding
{
public:
  bool Admit(uint64_t generation, uint64_t currentGeneration) noexcept
  {
    // Bind before checking staleness: even a rejected first task may already
    // have consumed the native caller's once-only initial-snapshot gate.
    uint64_t unset = 0;
    bound_.compare_exchange_strong(unset, generation, std::memory_order_acq_rel);
    Observe(currentGeneration);
    if (!generation || bound_.load(std::memory_order_acquire) != generation
        || observed_.load(std::memory_order_acquire) != generation)
      quarantined_.store(true, std::memory_order_release);
    return !Quarantined();
  }

  bool SessionChanged(uint64_t currentGeneration) noexcept
  {
    Observe(currentGeneration);
    const auto bound = Generation();
    if (bound && bound != observed_.load(std::memory_order_acquire))
      quarantined_.store(true, std::memory_order_release);
    return Quarantined();
  }

  bool Quarantined() const noexcept
  { return quarantined_.load(std::memory_order_acquire); }
  uint64_t Generation() const noexcept
  { return bound_.load(std::memory_order_acquire); }

private:
  void Observe(uint64_t generation) noexcept
  {
    // Header generations increase monotonically. A delayed old admission must
    // not overwrite a session transition observed while the stream was unbound.
    auto prior = observed_.load(std::memory_order_acquire);
    while (prior < generation && !observed_.compare_exchange_weak(prior, generation, std::memory_order_acq_rel)) {}
  }

  std::atomic_uint64_t bound_{0}, observed_{0};
  std::atomic_bool     quarantined_{false};
};
} // namespace sync_session
