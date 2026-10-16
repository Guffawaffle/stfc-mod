#include "patches/claim_notice.h"
#include "dev/overlay_runtime.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>

namespace claim_trace
{
namespace
{
std::mutex mutex;
NoticeState state;
dev::overlay::NativeElement background, label;
std::chrono::steady_clock::time_point retryAfter;
std::string displayed;
float lastWidth{}, lastRight{}, lastTop{};

NoticeState Snapshot()
{
  std::lock_guard lock(mutex);
  return state;
}
bool Visible() { return Snapshot().kind != NoticeKind::none; }
void ResetPanel()
{
  dev::overlay::ReleaseElement(label);
  dev::overlay::ReleaseElement(background);
  displayed.clear();
  lastWidth = lastRight = lastTop = 0;
}
void Failed()
{
  ResetPanel();
  retryAfter = std::chrono::steady_clock::now() + std::chrono::seconds(5);
}
void Tick(Transform* host, const dev::overlay::LayoutContext& layout)
{
  using namespace dev::overlay;
  const auto snapshot = Snapshot();
  if (snapshot.kind == NoticeKind::none || std::chrono::steady_clock::now() < retryAfter)
    return;
  if (!IsAlive(background) || !IsAlive(label)) {
    ResetPanel();
    background = CreateElement("CommunityClaimNotice", host, "UnityEngine.UI", "UnityEngine.UI", "Image");
    auto* transform = GetTransform(background);
    SetActive(background, false);
    label = CreateElement("CommunityClaimNoticeText", transform, "Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
    if (!IsAlive(background) || !IsAlive(label) || !SetRaycastTarget(background, false)
        || !ConfigureText(label, 20.0f, 514, {1.0f, 0.90f, 0.65f, 1.0f})
        || !ConfigureRect(label, {0, 0}, {1, 1}, {0.5f, 0.5f}, {-20, -8}, {})) {
      Failed();
      return;
    }
  }
  // Same right inset as the console, below its 300-unit panel and 12-unit margin.
  const float right = std::max(layout.reserved.right, std::min(560.0f, layout.width * 0.16f + 36.0f)) + 24.0f;
  const float width = std::min(380.0f, std::max(0.0f, layout.width - right - 24.0f));
  const float top = std::min(324.0f, std::max(12.0f, layout.height - 96.0f));
  if (width < 220.0f) {
    SetActive(background, false);
    return;
  }
  if (width != lastWidth || right != lastRight || top != lastTop) {
    if (!ConfigureRect(background, {1, 1}, {1, 1}, {1, 1}, {width, 72}, {-right, -top})) {
      Failed();
      return;
    }
    lastWidth = width;
    lastRight = right;
    lastTop = top;
  }
  std::string text;
  switch (snapshot.kind) {
    case NoticeKind::rejected: text = "Claim repro captured\nDONE navigation rejected"; break;
    case NoticeKind::recovered: text = "Claim recovered\nSummary closed; trace recorded"; break;
    case NoticeKind::timeout: text = "Claim timeout captured\nTrace recorded"; break;
    case NoticeKind::marked: text = "Claim repro captured\nIncident marked; trace recorded"; break;
    case NoticeKind::preview: text = "Claim indicator test\nDisplay preview only"; break;
    default: return;
  }
  if (snapshot.injected)
    text += " (TEST)";
  if (text != displayed) {
    const bool recovered = snapshot.kind == NoticeKind::recovered;
    if (!SetText(label, text) || !SetGraphicColor(background, recovered ? Color{0.02f, 0.18f, 0.10f, 0.94f}
                                                                       : Color{0.18f, 0.09f, 0.02f, 0.94f})) {
      Failed();
      return;
    }
    displayed = std::move(text);
  }
  SetActive(background, true);
}
} // namespace

void InstallClaimNotice()
{ dev::overlay::RegisterPanel("claim-capture-notice", Tick, ResetPanel, Visible); }
void ShowClaimNotice(NoticeKind kind, std::uint64_t generation, bool injected)
{
  std::lock_guard lock(mutex);
  state.Capture(kind, generation, injected);
}
void BeginClaimNoticeRecovery(std::uint64_t generation, std::uint64_t navigation, std::int64_t now)
{
  std::lock_guard lock(mutex);
  state.BeginRecovery(generation, navigation, now);
}
void ClaimNoticeRecoveryReturned(std::uint64_t generation, std::uint64_t navigation, bool accepted, std::int64_t now)
{
  std::lock_guard lock(mutex);
  state.Returned(generation, navigation, accepted, now);
}
void ClaimNoticeSummaryHidden(std::uint64_t generation, std::int64_t now)
{
  std::lock_guard lock(mutex);
  state.Hidden(generation, now);
}
void CancelClaimNoticeConfirmation()
{
  std::lock_guard lock(mutex);
  state.CancelConfirmation();
}
void ClearClaimNotice()
{
  std::lock_guard lock(mutex);
  state = {};
}
} // namespace claim_trace
