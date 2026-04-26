/**
 * @file object_tracker_core.h
 * @brief Pure tracked-object bucket storage and query helper.
 */
#pragma once

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename ClassKey, typename ObjectKey>
class ObjectTrackerCore
{
public:
  struct BucketSnapshot {
    ClassKey class_key{};
    std::vector<ObjectKey> objects;
  };

  void add(ClassKey class_key, ObjectKey object_key)
  {
    buckets_[class_key].push_back(object_key);
  }

  bool remove(ClassKey class_key, ObjectKey object_key)
  {
    auto found = buckets_.find(class_key);
    if (found == buckets_.end()) {
      return false;
    }

    auto& objects = found->second;
    for (auto iter = objects.begin(); iter != objects.end(); ++iter) {
      if (*iter == object_key) {
        objects.erase(iter);
        return true;
      }
    }

    return false;
  }

  size_t remove_object_from_all(ObjectKey object_key)
  {
    size_t removed = 0;
    for (auto& [class_key, objects] : buckets_) {
      (void)class_key;
      for (auto iter = objects.begin(); iter != objects.end();) {
        if (*iter == object_key) {
          iter = objects.erase(iter);
          ++removed;
        } else {
          ++iter;
        }
      }
    }

    return removed;
  }

  std::vector<ObjectKey> objects_for_class(ClassKey class_key) const
  {
    const auto found = buckets_.find(class_key);
    if (found == buckets_.end()) {
      return {};
    }

    return found->second;
  }

  ObjectKey latest_for_class(ClassKey class_key) const
  {
    const auto found = buckets_.find(class_key);
    if (found == buckets_.end() || found->second.empty()) {
      return {};
    }

    return found->second.back();
  }

  std::vector<BucketSnapshot> snapshot() const
  {
    std::vector<BucketSnapshot> buckets;
    buckets.reserve(buckets_.size());

    for (const auto& [class_key, objects] : buckets_) {
      buckets.push_back(BucketSnapshot{class_key, objects});
    }

    return buckets;
  }

  size_t class_count() const
  {
    return buckets_.size();
  }

  size_t object_count() const
  {
    size_t count = 0;
    for (const auto& [class_key, objects] : buckets_) {
      (void)class_key;
      count += objects.size();
    }
    return count;
  }

  void clear()
  {
    buckets_.clear();
  }

private:
  std::unordered_map<ClassKey, std::vector<ObjectKey>> buckets_;
};