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
#include "hook/detour.h"
#include "hook/signature.h"

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

// ─── SPUD Hooks ─────────────────────────────────────────────────────────────

/**
 * @brief Hook: T::.ctor (generic for each tracked type)
 *
 * Intercepts object construction to register the new instance in the
 * tracking map and install a GC finalizer that will clean it up.
 * Original method: initializes the managed object.
 * Our modification: adds the object to the tracking map post-construction.
 *
 * Template-based: each unique target method gets its own slot so that the
 * correct original function is called. We use a runtime counter to assign
 * slots since the number of unique .ctor/.OnDestroy methods depends on
 * how many tracked types share base methods.
 */
using track_ctor_fn = void*(*)(void*);

template<int Slot>
struct TrackCtorHook {
  static inline track_ctor_fn original = nullptr;
  static void* detour(void* _this)
  {
    auto obj = original(_this);
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
};

/**
 * @brief Hook: T::OnDestroy (generic for each tracked type)
 *
 * Intercepts Unity OnDestroy to eagerly remove the object from tracking
 * before the GC finalizer runs, avoiding stale pointer access.
 *
 * Template-based: same slot pattern as TrackCtorHook.
 */
using track_destroy_fn = void(*)(Il2CppObject*, uint64_t, uint64_t);

template<int Slot>
struct TrackDestroyHook {
  static inline track_destroy_fn original = nullptr;
  static void detour(Il2CppObject* _this, uint64_t a2, uint64_t a3)
  {
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
    if (_this != nullptr) {
      std::scoped_lock lk{tracked_objects_mutex};
      spdlog::trace("Clearing {}({})", (void*)_this, GET_CLASS(_this->klass)->name);
      remove_from_tracking_all(_this);
    }
    return original(_this, a2, a3);
#undef GET_CLASS
  }
};

/** @brief Hook: il2cpp_object_free — removes the object from tracking before deallocation. */
MH_HOOK(void, track_free, void* _this)
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
MH_HOOK(void, calc_liveness_hook, void* state)
{
  calc_liveness_hook_original(state);

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

/// Runtime counters to assign template slots at hook installation time.
static int next_ctor_slot    = 0;
static int next_destroy_slot = 0;

/// Max number of unique .ctor / OnDestroy methods we expect to see.
/// There are 13 tracked types; most share base methods. 16 is generous.
static constexpr int kMaxCtorSlots    = 16;
static constexpr int kMaxDestroySlots = 16;

/// Dispatch table: maps runtime slot index → install function.
/// We need this because C++ template args must be compile-time constants,
/// so we pre-instantiate slots 0..15 and dispatch at runtime.
template<int N> struct CtorInstaller {
  static bool install(void* target) {
    MH_ATTACH_SLOT(target, TrackCtorHook, N);
    return true;
  }
};

template<int N> struct DestroyInstaller {
  static bool install(void* target) {
    MH_ATTACH_SLOT(target, TrackDestroyHook, N);
    return true;
  }
};

using install_fn = bool(*)(void*);

/// Pre-instantiated dispatch tables for runtime slot selection.
static install_fn ctor_installers[kMaxCtorSlots] = {
  &CtorInstaller<0>::install,  &CtorInstaller<1>::install,
  &CtorInstaller<2>::install,  &CtorInstaller<3>::install,
  &CtorInstaller<4>::install,  &CtorInstaller<5>::install,
  &CtorInstaller<6>::install,  &CtorInstaller<7>::install,
  &CtorInstaller<8>::install,  &CtorInstaller<9>::install,
  &CtorInstaller<10>::install, &CtorInstaller<11>::install,
  &CtorInstaller<12>::install, &CtorInstaller<13>::install,
  &CtorInstaller<14>::install, &CtorInstaller<15>::install,
};

static install_fn destroy_installers[kMaxDestroySlots] = {
  &DestroyInstaller<0>::install,  &DestroyInstaller<1>::install,
  &DestroyInstaller<2>::install,  &DestroyInstaller<3>::install,
  &DestroyInstaller<4>::install,  &DestroyInstaller<5>::install,
  &DestroyInstaller<6>::install,  &DestroyInstaller<7>::install,
  &DestroyInstaller<8>::install,  &DestroyInstaller<9>::install,
  &DestroyInstaller<10>::install, &DestroyInstaller<11>::install,
  &DestroyInstaller<12>::install, &DestroyInstaller<13>::install,
  &DestroyInstaller<14>::install, &DestroyInstaller<15>::install,
};

/**
 * @brief Installs .ctor and OnDestroy hooks for a single tracked type.
 * @tparam T Game class (must expose get_class_helper()).
 *
 * Deduplicates hooks via seen_ctor / seen_destroy so that shared base
 * methods are only detoured once. Each unique method gets its own
 * template slot via runtime dispatch tables.
 */
template <typename T> void TrackObject()
{
  auto& object_class = T::get_class_helper();
  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");
  if (seen_ctor.find(ctor) == seen_ctor.end()) {
    if (next_ctor_slot >= kMaxCtorSlots) {
      spdlog::critical("FATAL: Ran out of TrackCtorHook slots ({} used)", next_ctor_slot);
      std::abort();
    }
    ctor_installers[next_ctor_slot++](ctor);
    seen_ctor.emplace(ctor);
  }

  if (seen_destroy.find(on_destroy) == seen_destroy.end()) {
    if (next_destroy_slot >= kMaxDestroySlots) {
      spdlog::critical("FATAL: Ran out of TrackDestroyHook slots ({} used)", next_destroy_slot);
      std::abort();
    }
    destroy_installers[next_destroy_slot++](on_destroy);
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

  MH_ATTACH(il2cpp_unity_liveness_finalize, calc_liveness_hook);

#if _WIN32
  auto GC_register_finalizer_inner_matches =
      sig::find_in_module("40 56 57 41 57 48 83 EC ? 83 3D", "GameAssembly.dll");
#else
#if defined(__aarch64__) || defined(_M_ARM64)
  auto GC_register_finalizer_inner_matches = sig::find_in_module(
    "FF 83 02 D1 FC 6F 04 A9 FA 67 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD 7B 09 A9 FD 43 02 91 E4 0F 03 A9", "GameAssembly.dylib");
#else
  auto GC_register_finalizer_inner_matches = sig::find_in_module(
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC ? 4C 89 45 ? 48 89 4D ? 83 3D", "GameAssembly.dylib");
#endif
#endif

  const auto GC_register_finalizer_inner_match = GC_register_finalizer_inner_matches.get(0);
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))GC_register_finalizer_inner_match.address();
}
