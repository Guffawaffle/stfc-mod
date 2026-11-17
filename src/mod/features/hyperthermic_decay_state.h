#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace hyperthermic_decay
{
struct BuffDefinition {
  int64_t id;
  int32_t hostileLevel;
};

// Client 275 static consumable definitions, localization ID 95900. Each hostile
// level has its own timed player buff; do not infer stacks from a kill counter.
inline constexpr std::array<BuffDefinition, 30> kBuffDefinitions{{
    {203039085, 51},  {3368332439, 52}, {2663561780, 53}, {1293548638, 54}, {3256305550, 55}, {2586638873, 56},
    {1241662699, 57}, {1168834081, 58}, {2673430459, 59}, {2386261204, 60}, {810534961, 61},  {1703560322, 62},
    {2568807913, 63}, {3937352501, 64}, {2469432040, 65}, {2052064255, 66}, {1015748123, 67}, {3851400607, 68},
    {2248058859, 69}, {4255576675, 70}, {3543064465, 71}, {4160784207, 72}, {2506118809, 73}, {51364806, 74},
    {199475811, 75},  {3367297219, 76}, {667284369, 77},  {3747009341, 78}, {2094153952, 79}, {77387058, 80},
}};

constexpr const BuffDefinition* FindBuff(int64_t id)
{
  for (const auto& definition : kBuffDefinitions)
    if (definition.id == id)
      return &definition;
  return nullptr;
}

struct Observation {
  int64_t                id{};
  double                 value{}; // Native BuffData.Value: a fraction, not a percentage or stack count.
  bool                   expired{};
  std::optional<int64_t> remainingSeconds;
  std::optional<int32_t> rank{}; // Native rank is not assumed to encode multiplicity.
  std::optional<int64_t> expiryTicks{};
};

struct Row {
  Observation observation;
  int32_t     hostileLevel{};
  size_t      occurrences = 1;
};

struct Rows {
  std::array<std::optional<Row>, kBuffDefinitions.size()> entries{};
  size_t                                                  duplicateEntries{};
  // One displayed row per native ID is a presentation choice. It does not
  // establish a same-level stack cap or the meaning of repeated acquisitions.
  bool multiplicityUnverified = true;
};

struct Summary {
  int32_t                active{}, contributing{}, invalidValues{}, unknownExpiries{};
  double                 decayPercent{}; // Amplifier subtotal only; excludes the Vulcan Ka'athyra artifact.
  bool                   valuesValid = true;
  std::optional<int64_t> nextExpirySeconds;
  size_t                 duplicateEntries{};
  bool                   multiplicityUnverified = true;
};

inline bool IsActive(const Observation& observation)
{ return !observation.expired && (!observation.remainingSeconds || *observation.remainingSeconds > 0); }

// Copies contain no managed pointers. Slots follow hostile-level order even if
// the native list order changes. A later duplicate replaces the earlier value,
// while occurrences preserves evidence of the duplicate instead of summing it.
inline Rows CanonicalRows(std::span<const Observation> observations)
{
  Rows result;
  for (const auto& observation : observations) {
    if (const auto* definition = FindBuff(observation.id)) {
      auto&      entry       = result.entries[static_cast<size_t>(definition - kBuffDefinitions.data())];
      const auto occurrences = entry ? entry->occurrences + 1 : 1;
      result.duplicateEntries += entry.has_value();
      entry = Row{observation, definition->hostileLevel, occurrences};
    }
  }
  return result;
}

inline Rows ActiveRows(std::span<const Observation> observations)
{
  auto result = CanonicalRows(observations);
  for (auto& entry : result.entries)
    if (entry && !IsActive(entry->observation))
      entry.reset();
  return result;
}

// Countdown changes are deliberately excluded. Absolute expiry detects a
// refresh even when the ID and value stay the same; rank is retained as native
// evidence without interpreting it as a stack count. Repeated NaNs compare
// equal so an invalid reading does not cause a new diagnostic every second.
inline bool SameNativeObservation(const Observation& lhs, const Observation& rhs)
{
  return lhs.id == rhs.id && lhs.rank == rhs.rank && lhs.expiryTicks == rhs.expiryTicks && lhs.expired == rhs.expired
         && (lhs.value == rhs.value || (std::isnan(lhs.value) && std::isnan(rhs.value)));
}

inline bool SameNativeSnapshot(const Rows& lhs, const Rows& rhs)
{
  if (lhs.duplicateEntries != rhs.duplicateEntries || lhs.multiplicityUnverified != rhs.multiplicityUnverified)
    return false;
  for (size_t index = 0; index < lhs.entries.size(); ++index) {
    const auto& left  = lhs.entries[index];
    const auto& right = rhs.entries[index];
    if (left.has_value() != right.has_value())
      return false;
    if (left
        && (left->occurrences != right->occurrences || !SameNativeObservation(left->observation, right->observation)))
      return false;
  }
  return true;
}

// Snapshot readiness belongs to the caller: an unreadable list is not empty.
// Counts describe distinct observed IDs, not proven kill or stack multiplicity.
inline Summary SummarizeRows(const Rows& rows)
{
  Summary result;
  double  fraction{};
  result.duplicateEntries       = rows.duplicateEntries;
  result.multiplicityUnverified = rows.multiplicityUnverified;
  for (const auto& row : rows.entries) {
    if (!row || !IsActive(row->observation))
      continue;
    const auto* observation = &row->observation;

    ++result.active;
    if (!observation->remainingSeconds)
      ++result.unknownExpiries;
    else if (!result.nextExpirySeconds || *observation->remainingSeconds < *result.nextExpirySeconds)
      result.nextExpirySeconds = observation->remainingSeconds;

    if (!std::isfinite(observation->value) || observation->value < 0) {
      ++result.invalidValues;
      result.valuesValid = false;
      continue;
    }
    if (observation->value == 0)
      continue;

    ++result.contributing;
    // Guard the final percentage as well as accumulation. A corrupt value must
    // not turn into an infinite displayed percentage or a plausible zero.
    if (observation->value > std::numeric_limits<double>::max() / 100 - fraction)
      result.valuesValid = false;
    else
      fraction += observation->value;
  }
  result.decayPercent = result.valuesValid ? fraction * 100 : 0;
  // An unknown expiry could precede every known one; do not promise a next
  // expiry until all active observations have a countdown.
  if (result.unknownExpiries)
    result.nextExpirySeconds.reset();
  return result;
}

inline Summary Summarize(std::span<const Observation> observations)
{ return SummarizeRows(CanonicalRows(observations)); }
} // namespace hyperthermic_decay
