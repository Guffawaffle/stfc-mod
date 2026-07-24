/**
 * @file object_tracker.cc
 * @brief IL2CPP object lifecycle tracker — prevents premature GC of game objects.
 *
 * The game's IL2CPP garbage collector can finalize objects that the mod still
 * holds references to (e.g. ObjectViewer widgets, FleetBarViewController).
 * This module hooks object construction, destruction, and GC liveness
 * calculation to maintain a parallel tracking map keyed by IL2CppClass.
 *
 * Architecture:
 *  - track_ctor: hooks .ctor to register new objects and install a GC finalizer.
 *  - track_destroy / track_free: hooks OnDestroy to remove objects from tracking.
 *  - calc_liveness_hook: runs after the GC liveness pass to evict objects the
 *    GC has marked for collection, keeping our map consistent.
 *  - GC_register_finalizer_inner: resolved via signature scan so we can register
 *    our own C-level GC callback without access to the Boehm GC headers.
 *
 * Thread safety: all map mutations are guarded by tracked_objects_mutex.
 */

#include <il2cpp/il2cpp_helper.h>

#include "patches/object_tracker_state.h"
#include "patches/hook_registry.h"

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

#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <spud/signature.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ─── Tracking State ──────────────────────────────────────────────────────────

std::mutex                                      tracked_objects_mutex;
ObjectTrackerCore<Il2CppClass*, uintptr_t>      tracked_objects;

using FinalizerCallback = void (*)(void* object, void* client_data);

struct PreviousFinalizer {
  FinalizerCallback callback = nullptr;
  void*             data     = nullptr;
};

static constexpr size_t kObjectTrackerMaxClassWalkDepth = 64;
static std::unordered_map<void*, PreviousFinalizer> previous_finalizers;

/// Function pointer resolved at runtime via signature scan (Boehm GC internal).
void (*GC_register_finalizer_inner)(unsigned __int64 obj, void (*fn)(void*, void*), void* cd,
                                    void (**ofn)(void*, void*), void** ocd) = nullptr;

static Il2CppClass* NormalizeClassPointer(Il2CppClass* klass)
{
  return reinterpret_cast<Il2CppClass*>(reinterpret_cast<size_t>(klass) & ~size_t{1});
}

static const char* SafeClassName(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass || !klass->name) {
    return "<unknown>";
  }

  return klass->name;
}

static PreviousFinalizer take_previous_finalizer_locked(void* object)
{
  if (const auto found = previous_finalizers.find(object); found != previous_finalizers.end()) {
    auto previous = found->second;
    previous_finalizers.erase(found);
    return previous;
  }

  return {};
}

static void restore_previous_finalizer(void* object, const PreviousFinalizer& previous)
{
  if (!object || !previous.callback || !GC_register_finalizer_inner) {
    return;
  }

  FinalizerCallback ignoredCallback = nullptr;
  void*             ignoredData     = nullptr;
  GC_register_finalizer_inner(reinterpret_cast<uintptr_t>(object), previous.callback, previous.data, &ignoredCallback,
                              &ignoredData);
}

static std::string ClassPointerToString(const Il2CppClass* klass)
{
  if (!klass) {
    return "";
  }

  char buffer[2 + sizeof(void*) * 2 + 1] = {};
  std::snprintf(buffer, sizeof(buffer), "%p", reinterpret_cast<const void*>(klass));
  return buffer;
}

std::vector<TrackedObjectClassSummary> GetTrackedObjectSummary()
{
  std::scoped_lock lk{tracked_objects_mutex};

  std::vector<TrackedObjectClassSummary> summaries;
  const auto buckets = tracked_objects.snapshot();
  summaries.reserve(buckets.size());

  for (const auto& bucket : buckets) {
    const auto* klass = bucket.class_key;
    if (!klass || bucket.objects.empty()) {
      continue;
    }

    summaries.push_back(TrackedObjectClassSummary{
      ClassPointerToString(klass),
        klass->namespaze ? klass->namespaze : "",
        klass->name ? klass->name : "<unnamed>",
        bucket.objects.size(),
    });
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

std::vector<void*> GetTrackedObjectsForClass(Il2CppClass* klass)
{
  if (!klass) {
    return {};
  }

  std::scoped_lock lk{tracked_objects_mutex};
  const auto       tracked = tracked_objects.objects_for_class(klass);

  std::vector<void*> objects;
  objects.reserve(tracked.size());
  for (const auto pointer : tracked) {
    objects.push_back(reinterpret_cast<void*>(pointer));
  }
  return objects;
}

void* GetLatestTrackedObjectForClass(Il2CppClass* klass)
{
  if (!klass) {
    return nullptr;
  }

  std::scoped_lock lk{tracked_objects_mutex};
  return reinterpret_cast<void*>(tracked_objects.latest_for_class(klass));
}

// ─── Tracking Map Helpers ────────────────────────────────────────────────────

/** @brief Registers an object pointer in the tracking map for its class and all parent classes. */
void add_to_tracking_recursive(Il2CppClass* klass, void* _this)
{
  eastl::unordered_set<Il2CppClass*> visited;
  size_t                             depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped tracking parent classes for {} after {} levels", _this,
                   kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (visited.find(normalized) != visited.end()) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {} while tracking {}", SafeClassName(normalized),
                   _this);
      return;
    }

    visited.emplace(normalized);
    tracked_objects.add(normalized, uintptr_t(_this));
    klass = normalized->parent;
  }
}

/** @brief Removes an object from every class entry in the tracking map (brute-force). */
void remove_from_tracking_all(void* _this)
{
  tracked_objects.remove_object_from_all(uintptr_t(_this));
}

/** @brief Removes an object from tracking by walking the class hierarchy upward. */
void remove_from_tracking_recursive(Il2CppClass* klass, void* _this)
{
  eastl::unordered_set<Il2CppClass*> visited;
  size_t                             depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped removing parent classes for {} after {} levels", _this,
                   kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (visited.find(normalized) != visited.end()) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {} while removing {}", SafeClassName(normalized),
                   _this);
      return;
    }

    visited.emplace(normalized);
    tracked_objects.remove(normalized, uintptr_t(_this));
    klass = normalized->parent;
  }
}

// ─── GC Integration ─────────────────────────────────────────────────────────

/** @brief GC finalizer callback — removes the object from tracking when collected. */
void track_finalizer(void* _this, void*)
{
  PreviousFinalizer previous{};
  bool              has_previous = false;

  {
    std::scoped_lock lk{tracked_objects_mutex};
    const auto*      object = reinterpret_cast<Il2CppObject*>(_this);
    spdlog::trace("Clearing {}({})", _this, object ? SafeClassName(object->klass) : "<null>");
    remove_from_tracking_all(_this);

    previous     = take_previous_finalizer_locked(_this);
    has_previous = previous.callback != nullptr;
  }

  if (has_previous) {
    previous.callback(_this, previous.data);
  }
}

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

/**
 * @brief Hook: T::.ctor (generic for each tracked type)
 *
 * Intercepts object construction to register the new instance in the
 * tracking map and install a GC finalizer that will clean it up.
 * Original method: initializes the managed object.
 * Our modification: adds the object to the tracking map post-construction.
 */
void* track_ctor(auto original, void* _this)
{
  auto obj = original(_this);
  if (_this == nullptr) {
    return _this;
  }

  std::scoped_lock lk{tracked_objects_mutex};
  auto             cls = (Il2CppObject*)_this;
  spdlog::trace("Tracking {}({})", _this, SafeClassName(cls->klass));
  if (!GC_register_finalizer_inner) {
    spdlog::warn("Object tracker cannot register GC finalizer for {}({}); resolver is unavailable", _this,
                 SafeClassName(cls->klass));
    add_to_tracking_recursive(cls->klass, _this);
    return obj;
  }

  FinalizerCallback oldCallback = nullptr;
  void*             oldData     = nullptr;
  GC_register_finalizer_inner(reinterpret_cast<uintptr_t>(_this), track_finalizer, nullptr, &oldCallback, &oldData);
  if (oldCallback && oldCallback != track_finalizer) {
    spdlog::warn("Object tracker is chaining existing GC finalizer for {}({})", _this, SafeClassName(cls->klass));
    previous_finalizers[_this] = PreviousFinalizer{oldCallback, oldData};
  } else {
    previous_finalizers.erase(_this);
    if (oldCallback == track_finalizer) {
      spdlog::warn("Object tracker replaced its own existing GC finalizer for {}({})", _this, SafeClassName(cls->klass));
    }
  }
  add_to_tracking_recursive(cls->klass, _this);
  return obj;
}

/**
 * @brief Hook: T::OnDestroy (generic for each tracked type)
 *
 * Intercepts Unity OnDestroy to eagerly remove the object from tracking
 * before the GC finalizer runs, avoiding stale pointer access.
 */
void track_destroy(auto original, Il2CppObject* _this, uint64_t a2, uint64_t a3)
{
  PreviousFinalizer previous{};
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    spdlog::trace("Clearing {}({})", (void*)_this, SafeClassName(_this->klass));
    remove_from_tracking_all(_this);
    previous = take_previous_finalizer_locked(_this);
  }
  restore_previous_finalizer(_this, previous);
  return original(_this, a2, a3);
}

/** @brief Hook: il2cpp_object_free — removes the object from tracking before deallocation. */
void track_free(auto original, void* _this)
{
  PreviousFinalizer previous{};
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    remove_from_tracking_all(_this);
    previous = take_previous_finalizer_locked(_this);
  }

  restore_previous_finalizer(_this, previous);
  return original(_this);
}

/**
 * @brief Hook: il2cpp_unity_liveness_finalize
 *
 * Runs after the GC liveness calculation. Objects whose klass pointer has
 * the low bit set (IS_MARKED) have been collected — we remove them from
 * the tracking map to prevent dangling references.
 */
void calc_liveness_hook(auto original, void* state)
{
  original(state);

  std::scoped_lock                                    lk{tracked_objects_mutex};
  eastl::vector<eastl::pair<Il2CppClass*, uintptr_t>> objects_to_free;
  eastl::unordered_set<uintptr_t>                     objects_seen;
#define IS_MARKED(obj) (((size_t)(obj)->klass) & (size_t)1)
  for (const auto& bucket : tracked_objects.snapshot()) {
    for (auto object : bucket.objects) {
      if (IS_MARKED((Il2CppObject*)object) && objects_seen.find(object) == objects_seen.end()) {
        objects_to_free.emplace_back(bucket.class_key, object);
        objects_seen.emplace(object);
      }
    }
  }

#undef IS_MARKED

  for (auto& [klass, object] : objects_to_free) {
    spdlog::trace("Clearing {}({})", (void*)object, SafeClassName(klass));
    remove_from_tracking_all((void*)object);
  }
}

// ─── Hook Installation ──────────────────────────────────────────────────────

/// Guards against double-hooking when multiple types share a base method.
static eastl::unordered_set<void*> seen_ctor;
static eastl::unordered_set<void*> seen_destroy;

/**
 * @brief Installs .ctor and OnDestroy hooks for a single tracked type.
 * @tparam T Game class (must expose get_class_helper()).
 *
 * Deduplicates hooks via seen_ctor / seen_destroy so that shared base
 * methods are only detoured once.
 */
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

  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");
  if (!ctor) {
    hooks.record_missing_method(ctor_descriptor);
  } else if (seen_ctor.find(ctor) == seen_ctor.end()) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, ctor_descriptor, ctor, track_ctor);
    seen_ctor.emplace(ctor);
  } else {
    hooks.record_skipped(ctor_descriptor, "shared constructor is already tracked");
  }

  if (!on_destroy) {
    hooks.record_missing_method(destroy_descriptor);
  } else if (seen_destroy.find(on_destroy) == seen_destroy.end()) {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, destroy_descriptor, on_destroy, track_destroy);
    seen_destroy.emplace(on_destroy);
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

/**
 * @brief Registers all tracked game object types, hooks the GC liveness
 *        finalizer, and resolves the Boehm GC internal finalizer registration
 *        function via platform-specific signature scanning.
 */
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

  static constexpr HookDescriptor liveness_descriptor{
      "Il2CppUnityLivenessFinalize", "Evict managed objects marked by the Unity liveness pass",
      {"GameAssembly", "il2cpp", "GC", "il2cpp_unity_liveness_finalize"},
      "ObjectFinder may retain objects after garbage collection", HookSupportTier::Internal};
  HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, liveness_descriptor, il2cpp_unity_liveness_finalize, calc_liveness_hook);

#if _WIN32
  auto GC_register_finalizer_inner_matches =
      spud::find_in_module("40 56 57 41 57 48 83 EC ? 83 3D", "GameAssembly.dll");
#else
#if SPUD_ARCH_ARM64
  auto GC_register_finalizer_inner_matches = spud::find_in_module(
    "FF ? 02 D1 FC 6F ? A9 FA 67 ? A9 F8 5F ? A9 F6 57 ? A9 F4 4F ? A9 FD 7B ? A9 FD ? 02 91 E4 0F ? A9", "GameAssembly.dylib");
#else
  auto GC_register_finalizer_inner_matches = spud::find_in_module(
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC ? 4C 89 45 ? 48 89 4D ? 83 3D", "GameAssembly.dylib");
#endif
#endif

  const auto GC_register_finalizer_inner_match = GC_register_finalizer_inner_matches.get(0);
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))GC_register_finalizer_inner_match.address();
  hooks.log_summary();
}

#undef TRACK_OBJECT
