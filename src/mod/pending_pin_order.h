#pragma once
#include "mod_state_writer.h"
#include <optional>

namespace pinned_ship_state
{
struct PinOrder {
  std::vector<int64_t>     ids;
  std::vector<std::string> legacy_names;
};
// Owner-thread desired state stays separate from applied native state. Rapid
// input composes against Desired(), and only the exact latest receipt can apply.
class PendingPinOrder
{
public:
  enum class Completion { Waiting, Saved, Failed };
  void Queue(PinOrder order, uint64_t revision)
  {
    desired_  = std::move(order);
    revision_ = revision;
  }
  const PinOrder* Desired() const
  { return desired_ ? &*desired_ : nullptr; }
  Completion Observe(mod_state::SaveStatus status) const
  {
    if (!desired_ || status.revision < revision_ || status.outcome == mod_state::SaveOutcome::Pending)
      return Completion::Waiting;
    if (status.revision != revision_ || status.outcome != mod_state::SaveOutcome::Saved)
      return Completion::Failed;
    return Completion::Saved;
  }
  PinOrder Take()
  {
    auto result = std::move(*desired_);
    desired_.reset();
    revision_ = 0;
    return result;
  }

private:
  std::optional<PinOrder> desired_;
  uint64_t                revision_ = 0;
};
} // namespace pinned_ship_state
