#pragma once
#include <cstdint>

namespace claim_trace
{
enum class NoticeKind { none, rejected, recovered, timeout, marked, preview };

// UI-only evidence state. A successful return alone never proves recovery.
struct NoticeState {
  NoticeKind kind{NoticeKind::none};
  bool injected{};
  std::uint64_t generation{}, navigation{};
  bool accepted{}, hidden{};
  std::int64_t expires{};

  void Capture(NoticeKind next, std::uint64_t gen = 0, bool test = false)
  {
    *this = {};
    kind = next;
    generation = gen;
    injected = test;
  }
  void BeginRecovery(std::uint64_t gen, std::uint64_t id, std::int64_t now)
  {
    if (kind != NoticeKind::rejected || generation != gen)
      return;
    navigation = id;
    accepted = hidden = false;
    expires = now + 5000;
  }
  void CancelConfirmation() { navigation = 0; }
  void Returned(std::uint64_t gen, std::uint64_t id, bool ok, std::int64_t now)
  {
    if (generation != gen || navigation != id || !navigation)
      return;
    accepted = ok;
    if (!ok || now > expires)
      CancelConfirmation();
    else if (hidden)
      kind = NoticeKind::recovered;
  }
  void Hidden(std::uint64_t gen, std::int64_t now)
  {
    if (generation != gen || !navigation || now > expires)
      return;
    hidden = true;
    if (accepted)
      kind = NoticeKind::recovered;
  }
};

void InstallClaimNotice();
void ShowClaimNotice(NoticeKind kind, std::uint64_t generation = 0, bool injected = false);
void BeginClaimNoticeRecovery(std::uint64_t generation, std::uint64_t navigation, std::int64_t now);
void ClaimNoticeRecoveryReturned(std::uint64_t generation, std::uint64_t navigation, bool accepted, std::int64_t now);
void ClaimNoticeSummaryHidden(std::uint64_t generation, std::int64_t now);
void CancelClaimNoticeConfirmation();
void ClearClaimNotice();
} // namespace claim_trace
