/**
 * @file notification_queue.h
 * @brief Pure notification queue batching/coalescing helpers.
 */
#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

struct NotificationQueueRequest {
  std::string source;
  std::string title;
  std::string body;
  std::chrono::steady_clock::time_point queued_at;
};

NotificationQueueRequest notification_queue_collapse_batch(std::vector<NotificationQueueRequest>&& batch,
                                                           size_t summary_limit = 4);
std::string notification_queue_batch_preview(const std::vector<NotificationQueueRequest>& batch,
                                             size_t summary_limit = 4);
