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
  events_.push_back({nlohmann::json{{"seq", ++nextSequence_},
                                    {"timestampMsUtc", timestamp_ms_utc},
                                    {"kind", kind},
                                    {"details", std::move(details)}},
                     std::string(kind)});
  kindIndexDirty_ = true;

  while (events_.size() > capacity_) {
    ++evictedCount_;
    events_.pop_front();
    kindIndexDirty_ = true;
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

bool query_has_kind_filter(const LiveDebugRecentEventStoreQuery& query)
{
  return !query.kind.empty() || !query.kinds.empty();
}

bool event_matches_kind_query(std::string_view kind, const LiveDebugRecentEventStoreQuery& query)
{
  if (!query.kind.empty() && kind != query.kind) {
    return false;
  }

  if (!query.kinds.empty() &&
      std::find(query.kinds.begin(), query.kinds.end(), kind) == query.kinds.end()) {
    return false;
  }

  return true;
}

void increment_kind_count(nlohmann::json& kind_counts, std::string_view kind)
{
  const auto existing = kind_counts.find(kind);
  if (existing == kind_counts.end()) {
    kind_counts[kind] = 1;
  } else {
    *existing = existing->get<int>() + 1;
  }
}

std::string_view search_text_for_event(const LiveDebugRecentEventStore::StoredEvent& event)
{
  if (!event.searchTextCached) {
    event.searchText = event.value.dump();
    event.searchTextCached = true;
  }

  return event.searchText;
}

bool event_matches_query(const LiveDebugRecentEventStore::StoredEvent& event,
                         const LiveDebugRecentEventStoreQuery& query,
                         size_t& query_text_scan_count)
{
  if (query.afterSeq >= 0) {
    const auto seqIt = event.value.find("seq");
    if (seqIt == event.value.end() || !seqIt->is_number_unsigned()
        || seqIt->get<uint64_t>() <= static_cast<uint64_t>(query.afterSeq)) {
      return false;
    }
  }

  if (!event_matches_kind_query(event.kind, query)) {
    return false;
  }

  if (!query.match.empty()) {
    if (!text_matches_query(event.kind, query)) {
      ++query_text_scan_count;
      if (!text_matches_query(search_text_for_event(event), query)) {
        return false;
      }
    }
  }

  return true;
}

nlohmann::json event_for_query(const LiveDebugRecentEventStore::StoredEvent& event,
                              const LiveDebugRecentEventStoreQuery& query)
{
  if (query.includeDetails) {
    return event.value;
  }

  auto summarized = event.value;
  summarized.erase("details");
  return summarized;
}
}

void LiveDebugRecentEventStore::rebuild_kind_index() const
{
  if (!kindIndexDirty_) {
    return;
  }

  kindIndex_.clear();
  cachedBufferKindCounts_ = nlohmann::json::object();

  for (size_t index = 0; index < events_.size(); ++index) {
    const auto& event = events_[index];
    kindIndex_[event.kind].push_back(index);
    increment_kind_count(cachedBufferKindCounts_, event.kind);
  }

  kindIndexDirty_ = false;
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
    result.firstSeq = events_.front().value.at("seq").get<uint64_t>();
    result.lastSeq = events_.back().value.at("seq").get<uint64_t>();
  }

  if (query.afterSeq >= 0 && result.count > 0) {
    const auto expectedFirstSeq = static_cast<uint64_t>(query.afterSeq) + 1;
    if (result.firstSeq > expectedFirstSeq) {
      result.queryGap = true;
      result.missingCountBeforeFirstReturned = result.firstSeq - expectedFirstSeq;
    }
  }

  rebuild_kind_index();
  result.bufferKindCounts = cachedBufferKindCounts_;

  std::vector<nlohmann::json> matched_events;
  matched_events.reserve(events_.size());

  std::vector<size_t> candidate_indices;
  if (query_has_kind_filter(query)) {
    result.queryUsedKindIndex = true;

    if (!query.kind.empty()) {
      if (const auto kind_it = kindIndex_.find(query.kind); kind_it != kindIndex_.end()) {
        candidate_indices = kind_it->second;
      }
    } else {
      for (const auto& kind : query.kinds) {
        if (const auto kind_it = kindIndex_.find(kind); kind_it != kindIndex_.end()) {
          candidate_indices.insert(candidate_indices.end(), kind_it->second.begin(), kind_it->second.end());
        }
      }

      std::sort(candidate_indices.begin(), candidate_indices.end());
      candidate_indices.erase(std::unique(candidate_indices.begin(), candidate_indices.end()), candidate_indices.end());
    }
  }

  if (candidate_indices.empty() && !result.queryUsedKindIndex) {
    candidate_indices.reserve(events_.size());
    for (size_t index = 0; index < events_.size(); ++index) {
      candidate_indices.push_back(index);
    }
  }

  result.queryScanCount = candidate_indices.size();

  for (const auto index : candidate_indices) {
    const auto& event = events_[index];
    if (event_matches_query(event, query, result.queryTextScanCount)) {
      matched_events.push_back(event_for_query(event, query));
    }
  }

  result.matchedCount = matched_events.size();

  if (query.limit > 0 && matched_events.size() > query.limit) {
    matched_events.erase(matched_events.begin(), matched_events.end() - static_cast<std::ptrdiff_t>(query.limit));
  }

  for (const auto& event : matched_events) {
    result.events.push_back(event);
    const auto kind_it = event.find("kind");
    if (kind_it != event.end() && kind_it->is_string()) {
      increment_kind_count(result.kindCounts, kind_it->get_ref<const std::string&>());
    }
  }

  result.returnedCount = result.events.size();

  return result;
}

size_t LiveDebugRecentEventStore::clear()
{
  const auto cleared = events_.size();
  events_.clear();
  ++clearCount_;
  kindIndexDirty_ = true;
  kindIndex_.clear();
  cachedBufferKindCounts_ = nlohmann::json::object();
  return cleared;
}