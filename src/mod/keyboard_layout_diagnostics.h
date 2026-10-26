#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace keyboard_layout
{
// Copied diagnostics only: no mapping queries, native subscriptions or persistence.
struct DiagnosticSnapshot {
  std::string layout;
  std::string status     = "physical";
  std::string reason     = "configured_physical";
  uint64_t    generation = 0;
};

class DiagnosticState
{
public:
  bool Publish(std::string_view layout, std::string_view status, std::string_view reason)
  {
    if (snapshot_.layout == layout && snapshot_.status == status && snapshot_.reason == reason)
      return false;
    DiagnosticSnapshot next{std::string(layout), std::string(status), std::string(reason), snapshot_.generation + 1};
    snapshot_ = std::move(next);
    return true;
  }

  const DiagnosticSnapshot& Snapshot() const noexcept
  { return snapshot_; }

  // Reading never acknowledges a generation. A failed vars write must remain eligible for retry.
  bool ChangedSince(uint64_t accepted_generation) const noexcept
  { return snapshot_.generation != accepted_generation; }

private:
  DiagnosticSnapshot snapshot_;
};
} // namespace keyboard_layout
