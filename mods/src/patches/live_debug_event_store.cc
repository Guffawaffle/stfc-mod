/**
 * @file live_debug_event_store.cc
 * @brief Pure recent-event storage for live-debug event history.
 */
#include "patches/live_debug_event_store.h"

#include "str_utils_pure.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

LiveDebugRecentEventStore::LiveDebugRecentEventStore(size_t capacity)
    : capacity_(capacity)
{
}

void LiveDebugRecentEventStore::append(std::string_view kind, nlohmann::json details, int64_t timestamp_ms_utc)
{
  events_.push_back(nlohmann::json{{"seq", ++nextSequence_},
                                   {"timestampMsUtc", timestamp_ms_utc},
                                   {"kind", kind},
                                   {"details", std::move(details)}});

  while (events_.size() > capacity_) {
    ++evictedCount_;
    events_.pop_front();
  }
}

namespace {
std::string normalize_for_match(std::string_view text)
{
  return AsciiStrToUpper(text);
}

bool wildcard_match_case_insensitive(std::string_view pattern, std::string_view candidate)
{
  const auto normalized_pattern = normalize_for_match(pattern);
  const auto normalized_candidate = normalize_for_match(candidate);

  size_t pattern_index = 0;
  size_t candidate_index = 0;
  size_t star_index = std::string::npos;
  size_t match_index = 0;

  while (candidate_index < normalized_candidate.size()) {
    if (pattern_index < normalized_pattern.size() &&
        (normalized_pattern[pattern_index] == '?' || normalized_pattern[pattern_index] == normalized_candidate[candidate_index])) {
      ++pattern_index;
      ++candidate_index;
      continue;
    }

    if (pattern_index < normalized_pattern.size() && normalized_pattern[pattern_index] == '*') {
      star_index = pattern_index++;
      match_index = candidate_index;
      continue;
    }

    if (star_index != std::string::npos) {
      pattern_index = star_index + 1;
      candidate_index = ++match_index;
      continue;
    }

    return false;
  }

  while (pattern_index < normalized_pattern.size() && normalized_pattern[pattern_index] == '*') {
    ++pattern_index;
  }

  return pattern_index == normalized_pattern.size();
}

bool text_matches_query(std::string_view candidate, const LiveDebugRecentEventStoreQuery& query)
{
  if (query.match.empty()) {
    return true;
  }

  if (query.exact) {
    return normalize_for_match(candidate) == normalize_for_match(query.match);
  }

  if (query.match.find_first_of("*?") != std::string::npos) {
    return wildcard_match_case_insensitive(query.match, candidate);
  }

  return normalize_for_match(candidate).find(normalize_for_match(query.match)) != std::string::npos;
}

bool event_matches_kind_query(const nlohmann::json& event, const LiveDebugRecentEventStoreQuery& query)
{
  const auto kindIt = event.find("kind");
  if (kindIt == event.end() || !kindIt->is_string()) {
    return query.kind.empty() && query.kinds.empty();
  }

  const auto kind = kindIt->get<std::string>();
  if (!query.kind.empty() && kind != query.kind) {
    return false;
  }

  if (!query.kinds.empty() &&
      std::find(query.kinds.begin(), query.kinds.end(), kind) == query.kinds.end()) {
    return false;
  }

  return true;
}

void increment_kind_count(nlohmann::json& kind_counts, const nlohmann::json& event)
{
  const auto kindIt = event.find("kind");
  if (kindIt == event.end() || !kindIt->is_string()) {
    return;
  }

  const auto kind = kindIt->get<std::string>();
  const auto existing = kind_counts.find(kind);
  if (existing == kind_counts.end()) {
    kind_counts[kind] = 1;
  } else {
    *existing = existing->get<int>() + 1;
  }
}

bool event_matches_query(const nlohmann::json& event, const LiveDebugRecentEventStoreQuery& query)
{
  if (query.afterSeq >= 0) {
    const auto seqIt = event.find("seq");
    if (seqIt == event.end() || !seqIt->is_number_unsigned() || seqIt->get<uint64_t>() <= static_cast<uint64_t>(query.afterSeq)) {
      return false;
    }
  }

  if (!event_matches_kind_query(event, query)) {
    return false;
  }

  if (!query.match.empty()) {
    const auto kindIt = event.find("kind");
    const auto eventText = event.dump();
    if (!(kindIt != event.end() && kindIt->is_string() && text_matches_query(kindIt->get<std::string>(), query)) &&
        !text_matches_query(eventText, query)) {
      return false;
    }
  }

  return true;
}

nlohmann::json event_for_query(const nlohmann::json& event, const LiveDebugRecentEventStoreQuery& query)
{
  if (query.includeDetails) {
    return event;
  }

  auto summarized = event;
  summarized.erase("details");
  return summarized;
}
}

LiveDebugRecentEventStoreSnapshot LiveDebugRecentEventStore::snapshot(const LiveDebugRecentEventStoreQuery& query) const
{
  LiveDebugRecentEventStoreSnapshot result;
  result.count = events_.size();
  result.capacity = capacity_;
  result.nextSeq = nextSequence_ + 1;
  result.evictedCount = evictedCount_;
  result.clearCount = clearCount_;

  if (!events_.empty()) {
    result.firstSeq = events_.front().at("seq").get<uint64_t>();
    result.lastSeq = events_.back().at("seq").get<uint64_t>();
  }

  if (query.afterSeq >= 0 && result.count > 0) {
    const auto expectedFirstSeq = static_cast<uint64_t>(query.afterSeq) + 1;
    if (result.firstSeq > expectedFirstSeq) {
      result.queryGap = true;
      result.missingCountBeforeFirstReturned = result.firstSeq - expectedFirstSeq;
    }
  }

  std::vector<nlohmann::json> matched_events;
  matched_events.reserve(events_.size());

  for (const auto& event : events_) {
    increment_kind_count(result.bufferKindCounts, event);

    if (event_matches_query(event, query)) {
      matched_events.push_back(event_for_query(event, query));
    }
  }

  result.matchedCount = matched_events.size();

  if (query.limit > 0 && matched_events.size() > query.limit) {
    matched_events.erase(matched_events.begin(), matched_events.end() - static_cast<std::ptrdiff_t>(query.limit));
  }

  for (const auto& event : matched_events) {
    result.events.push_back(event);
    increment_kind_count(result.kindCounts, event);
  }

  result.returnedCount = result.events.size();

  return result;
}

size_t LiveDebugRecentEventStore::clear()
{
  const auto cleared = events_.size();
  events_.clear();
  ++clearCount_;
  return cleared;
}