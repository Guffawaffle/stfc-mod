/**
 * @file object_tracker_public_stubs.cc
 * @brief Public-release no-op stubs for object-tracker query surfaces.
 */
#include <il2cpp/il2cpp_helper.h>

#include "patches/object_tracker_state.h"

#if defined(STFC_PUBLIC_RELEASE) && STFC_PUBLIC_RELEASE

ObjectTrackerCore<Il2CppClass*, uintptr_t> tracked_objects;

std::vector<TrackedObjectClassSummary> GetTrackedObjectSummary()
{
  return {};
}

std::vector<void*> GetTrackedObjectsForClass(Il2CppClass*)
{
  return {};
}

void* GetLatestTrackedObjectForClass(Il2CppClass*)
{
  return nullptr;
}

#endif
