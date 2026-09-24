#pragma once

// Local science branch only. Inclusive timings; nested categories must not be summed.
#include <chrono>
#include <cstddef>

namespace fleet_perf
{
enum class Part : std::size_t {
  Watch,
  StateSet,
  StateClear,
  FlagSet,
  FlagClear,
  Bind,
  CargoEvent,
  ChildLookup,
  Highlight,
  Eta,
  SampleRead,
  SampleMiss,
  Audio,
  Toast,
  Count
};
bool Enabled();
void Begin();
void End(Part part, std::chrono::steady_clock::time_point start);
void Frame();
void Install();

class Scope
{
  Part                                  part_;
  bool                                  active_;
  std::chrono::steady_clock::time_point start_;

public:
  explicit Scope(Part part)
      : part_(part)
      , active_(Enabled())
  {
    if (active_) {
      Begin();
      start_ = std::chrono::steady_clock::now();
    }
  }
  ~Scope()
  {
    if (active_)
      End(part_, start_);
  }
  Scope(const Scope&)            = delete;
  Scope& operator=(const Scope&) = delete;
};
} // namespace fleet_perf
