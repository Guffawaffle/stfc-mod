#include <il2cpp/il2cpp_helper.h>

#include "patches/hook_registry.h"
#include "patches/object_tracker_state.h"

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/AssignShipsWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/ElementSelectorViewController.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/FleetBarViewController.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/InventoryListViewController.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/OfficerAssignmentViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <cstdio>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
struct TrackedObjectRecord {
  Il2CppGCHandle            weak_handle = nullptr;
  std::vector<Il2CppClass*> classes;
};

static constexpr size_t kObjectTrackerMaxClassWalkDepth = 64;

std::mutex                                                          tracked_objects_mutex;
std::list<TrackedObjectRecord>                                      tracked_objects;
std::unordered_map<Il2CppClass*, std::vector<TrackedObjectRecord*>> tracked_objects_by_class;

Il2CppClass* NormalizeClassPointer(Il2CppClass* klass)
{ return reinterpret_cast<Il2CppClass*>(reinterpret_cast<size_t>(klass) & ~size_t{1}); }

const char* SafeClassName(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass || !klass->name) {
    return "<unknown>";
  }

  return klass->name;
}

std::string ClassPointerToString(const Il2CppClass* klass)
{
  if (!klass) {
    return "";
  }

  char buffer[2 + sizeof(void*) * 2 + 1] = {};
  std::snprintf(buffer, sizeof(buffer), "%p", reinterpret_cast<const void*>(klass));
  return buffer;
}

void AddClassReferencesLocked(TrackedObjectRecord& record, Il2CppClass* klass)
{
  std::unordered_set<Il2CppClass*> visited;
  size_t                           depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped tracking parent classes after {} levels", kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (!visited.emplace(normalized).second) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {}", SafeClassName(normalized));
      return;
    }

    if (std::find(record.classes.begin(), record.classes.end(), normalized) == record.classes.end()) {
      record.classes.emplace_back(normalized);
      tracked_objects_by_class[normalized].emplace_back(&record);
    }

    klass = normalized->parent;
  }
}

void EraseClassReferencesLocked(TrackedObjectRecord& record)
{
  for (auto* klass : record.classes) {
    auto found = tracked_objects_by_class.find(klass);
    if (found == tracked_objects_by_class.end()) {
      continue;
    }

    auto& records = found->second;
    records.erase(std::remove(records.begin(), records.end(), &record), records.end());
    if (records.empty()) {
      tracked_objects_by_class.erase(found);
    }
  }
}

std::list<TrackedObjectRecord>::iterator EraseTrackedObjectLocked(std::list<TrackedObjectRecord>::iterator record)
{
  EraseClassReferencesLocked(*record);
  if (record->weak_handle) {
    il2cpp_gchandle_free(record->weak_handle);
  }
  return tracked_objects.erase(record);
}

void PruneDeadObjectsLocked()
{
  for (auto record = tracked_objects.begin(); record != tracked_objects.end();) {
    if (!record->weak_handle || !il2cpp_gchandle_get_target(record->weak_handle)) {
      record = EraseTrackedObjectLocked(record);
    } else {
      ++record;
    }
  }
}

TrackedObjectRecord* FindTrackedObjectLocked(Il2CppObject* object)
{
  for (auto& record : tracked_objects) {
    if (record.weak_handle && il2cpp_gchandle_get_target(record.weak_handle) == object) {
      return &record;
    }
  }
  return nullptr;
}

void RemoveTrackedObjectLocked(Il2CppObject* object)
{
  for (auto record = tracked_objects.begin(); record != tracked_objects.end(); ++record) {
    if (record->weak_handle && il2cpp_gchandle_get_target(record->weak_handle) == object) {
      EraseTrackedObjectLocked(record);
      return;
    }
  }
}
} // namespace

namespace object_tracker
{
ObjectLeaseHandle AcquireLatest(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass) {
    return {};
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  const auto found = tracked_objects_by_class.find(klass);
  if (found == tracked_objects_by_class.end()) {
    return {};
  }

  for (auto record = found->second.rbegin(); record != found->second.rend(); ++record) {
    auto* target = il2cpp_gchandle_get_target((*record)->weak_handle);
    if (target) {
      auto handle = il2cpp_gchandle_new(target, false);
      if (handle) {
        return ObjectLeaseHandle{handle};
      }
    }
  }

  return {};
}

std::vector<ObjectLeaseHandle> AcquireAll(Il2CppClass* klass)
{
  std::vector<ObjectLeaseHandle> leases;

  klass = NormalizeClassPointer(klass);
  if (!klass) {
    return leases;
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  const auto found = tracked_objects_by_class.find(klass);
  if (found == tracked_objects_by_class.end()) {
    return leases;
  }

  leases.reserve(found->second.size());
  for (auto* record : found->second) {
    auto* target = il2cpp_gchandle_get_target(record->weak_handle);
    if (target) {
      auto handle = il2cpp_gchandle_new(target, false);
      if (handle) {
        leases.emplace_back(handle);
      }
    }
  }

  return leases;
}
} // namespace object_tracker

std::vector<TrackedObjectClassSummary> GetTrackedObjectSummary()
{
  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  std::vector<TrackedObjectClassSummary> summaries;
  summaries.reserve(tracked_objects_by_class.size());
  for (const auto& [klass, records] : tracked_objects_by_class) {
    if (!klass || records.empty()) {
      continue;
    }

    summaries.push_back(TrackedObjectClassSummary{
        ClassPointerToString(klass), klass->namespaze ? klass->namespaze : "", klass->name ? klass->name : "<unnamed>",
        records.size()});
  }

  std::sort(summaries.begin(), summaries.end(), [](const auto& left, const auto& right) {
    if (left.count != right.count) {
      return left.count > right.count;
    }
    if (left.classNamespace != right.classNamespace) {
      return left.classNamespace < right.classNamespace;
    }
    if (left.className != right.className) {
      return left.className < right.className;
    }
    return left.classPointer < right.classPointer;
  });

  return summaries;
}

void* track_ctor(auto original, void* _this)
{
  auto result = original(_this);
  if (!_this) {
    return result;
  }

  auto* object = reinterpret_cast<Il2CppObject*>(_this);
  if (!object->klass) {
    return result;
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  if (auto* existing = FindTrackedObjectLocked(object)) {
    AddClassReferencesLocked(*existing, object->klass);
    return result;
  }

  auto weak_handle = il2cpp_gchandle_new_weakref(object, false);
  if (!weak_handle) {
    spdlog::warn("Object tracker could not create a weak GC handle for {}({})", _this, SafeClassName(object->klass));
    return result;
  }

  spdlog::trace("Tracking {}({})", _this, SafeClassName(object->klass));
  tracked_objects.emplace_back(TrackedObjectRecord{weak_handle});
  AddClassReferencesLocked(tracked_objects.back(), object->klass);
  return result;
}

void track_destroy(auto original, Il2CppObject* _this, uint64_t a2, uint64_t a3)
{
  if (_this) {
    std::scoped_lock lock{tracked_objects_mutex};
    spdlog::trace("Clearing {}({})", static_cast<void*>(_this), SafeClassName(_this->klass));
    RemoveTrackedObjectLocked(_this);
  }

  return original(_this, a2, a3);
}

static std::unordered_set<void*> seen_ctor;
static std::unordered_set<void*> seen_destroy;

template <typename T>
void TrackObject(HookModuleHealth& hooks, const HookDescriptor& ctor_descriptor,
                 const HookDescriptor& destroy_descriptor)
{
  auto& object_class = T::get_class_helper();
  if (!object_class.isValidHelper()) {
    hooks.record_missing_helper(ctor_descriptor);
    hooks.record_missing_helper(destroy_descriptor);
    return;
  }

  auto ctor       = object_class.GetMethod(".ctor");
  auto on_destroy = object_class.GetMethod("OnDestroy");
  if (!ctor) {
    hooks.record_missing_method(ctor_descriptor);
  } else if (seen_ctor.emplace(ctor).second) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, ctor_descriptor, ctor, track_ctor);
  } else {
    hooks.record_skipped(ctor_descriptor, "shared constructor is already tracked");
  }

  if (!on_destroy) {
    hooks.record_missing_method(destroy_descriptor);
  } else if (seen_destroy.emplace(on_destroy).second) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, destroy_descriptor, on_destroy, track_destroy);
  } else {
    hooks.record_skipped(destroy_descriptor, "shared OnDestroy method is already tracked");
  }
}

#define TRACK_OBJECT(hooks, type, namespc, class_name) \
  TrackObject<type>((hooks), \
                    HookDescriptor{#type ".ctor", "Track constructed managed objects", \
                                   {"Assembly-CSharp", (namespc), (class_name), ".ctor"}, \
                                   "ObjectFinder cannot discover newly constructed objects", HookSupportTier::Internal}, \
                    HookDescriptor{#type ".OnDestroy", "Remove destroyed managed objects from tracking", \
                                   {"Assembly-CSharp", (namespc), (class_name), "OnDestroy"}, \
                                   "ObjectFinder may retain destroyed objects", HookSupportTier::Internal})

void InstallObjectTrackers()
{
  HookModuleHealth hooks("ObjectTrackerHooks");

  TRACK_OBJECT(hooks, PreScanTargetWidget, "Digit.Prime.ObjectViewer", "PreScanTargetWidget");
  TRACK_OBJECT(hooks, FleetBarViewController, "Digit.Prime.FleetManagement", "FleetBarViewController");
  TRACK_OBJECT(hooks, AllianceStarbaseObjectViewerWidget, "Digit.Prime.ObjectViewer",
               "AllianceStarbaseObjectViewerWidget");
  TRACK_OBJECT(hooks, AnimatedRewardsScreenViewController, "Digit.Prime.Rewards",
               "AnimatedRewardsScreenViewController");
  TRACK_OBJECT(hooks, ArmadaObjectViewerWidget, "Digit.Prime.ObjectViewer", "ArmadaObjectViewerWidget");
  TRACK_OBJECT(hooks, AssignShipsWidget, "Digit.Prime.Ships", "AssignShipsWidget");
  TRACK_OBJECT(hooks, CelestialObjectViewerWidget, "Digit.Prime.ObjectViewer", "CelestialObjectViewerWidget");
  TRACK_OBJECT(hooks, ElementSelectorViewController, "Digit.Prime.UI", "ElementSelectorViewController");
  TRACK_OBJECT(hooks, EmbassyObjectViewer, "Digit.Prime.ObjectViewer", "EmbassyObjectViewer");
  TRACK_OBJECT(hooks, FullScreenChatViewController, "Digit.Prime.Chat", "FullScreenChatViewController");
  TRACK_OBJECT(hooks, HousingObjectViewerWidget, "Digit.Prime.ObjectViewer", "HousingObjectViewerWidget");
  TRACK_OBJECT(hooks, InventoryListViewController, "Digit.Prime.Inventories", "InventoryListViewController");
  TRACK_OBJECT(hooks, MiningObjectViewerWidget, "Digit.Prime.ObjectViewer", "MiningObjectViewerWidget");
  TRACK_OBJECT(hooks, MissionsObjectViewerWidget, "Digit.Prime.ObjectViewer", "MissionsObjectViewerWidget");
  TRACK_OBJECT(hooks, NavigationInteractionUIViewController, "Digit.Prime.Navigation",
               "NavigationInteractionUIViewController");
  TRACK_OBJECT(hooks, OfficerAssignmentViewController, "Digit.Prime.OfficerAssignment",
               "OfficerAssignmentViewController");
  TRACK_OBJECT(hooks, StarNodeObjectViewerWidget, "Digit.Prime.ObjectViewer", "StarNodeObjectViewerWidget");

  spdlog::info("Object tracker: using weak IL2CPP GC handles with leased snapshots");
  hooks.log_summary();
}

#undef TRACK_OBJECT
