// [ui].hyperthermic_decay_tracker: player-wide Beam Amplifiers, their native
// first-round Decay subtotal, and individual expiries. No detours or Sync uploads.
#include "config.h"
#include "features/features.h"
#include "features/hyperthermic_decay_hud.h"
#include "features/hyperthermic_decay_state.h"

#include "game/hyperthermic_decay.h"

#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
namespace decay_binding = hyperthermic_decay_binding;
using DecayClock        = std::chrono::steady_clock;

struct DecayTracker {
  hyperthermic_decay_hud::Hud                  hud;
  std::vector<hyperthermic_decay::Observation> observations;
  DecayClock::time_point                       next_context{}, next_update{};
  std::optional<hyperthermic_decay::Rows>      previous;
  il2cpp::GcHandle                             manager_root;
  std::string                                  heading, left, right;
  bool                                         enabled = false, in_system = false;

  DecayTracker()
  { observations.reserve(hyperthermic_decay::kBuffDefinitions.size()); }

  bool Read(bool diagnostics)
  {
    observations.clear();
    const auto current = decay_binding::TryCurrentManager();
    auto*      owner   = current ? current.value : nullptr;
    if (owner != manager_root.Get()) {
      previous.reset();
      manager_root = il2cpp::GcHandle::Strong(owner);
    }
    owner = manager_root.Get<decay_binding::Manager>();
    if (!owner)
      return false;
    const auto acquired = owner->TryActiveBuffs();
    auto       root     = il2cpp::GcHandle::Strong(acquired ? acquired.value : nullptr);
    auto*      list     = root.Get<il2cpp::List<decay_binding::BuffData*>>();
    if (!list)
      return false;
    const auto count = list->TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return false;
    il2cpp::CallResult<int64_t> server_time;
    bool                        sampled_time = false;
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item    = list->TryCall<"get_Item", decay_binding::BuffData*>(index);
      auto       element = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
      auto*      buff    = element.Get<decay_binding::BuffData>();
      if (!buff)
        return false;
      const auto id = buff->TryId();
      if (!id)
        return false;
      if (!hyperthermic_decay::FindBuff(id.value))
        continue;
      const auto value  = buff->TryValue();
      const auto expiry = buff->TryExpiryTicks();
      if (!sampled_time && expiry && expiry.value) {
        server_time  = decay_binding::TryServerTicks();
        sampled_time = true;
      }
      // A readable nullable field can prove no expiry. Otherwise retain the
      // native fallback; unavailable expiry is never treated as permanent.
      const auto expired = expiry && (!expiry.value || server_time)
                               ? il2cpp::CallResult<bool>{true, expiry.value && *expiry.value <= server_time.value}
                               : buff->TryHasExpired();
      if (!value || !expired)
        return false;
      const auto             rank = diagnostics ? buff->TryRank() : il2cpp::CallResult<int32_t>{};
      std::optional<int64_t> seconds;
      if (server_time && expiry && expiry.value) {
        const auto ticks = *expiry.value - server_time.value;
        seconds          = ticks / 10'000'000 + (ticks > 0 && ticks % 10'000'000 != 0);
      }
      observations.push_back({id.value, value.value, expired.value, seconds,
                              rank ? std::optional<int32_t>{rank.value} : std::nullopt,
                              expiry ? expiry.value : std::nullopt});
    }
    return true;
  }

  void Trace(const hyperthermic_decay::Rows& rows)
  {
    if (previous && hyperthermic_decay::SameNativeSnapshot(*previous, rows))
      return;
    for (size_t index = 0; index < rows.entries.size(); ++index) {
      const auto& after  = rows.entries[index];
      const auto* before = previous && previous->entries[index] ? &*previous->entries[index] : nullptr;
      if (!after) {
        if (before)
          spdlog::debug("[HyperthermicDecay] event=removed level={}", before->hostileLevel);
        continue;
      }
      const auto& observation = after->observation;
      if (before && before->occurrences == after->occurrences
          && hyperthermic_decay::SameNativeObservation(before->observation, observation))
        continue;
      const char* event = !previous ? "baseline" : !before ? "added" : "updated";
      if (!hyperthermic_decay::IsActive(observation))
        event = "expired";
      else if (before && before->observation.expiryTicks && observation.expiryTicks
               && *observation.expiryTicks > *before->observation.expiryTicks)
        event = "expiry_extended";
      const auto number = [](auto value) { return value ? std::to_string(*value) : "unavailable"; };
      spdlog::debug("[HyperthermicDecay] event={} level={} value={:.3f}% rank={} expires_ticks={} "
                    "previous_expires_ticks={} remaining_s={} observed_entries={}",
                    event, after->hostileLevel, observation.value * 100, number(observation.rank),
                    number(observation.expiryTicks), before ? number(before->observation.expiryTicks) : "none",
                    number(observation.remainingSeconds), after->occurrences);
    }
    const auto summary = hyperthermic_decay::SummarizeRows(rows);
    spdlog::debug("[HyperthermicDecay] active_levels={} contributing_levels={} decay={:.3f}% values_valid={} "
                  "duplicate_entries={}",
                  summary.active, summary.contributing, summary.decayPercent, summary.valuesValid,
                  rows.duplicateEntries);
    previous = rows;
  }

  void Clear()
  {
    hud.Clear();
    observations.clear();
    previous.reset();
    manager_root.Reset();
    next_update = {};
    in_system   = false;
  }

  void Update()
  {
    if (!Config::Get().ui.hyperthermic_decay_tracker) {
      if (enabled) {
        Clear();
        next_context = {};
        enabled      = false;
      }
      return;
    }
    enabled        = true;
    const auto now = DecayClock::now();
    // Ordinary frames do no managed work. Check scene context four times a
    // second, and sample buffs/UI at most once a second while in System view.
    if (now >= next_context) {
      next_context      = now + std::chrono::milliseconds(250);
      const bool system = Hub::CurrentSection() == SectionID::Navigation_System;
      if (!system) {
        if (in_system)
          Clear();
        return;
      }
      in_system = true;
    }
    if (!in_system || now < next_update)
      return;
    next_update = now + std::chrono::seconds(1);
    if (!hud.Prepare()) {
      previous.reset();
      return;
    }
    const bool diagnostics = spdlog::should_log(spdlog::level::debug);
    if (!diagnostics)
      previous.reset();
    const bool anomaly_timer = Config::Get().graphics.galactic_anomaly_timer;
    if (!Read(diagnostics)) {
      // An unreadable list cannot establish that any amplifier is applied.
      hud.Hide();
      if (previous)
        spdlog::debug("[HyperthermicDecay] observation=unavailable");
      previous.reset();
      return;
    }
    const auto canonical = hyperthermic_decay::CanonicalRows(observations);
    if (diagnostics)
      Trace(canonical);
    const auto summary = hyperthermic_decay::SummarizeRows(canonical);
    if (!summary.active) {
      hud.Hide();
      return;
    }
    if (!summary.valuesValid) {
      hud.Show("Buff value unavailable", {}, {}, 0, 0, anomaly_timer);
      return;
    }
    // Reuse three buffers; no per-row strings or intermediate vector/join.
    heading.clear();
    left.clear();
    right.clear();
    const auto count      = static_cast<size_t>(summary.active);
    const auto left_count = count > 8 ? (count + 1) / 2 : count;
    size_t     index      = 0;
    for (const auto& row : canonical.entries) {
      if (!row || !hyperthermic_decay::IsActive(row->observation))
        continue;
      auto& column = index++ < left_count ? left : right;
      if (!column.empty())
        column += '\n';
      const auto& observation = row->observation;
      fmt::format_to(std::back_inserter(column), "Lv {}   {:.0f}%   ", row->hostileLevel, observation.value * 100);
      if (observation.remainingSeconds)
        fmt::format_to(std::back_inserter(column), "{:02}:{:02}", *observation.remainingSeconds / 60,
                       *observation.remainingSeconds % 60);
      else
        column += "--:--";
    }
    fmt::format_to(std::back_inserter(heading), "{:.0f}% · {} contributing", summary.decayPercent,
                   summary.contributing);
    hud.Show(heading, left, right, left_count, count - left_count, anomaly_timer);
  }
};

void HyperthermicDecayFrame()
{
  // Runtime-owned UI roots must not be released by a static destructor after
  // IL2CPP shutdown. Disabled sessions never allocate this owner or touch game data.
  static DecayTracker* tracker = nullptr;
  if (!tracker && !Config::Get().ui.hyperthermic_decay_tracker)
    return;
  if (!tracker)
    tracker = new DecayTracker;
  tracker->Update();
}
} // namespace

const Feature kHyperthermicDecay{"HyperthermicDecay", nullptr, nullptr, HyperthermicDecayFrame};
