/**
 * @file object_tracker_state.h
 * @brief Read-only snapshots of the tracked-object map.
 *
 * These helpers expose stable summaries of the object tracker so runtime
 * diagnostics can inspect tracked classes without depending directly on the
 * tracker implementation details.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct Il2CppClass;

struct TrackedObjectClassSummary {
  std::string classPointer;
  std::string classNamespace;
  std::string className;
  size_t      count;
};

/**
 * @brief Return a point-in-time snapshot of tracked class buckets and counts.
 */
std::vector<TrackedObjectClassSummary> GetTrackedObjectSummary();

/**
 * @brief Return a point-in-time snapshot of all tracked object pointers for a class.
 * @param klass The tracked Il2CppClass bucket to query.
 * @return Stable copy of the tracked pointer list, oldest to newest.
 */
std::vector<void*> GetTrackedObjectsForClass(Il2CppClass* klass);

/**
 * @brief Return the most recently tracked object pointer for the given class.
 * @param klass The tracked Il2CppClass bucket to query.
 * @return The newest tracked object pointer, or nullptr if none exist.
 */
void* GetLatestTrackedObjectForClass(Il2CppClass* klass);

template <typename T> T* GetLatestTrackedObject()
{
  return reinterpret_cast<T*>(GetLatestTrackedObjectForClass(T::get_class_helper().get_cls()));
}

template <typename T> std::vector<T*> GetTrackedObjects()
{
  const auto objects = GetTrackedObjectsForClass(T::get_class_helper().get_cls());

  std::vector<T*> typed_objects;
  typed_objects.reserve(objects.size());
  for (auto* object : objects) {
    typed_objects.push_back(reinterpret_cast<T*>(object));
  }
  return typed_objects;
}