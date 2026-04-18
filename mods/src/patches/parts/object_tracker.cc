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

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/FleetBarViewController.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <spdlog/spdlog.h>
#include <hook/hook.h>
#include <hook/pattern.h>

#include <mutex>

// ─── Tracking State ──────────────────────────────────────────────────────────

std::mutex                                                   tracked_objects_mutex;
eastl::unordered_map<Il2CppClass*, eastl::vector<uintptr_t>> tracked_objects;

// ─── Tracking Map Helpers ────────────────────────────────────────────────────

/** @brief Registers an object pointer in the tracking map for its class and all parent classes. */
void add_to_tracking_recursive(Il2CppClass* klass, void* _this)
{
  if (!klass) {
    return;
  }

  auto& tracked_object_vector = tracked_objects[klass];
  tracked_object_vector.emplace_back(uintptr_t(_this));

  return add_to_tracking_recursive(klass->parent, _this);
}

/** @brief Removes an object from every class entry in the tracking map (brute-force). */
void remove_from_tracking_all(void* _this)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  for (auto& [klass, tracked_object_vector] : tracked_objects) {
    tracked_object_vector.erase_first(uintptr_t(_this));
  }
#undef GET_CLASS
}

/** @brief Removes an object from tracking by walking the class hierarchy upward. */
void remove_from_tracking_recursive(Il2CppClass* klass, void* _this)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  if (!GET_CLASS(klass)) {
    return;
  }

  if (tracked_objects.find(klass) == tracked_objects.end()) {
    return;
  }

  auto& tracked_object_vector = tracked_objects[GET_CLASS(klass->parent)];
  tracked_object_vector.erase_first(uintptr_t(_this));
  return remove_from_tracking_recursive(GET_CLASS(klass->parent), _this);
#undef GET_CLASS
}

// ─── GC Integration ─────────────────────────────────────────────────────────

/// Function pointer resolved at runtime via signature scan (Boehm GC internal).
void (*GC_register_finalizer_inner)(unsigned __int64 obj, void (*fn)(void*, void*), void* cd,
                                    void (**ofn)(void*, void*), void** ocd) = nullptr;

/** @brief GC finalizer callback — removes the object from tracking when collected. */
void track_finalizer(void* _this, void*)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  spdlog::trace("Clearing {}({})", (void*)_this, GET_CLASS(((Il2CppObject*)_this)->klass)->name);
  remove_from_tracking_all(_this);
#undef GET_CLASS
}

// ─── MinHook Hooks ─────────────────────────────────────────────────────────

/// Per-type original function pointers for template hooks.
template <typename T>
struct TrackHooks {
  static inline void* (*ctor_original)(void*)                               = nullptr;
  static inline void (*destroy_original)(Il2CppObject*, uint64_t, uint64_t) = nullptr;
};

template <typename T>
void* track_ctor(void* _this)
{
  auto obj = TrackHooks<T>::ctor_original(_this);
  if (_this == nullptr) {
    return _this;
  }

  std::scoped_lock lk{tracked_objects_mutex};
  auto             cls = (Il2CppObject*)_this;
  spdlog::trace("Tracking {}({})", _this, cls->klass->name);
  typedef void      (*FinalizerCallback)(void* object, void* client_data);
  FinalizerCallback oldCallback = nullptr;
  void*             oldData     = nullptr;
  GC_register_finalizer_inner((intptr_t)_this, track_finalizer, nullptr, &oldCallback, &oldData);
  assert(!oldCallback);
  add_to_tracking_recursive(cls->klass, _this);
  return obj;
}

/**
 * @brief Hook: T::OnDestroy (generic for each tracked type)
 *
 * Intercepts Unity OnDestroy to eagerly remove the object from tracking
 * before the GC finalizer runs, avoiding stale pointer access.
 */
template <typename T>
void track_destroy(Il2CppObject* _this, uint64_t a2, uint64_t a3)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    spdlog::trace("Clearing {}({})", (void*)_this, GET_CLASS(_this->klass)->name);
    remove_from_tracking_all(_this);
  }
  return TrackHooks<T>::destroy_original(_this, a2, a3);
#undef GET_CLASS
}

typedef void (*track_free_fn)(void*);
static track_free_fn track_free_original = nullptr;

void track_free(void* _this)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    auto             cls = (Il2CppObject*)_this;
    remove_from_tracking_all(_this);
    return track_free_original(_this);
  }
#undef GET_CLASS
}

/**
 * @brief Hook: il2cpp_unity_liveness_finalize
 *
 * Runs after the GC liveness calculation. Objects whose klass pointer has
 * the low bit set (IS_MARKED) have been collected — we remove them from
 * the tracking map to prevent dangling references.
 */
typedef void (*calc_liveness_fn)(void*);
static calc_liveness_fn calc_liveness_original = nullptr;

void calc_liveness_hook(void* state)
{
  calc_liveness_original(state);

  std::scoped_lock                                    lk{tracked_objects_mutex};
  eastl::vector<eastl::pair<Il2CppClass*, uintptr_t>> objects_to_free;
  eastl::unordered_set<uintptr_t>                     objects_seen;
#define IS_MARKED(obj) (((size_t)(obj)->klass) & (size_t)1)
  for (auto& [klass, objects] : tracked_objects) {
    for (auto object : objects) {
      if (IS_MARKED((Il2CppObject*)object) && objects_seen.find(object) == objects_seen.end()) {
        objects_to_free.emplace_back(klass, object);
        objects_seen.emplace(object);
      }
    }
  }

#undef IS_MARKED

#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  for (auto& [klass, object] : objects_to_free) {
    spdlog::trace("Clearing {}({})", (void*)object, GET_CLASS(klass)->name);
    remove_from_tracking_all((void*)object);
  }
#undef GET_CLASS

  tracked_objects = tracked_objects;
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
template <typename T> void TrackObject()
{
  auto& object_class = T::get_class_helper();
  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");
  if (seen_ctor.find(ctor) == seen_ctor.end()) {
    MH_INSTALL(ctor, track_ctor<T>, TrackHooks<T>::ctor_original);
    seen_ctor.emplace(ctor);
  }

  if (seen_destroy.find(on_destroy) == seen_destroy.end()) {
    MH_INSTALL(on_destroy, track_destroy<T>, TrackHooks<T>::destroy_original);
    seen_destroy.emplace(on_destroy);
  }
}

/**
 * @brief Registers all tracked game object types, hooks the GC liveness
 *        finalizer, and resolves the Boehm GC internal finalizer registration
 *        function via platform-specific signature scanning.
 */
void InstallObjectTrackers()
{
  TrackObject<PreScanTargetWidget>();
  TrackObject<FleetBarViewController>();
  TrackObject<AllianceStarbaseObjectViewerWidget>();
  TrackObject<AnimatedRewardsScreenViewController>();
  TrackObject<ArmadaObjectViewerWidget>();
  TrackObject<CelestialObjectViewerWidget>();
  TrackObject<EmbassyObjectViewer>();
  TrackObject<FullScreenChatViewController>();
  TrackObject<HousingObjectViewerWidget>();
  TrackObject<MiningObjectViewerWidget>();
  TrackObject<MissionsObjectViewerWidget>();
  TrackObject<NavigationInteractionUIViewController>();
  TrackObject<StarNodeObjectViewerWidget>();

  MH_INSTALL(il2cpp_unity_liveness_finalize, calc_liveness_hook, calc_liveness_original);

#if _WIN32
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))pattern::find_in_module(
      "40 56 57 41 57 48 83 EC ? 83 3D", "GameAssembly.dll");
#else
#if defined(__aarch64__) || defined(__arm64__)
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))pattern::find_in_module(
      "FF 83 02 D1 FC 6F 04 A9 FA 67 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD 7B 09 A9 FD 43 02 91 E4 0F 03 A9",
      "GameAssembly.dylib");
#else
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))pattern::find_in_module(
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC ? 4C 89 45 ? 48 89 4D ? 83 3D",
      "GameAssembly.dylib");
#endif
#endif
}
