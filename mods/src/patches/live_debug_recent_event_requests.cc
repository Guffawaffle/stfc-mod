/**
 * @file live_debug_recent_event_requests.cc
 * @brief Pure request/response helpers for the live-debug recent-events command.
 */
#include "patches/live_debug_recent_event_requests.h"

#include <algorithm>

LiveDebugRecentEventStoreQuery live_debug_recent_events_query_from_request(const nlohmann::json& request)
{
  LiveDebugRecentEventStoreQuery query;

  if (const auto after_seq_it = request.find("afterSeq");
      after_seq_it != request.end() && after_seq_it->is_number_integer()) {
    query.afterSeq = after_seq_it->get<int64_t>();
  }

  if (const auto limit_it = request.find("limit");
      limit_it != request.end() && limit_it->is_number_integer()) {
    query.limit = static_cast<size_t>(std::max<int64_t>(0, limit_it->get<int64_t>()));
  } else if (const auto last_it = request.find("last");
             last_it != request.end() && last_it->is_number_integer()) {
    query.limit = static_cast<size_t>(std::max<int64_t>(0, last_it->get<int64_t>()));
  }

  if (const auto kinds_it = request.find("kinds"); kinds_it != request.end() && kinds_it->is_array()) {
    for (const auto& kind : *kinds_it) {
      if (kind.is_string()) {
        query.kinds.push_back(kind.get<std::string>());
      }
    }
  }

  if (const auto kind_it = request.find("kind"); kind_it != request.end() && kind_it->is_string()) {
    if (query.kinds.empty()) {
      query.kind = kind_it->get<std::string>();
    } else {
      const auto kind = kind_it->get<std::string>();
      if (std::find(query.kinds.begin(), query.kinds.end(), kind) == query.kinds.end()) {
        query.kinds.push_back(kind);
      }
    }
  }

  if (const auto match_it = request.find("match"); match_it != request.end() && match_it->is_string()) {
    query.match = match_it->get<std::string>();
  }

  if (const auto exact_it = request.find("exact"); exact_it != request.end() && exact_it->is_boolean()) {
    query.exact = exact_it->get<bool>();
  }

  if (const auto include_details_it = request.find("includeDetails");
      include_details_it != request.end() && include_details_it->is_boolean()) {
    query.includeDetails = include_details_it->get<bool>();
  } else if (const auto summary_it = request.find("summary");
             summary_it != request.end() && summary_it->is_boolean()) {
    query.includeDetails = !summary_it->get<bool>();
  }

  return query;
}

nlohmann::json live_debug_recent_events_result(LiveDebugRecentEventStoreSnapshot snapshot)
{
  return nlohmann::json{{"count", snapshot.count},
                        {"returnedCount", snapshot.returnedCount},
                        {"matchedCount", snapshot.matchedCount},
                        {"capacity", snapshot.capacity},
                        {"firstSeq", snapshot.firstSeq == 0 ? nlohmann::json(nullptr) : nlohmann::json(snapshot.firstSeq)},
                        {"lastSeq", snapshot.lastSeq == 0 ? nlohmann::json(nullptr) : nlohmann::json(snapshot.lastSeq)},
                        {"nextSeq", snapshot.nextSeq},
                        {"evictedCount", snapshot.evictedCount},
                        {"clearCount", snapshot.clearCount},
                        {"queryGap", snapshot.queryGap},
                        {"missingCountBeforeFirstReturned", snapshot.missingCountBeforeFirstReturned},
                        {"kindCounts", std::move(snapshot.kindCounts)},
                        {"bufferKindCounts", std::move(snapshot.bufferKindCounts)},
                        {"events", std::move(snapshot.events)}};
}