/**
 * @file notification_queue.cc
 * @brief Pure notification queue batching/coalescing helpers.
 */
#include "patches/notification_queue.h"

#include "patches/notification_text.h"

#include <utility>

NotificationQueueRequest notification_queue_collapse_batch(std::vector<NotificationQueueRequest>&& batch,
                                                           size_t summary_limit)
{
  if (batch.empty()) {
    return {};
  }

  if (batch.size() == 1) {
    return std::move(batch.front());
  }

  NotificationQueueRequest collapsed;
  bool same_title = true;
  for (size_t i = 1; i < batch.size(); ++i) {
    if (batch[i].title != batch.front().title) {
      same_title = false;
      break;
    }
  }

  if (same_title) {
    collapsed.title = batch.front().title + " (" + std::to_string(batch.size()) + ")";
  } else {
    collapsed.title = std::to_string(batch.size()) + " Notifications";
  }

  size_t appended = 0;
  for (size_t i = 0; i < batch.size() && appended < summary_limit; ++i) {
    auto title = notification_flatten_text(batch[i].title);
    auto body  = notification_flatten_text(batch[i].body);

    std::string line;
    if (same_title) {
      line = body.empty() ? title : body;
    } else if (body.empty()) {
      line = title;
    } else {
      line = title + ": " + body;
    }

    if (line.empty()) {
      continue;
    }

    if (!collapsed.body.empty()) {
      collapsed.body += "\n";
    }
    collapsed.body += line;
    ++appended;
  }

  if (batch.size() > appended) {
    if (!collapsed.body.empty()) {
      collapsed.body += "\n";
    }
    collapsed.body += "+" + std::to_string(batch.size() - appended) + " more";
  }

  return collapsed;
}

std::string notification_queue_batch_preview(const std::vector<NotificationQueueRequest>& batch,
                                             size_t summary_limit)
{
  std::string preview;
  size_t appended = 0;

  for (const auto& item : batch) {
    if (appended >= summary_limit) {
      break;
    }

    auto title = notification_flatten_text(item.title);
    if (title.empty()) {
      title = "(untitled)";
    }

    if (!preview.empty()) {
      preview += ", ";
    }
    preview += item.source + ":" + title;
    ++appended;
  }

  if (batch.size() > appended) {
    preview += ", +" + std::to_string(batch.size() - appended) + " more";
  }

  return preview;
}
