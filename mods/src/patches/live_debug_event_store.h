/**
 * @file live_debug_event_store.h
 * @brief Pure recent-event storage for live-debug event history.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

struct LiveDebugRecentEventStoreQuery {
  int64_t     afterSeq = -1;
  size_t      limit = 0;
  std::string kind;
  std::vector<std::string> kinds;
  std::string match;
  bool        exact = false;
  bool        includeDetails = true;
};

struct LiveDebugRecentEventStoreSnapshot {
  size_t         count = 0;
  size_t         capacity = 0;
  size_t         returnedCount = 0;
  size_t         matchedCount = 0;
  uint64_t       firstSeq = 0;
  uint64_t       lastSeq = 0;
  uint64_t       nextSeq = 1;
  uint64_t       evictedCount = 0;
  uint64_t       clearCount = 0;
  uint64_t       missingCountBeforeFirstReturned = 0;
  size_t         queryScanCount = 0;
  size_t         queryTextScanCount = 0;
  bool           queryGap = false;
  bool           queryUsedKindIndex = false;
  nlohmann::json kindCounts = nlohmann::json::object();
  nlohmann::json bufferKindCounts = nlohmann::json::object();
  nlohmann::json events = nlohmann::json::array();
};

class LiveDebugRecentEventStore
{
public:
  struct StoredEvent {
    nlohmann::json       value;
    std::string          kind;
    mutable bool         searchTextCached = false;
    mutable std::string  searchText;
  };

  explicit LiveDebugRecentEventStore(size_t capacity = 256);

  void append(std::string_view kind, nlohmann::json details, int64_t timestamp_ms_utc);
  LiveDebugRecentEventStoreSnapshot snapshot(const LiveDebugRecentEventStoreQuery& query = {}) const;
  size_t clear();

private:
  void rebuild_kind_index() const;

  size_t                     capacity_ = 0;
  uint64_t                   nextSequence_ = 0;
  uint64_t                   evictedCount_ = 0;
  uint64_t                   clearCount_ = 0;
  std::deque<StoredEvent>    events_;
  mutable bool               kindIndexDirty_ = true;
  mutable nlohmann::json     cachedBufferKindCounts_ = nlohmann::json::object();
  mutable std::unordered_map<std::string, std::vector<size_t>> kindIndex_;
};