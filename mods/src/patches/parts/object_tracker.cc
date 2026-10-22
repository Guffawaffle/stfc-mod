#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include "patches/object_tracking.h"

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/ArtifactHallDetailsViewController.h"
#include "prime/AssignShipsWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetMeshSelector.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/InventoryListViewController.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/OfficerAssignmentViewController.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ElementSelectorViewController.h"
#include "prime/ShipManagementViewController.h"
#include "prime/ShipManagementScreenStateViewController.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <spud/signature.h>

#include <mutex>

std::mutex                                                   tracked_objects_mutex;
eastl::unordered_map<Il2CppClass*, eastl::vector<uintptr_t>> tracked_objects;
// Metadata pointers and identity tokens only. Entries leave with the tracked
// object; this does not keep a managed wrapper or native Unity object alive.
static eastl::unordered_map<uintptr_t, Il2CppClass*> tracked_memberships;

void ReportTrackedObjectSnapshotIssue(Il2CppClass* klass, size_t total, size_t limit, bool root_failed)
{
  // Snapshot collection has released the registry lock before reporting. Warn
  // once per tracked class, rather than turning an oversized registry into I/O
  // on every UI update. The snapshot's complete()/tracked_count() remain queryable.
  static std::mutex                         warning_mutex;
  static eastl::unordered_set<Il2CppClass*> warned;
  {
    std::scoped_lock lock{warning_mutex};
    if (!warned.insert(klass).second)
      return;
  }
  spdlog::warn("Object tracker: bounded snapshot for {} retained at most {} of {} entries (root/layout failure={})",
               klass ? klass->name : "<unavailable>", limit, total, root_failed);
}

static Il2CppClass* tracked_parent(Il2CppClass* klass)
{ return klass->parent; }

void add_to_tracking_recursive(Il2CppClass* klass, void* _this)
{
  object_tracking::Add(tracked_objects, tracked_memberships, uintptr_t(_this), klass, tracked_parent);
}

void remove_from_tracking_all(void* _this)
{
  object_tracking::Remove(tracked_objects, tracked_memberships, uintptr_t(_this), tracked_parent);
}

// Unity's Boehm GC fork passes a per-entry "finalize mark proc" as a 6th argument to
// GC_register_finalizer_inner. GC_finalize calls it for every unreachable finalized object, and the
// game's own registrations (via GC_register_finalizer_no_order) always pass GC_null_finalize_mark_proc,
// a no-op. If the 6th argument is omitted, register/stack garbage ends up in the finalization entry and
// GC_finalize eventually calls it (observed as a call to nullptr -> SIGSEGV during ReclaimUnusedMemory).
void gc_finalize_mark_proc_noop(void*)
{
}

void (*GC_register_finalizer_inner)(unsigned __int64 obj, void (*fn)(void*, void*), void* cd,
                                    void (**ofn)(void*, void*), void** ocd, void (*mark_proc)(void*)) = nullptr;

void track_finalizer(void* _this, void*)
{
  if (_this == nullptr) {
    return;
  }

  std::scoped_lock lk{tracked_objects_mutex};

  auto object = (Il2CppObject*)_this;
  if (object->klass == nullptr) {
    remove_from_tracking_all(_this);
    return;
  }

#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  spdlog::trace("Clearing {}({})", (void*)_this, GET_CLASS(object->klass)->name);
  remove_from_tracking_all(_this);
#undef GET_CLASS
}

void* track_ctor(auto original, void* _this)
{
  auto obj = original(_this);
  if (_this == nullptr) {
    return _this;
  }

  auto cls = (Il2CppObject*)_this;
  if (cls->klass == nullptr) {
    return obj;
  }

  std::scoped_lock lk{tracked_objects_mutex};
  // A hooked derived/base constructor can observe the same instance twice.
  // Do not duplicate class entries or replace our existing finalizer.
  if (tracked_memberships.find(uintptr_t(_this)) != tracked_memberships.end())
    return obj;
  spdlog::trace("Tracking {}({})", _this, cls->klass->name);
  if (GC_register_finalizer_inner != nullptr) {
    typedef void (*FinalizerCallback)(void* object, void* client_data);
    FinalizerCallback oldCallback = nullptr;
    void*             oldData     = nullptr;
    GC_register_finalizer_inner((intptr_t)_this, track_finalizer, nullptr, &oldCallback, &oldData,
                                gc_finalize_mark_proc_noop);
    assert(!oldCallback);
  }
  add_to_tracking_recursive(cls->klass, _this);
  return obj;
}

void track_destroy(auto original, Il2CppObject* _this, uint64_t a2, uint64_t a3)
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

void track_free(auto original, void* _this)
{
#define GET_CLASS(obj) ((Il2CppClass*)(((size_t)obj) & ~(size_t)1))
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    auto             cls = (Il2CppObject*)_this;
    remove_from_tracking_all(_this);
    return original(_this);
  }
#undef GET_CLASS
}

void artifact_details_bind_hook(auto original, Il2CppObject* _this)
{
  original(_this);
  if (!_this || !_this->klass)
    return;

  std::scoped_lock lk{tracked_objects_mutex};
  remove_from_tracking_all(_this);
  add_to_tracking_recursive(_this->klass, _this);
}

void artifact_details_release_hook(auto original, Il2CppObject* _this)
{
  if (_this) {
    std::scoped_lock lk{tracked_objects_mutex};
    remove_from_tracking_all(_this);
  }
  original(_this);
}

void calc_liveness_hook(auto original, void* state)
{
  original(state);

  std::scoped_lock lk{tracked_objects_mutex};
  object_tracking::Collect(
      tracked_objects, tracked_memberships,
      [](uintptr_t object) { return (uintptr_t(reinterpret_cast<Il2CppObject*>(object)->klass) & uintptr_t{1}) != 0; },
      [](uintptr_t object, Il2CppClass* klass) { spdlog::trace("Clearing {}({})", (void*)object, klass->name); });
}

static eastl::unordered_set<void*> seen_ctor;
static eastl::unordered_set<void*> seen_destroy;

template <typename T> void TrackObject()
{
  auto& object_class = T::get_class_helper();
  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");
  if (seen_ctor.find(ctor) == seen_ctor.end()) {
    SPUD_STATIC_DETOUR(ctor, track_ctor);
    seen_ctor.emplace(ctor);
  }

  if (seen_destroy.find(on_destroy) == seen_destroy.end()) {
    SPUD_STATIC_DETOUR(on_destroy, track_destroy);
    seen_destroy.emplace(on_destroy);
  }
}

void TrackArtifactDetailsCanvas()
{
  auto* cls = ArtifactHallDetailsViewController::get_class_helper().get_cls();
  const auto* bind = method_contract::Resolve(cls, "OnDidBindCanvasContext", false, "System.Void", {});
  const auto* release = method_contract::Resolve(cls, "OnAboutToReleaseCanvasContext", false, "System.Void", {});
  if (!bind || !release || bind->has_full_generic_sharing_signature || release->has_full_generic_sharing_signature) {
    spdlog::warn("Object tracker: artifact details canvas lifecycle unavailable; artifact arrow navigation disabled");
    return;
  }

  const bool bind_installed = SPUD_STATIC_DETOUR(bind->methodPointer, artifact_details_bind_hook);
  const bool release_installed = SPUD_STATIC_DETOUR(release->methodPointer, artifact_details_release_hook);
  if (bind_installed && release_installed)
    spdlog::info("Object tracker: tracking artifact details through canvas lifecycle");
  else
    spdlog::warn("Object tracker: artifact details canvas hooks were not installed; artifact arrow navigation disabled");
}

void InstallObjectTrackers()
{
  TrackObject<PreScanTargetWidget>();
  TrackObject<FleetBarViewController>();
  TrackObject<FleetMeshSelector>();
  TrackObject<AllianceStarbaseObjectViewerWidget>();
  TrackObject<AnimatedRewardsScreenViewController>();
  TrackObject<ArmadaObjectViewerWidget>();
  TrackArtifactDetailsCanvas();
  TrackObject<AssignShipsWidget>();
  TrackObject<CelestialObjectViewerWidget>();
  TrackObject<EmbassyObjectViewer>();
  TrackObject<FullScreenChatViewController>();
  TrackObject<HousingObjectViewerWidget>();
  TrackObject<InventoryListViewController>();
  TrackObject<MiningObjectViewerWidget>();
  TrackObject<MissionsObjectViewerWidget>();
  TrackObject<NavigationInteractionUIViewController>();
  TrackObject<OfficerAssignmentViewController>();
  TrackObject<ElementSelectorViewController>();
  TrackObject<ShipManagementViewController>();
  TrackObject<ShipManagementScreenStateViewController>();
  TrackObject<StarNodeObjectViewerWidget>();

  SPUD_STATIC_DETOUR(il2cpp_unity_liveness_finalize, calc_liveness_hook);

#if defined(__APPLE__) && defined(SPUD_ARCH_ARM64)
  spdlog::warn("Object tracker: macOS ARM64 SPUD indirect-branch fix active; liveness-finalize hook enabled");
#endif

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

  if (GC_register_finalizer_inner_matches.size() == 0) {
    spdlog::warn("Unable to resolve GC_register_finalizer_inner; object finalizers disabled");
    return;
  }

  const auto GC_register_finalizer_inner_match = GC_register_finalizer_inner_matches.get(0);
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))GC_register_finalizer_inner_match.address();
}
