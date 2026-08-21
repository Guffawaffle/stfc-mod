/**
 * @file object_tracker_state.h
 * @brief Read-only snapshots of the tracked-object map.
 *
 * These helpers expose stable summaries of the object tracker so runtime
 * diagnostics can inspect tracked classes without depending directly on the
 * tracker implementation details.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

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

template <typename T> TrackedObjectLease<T> GetLatestTrackedObject()
{ return ObjectFinder<T>::Get(); }

template <typename T> std::vector<TrackedObjectLease<T>> GetTrackedObjects()
{ return ObjectFinder<T>::GetAll(); }
