#include "config.h"
#include "errormsg.h"
#include "patches/orbit_constraints.h"
#include "patches/native_hook_extent.h"
#include <il2cpp/method_contract.h>

#include <il2cpp/il2cpp_helper.h>

#include <prime/Hub.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <utility>

// Station-interior zoom-out.
//
// The interior view is a BlendFrameProvider between a StaticFrameProvider (zoomed-in) and an
// OrbitFrameProvider pinned at a fixed radius (zoomed-out, min==max); zoom slides the blend ratio, so
// the far stop is that orbit endpoint's radius. We push that pinned radius outward to extend zoom-out.
// That same OrbitFrameProvider instance IS the exterior station camera (the interior's fully-zoomed-out
// end is the exterior view), so the hook widens it only while the section is Starbase_Interior and
// restores the native radius otherwise -- leaving the exterior view (and its component-level labels)
// stock. See fix_pan.cc, which drives the same provider for the exterior arrow-key rotate.
//
// graphics.interior_zoom_scale is a multiplier of the native endpoint (1.0 = native), not an absolute
// distance like zoom/haven_zoom: the native stop is a single pinned radius, so a factor is
// resolution-independent and self-heals if the game changes that radius. The config-knob and
// Constraint.FloatData field-resolution pattern follow haven_zoom.cc.
namespace
{
FieldInfo *provider_constraint = nullptr; // AbstractOrbitFrameProvider._constraint -> OrbitConstraint
FieldInfo *constraint_radius   = nullptr; // OrbitConstraint._radius -> Constraint.FloatData
FieldInfo *radius_enabled      = nullptr; // Constraint.FloatData._enabled
FieldInfo *radius_min          = nullptr; // Constraint.FloatData._min
FieldInfo *radius_max          = nullptr; // Constraint.FloatData._max
bool       fields_ok           = false;

struct NativeRange {
  float min, max;
  bool  operator==(const NativeRange&) const = default;
};
struct WeakOwner {
  Il2CppGCHandle handle = nullptr;
  explicit WeakOwner(Il2CppObject* object)
      : handle(object ? il2cpp_gchandle_new_weakref(object, false) : nullptr)
  {
  }
  WeakOwner(const WeakOwner&) = delete;
  WeakOwner(WeakOwner&& other) noexcept
      : handle(std::exchange(other.handle, nullptr))
  {
  }
  ~WeakOwner()
  {
    if (handle)
      il2cpp_gchandle_free(handle);
  }
  Il2CppObject* Get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
};
struct NativeEntry {
  WeakOwner   provider, constraint, radius;
  NativeRange native, applied;
  bool        owns_write = false;
};
// Weak identities distinguish a reused address or replaced constraint. Keep a
// hard bound even if a retired managed provider remains referenced elsewhere.
// The owner is process-lifetime so its weak handles never run a shutdown dtor.
auto&                                           g_native       = *new std::unordered_map<Il2CppObject*, NativeEntry>;
constexpr size_t                                kProviderLimit = 16;
bool                                            g_reported = false;

NativeRange ReadRange(Il2CppObject* radius)
{
  NativeRange value{};
  il2cpp_field_get_value(radius, radius_min, &value.min);
  il2cpp_field_get_value(radius, radius_max, &value.max);
  return value;
}

void WriteRange(Il2CppObject* radius, NativeRange value)
{
  il2cpp_field_set_value(radius, radius_min, &value.min);
  il2cpp_field_set_value(radius, radius_max, &value.max);
}

void Restore(NativeEntry& entry)
{
  auto* radius = entry.radius.Get();
  if (!radius || !entry.owns_write)
    return;
  // Do not overwrite a range subsequently changed by native code or a feature.
  // Root the managed FloatData while touching the retired owner's fields.
  const auto root = il2cpp_gchandle_new(radius, false);
  if (!root)
    return;
  if (ReadRange(il2cpp_gchandle_get_target(root)) == entry.applied)
    WriteRange(il2cpp_gchandle_get_target(root), entry.native);
  il2cpp_gchandle_free(root);
}

void PruneRetired()
{
  static auto next = std::chrono::steady_clock::time_point{};
  const auto  now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::seconds(1);
  for (auto it = g_native.begin(); it != g_native.end();) {
    auto& entry = it->second;
    if (!entry.provider.Get() || !entry.constraint.Get() || !entry.radius.Get()) {
      Restore(entry);
      it = g_native.erase(it);
    } else {
      ++it;
    }
  }
}

float InteriorScale()
{
  const auto scale = Config::Get().interior_zoom_scale;
  return (std::isfinite(scale) && scale > 1.0f) ? scale : 1.0f;
}

void OrbitFrameProvider_UpdateConstrains_Hook(auto original, Il2CppObject *provider, void *camera)
{
  if (!fields_ok || provider == nullptr) {
    original(provider, camera);
    return;
  }
  PruneRetired();
  auto*      section_manager = Hub::get_SectionManager();
  const bool in_interior =
      section_manager != nullptr && section_manager->CurrentSection == SectionID::Starbase_Interior;
  const float scale = InteriorScale();
  if (!in_interior || scale <= 1.0f) {
    // Scene exit must also retire providers which stop receiving callbacks.
    for (auto& [_, entry] : g_native)
      Restore(entry);
    g_native.clear();
    original(provider, camera);
    return;
  }

  Il2CppObject *constraint = nullptr, *radius = nullptr;
  il2cpp_field_get_value(provider, provider_constraint, &constraint);
  if (constraint != nullptr) {
    il2cpp_field_get_value(constraint, constraint_radius, &radius);
  }
  auto cached = g_native.find(provider);
  if (cached != g_native.end()
      && (cached->second.provider.Get() != provider || cached->second.constraint.Get() != constraint
          || cached->second.radius.Get() != radius)) {
    Restore(cached->second);
    g_native.erase(cached);
    cached = g_native.end();
  }
  if (radius == nullptr) {
    original(provider, camera);
    return;
  }

  {
    // Capture the native range once (before any widening), keyed by this provider.
    auto it = cached;
    if (it == g_native.end()) {
      // Multiple providers may reference the same FloatData. Reuse its one
      // native baseline instead of treating an already widened range as stock.
      it = std::find_if(g_native.begin(), g_native.end(),
                        [radius](const auto& value) { return value.second.radius.Get() == radius; });
    }
    if (it == g_native.end()) {
      if (g_native.size() >= kProviderLimit) {
        original(provider, camera);
        return; // Keep the native range if all bounded slots still have owners.
      }
      const auto  nr = ReadRange(radius);
      NativeEntry entry{WeakOwner(provider), WeakOwner(constraint), WeakOwner(radius), nr, nr};
      if (!entry.provider.Get() || !entry.constraint.Get() || !entry.radius.Get()) {
        original(provider, camera);
        return;
      }
      it = g_native.emplace(provider, std::move(entry)).first;
      if (!g_reported) {
        bool enabled = false;
        if (radius_enabled) {
          il2cpp_field_get_value(radius, radius_enabled, &enabled);
        }
        spdlog::info("[InteriorZoom] interior orbit endpoint native radius min={} max={} enabled={} (scale={})", nr.min,
                     nr.max, enabled, scale);
        g_reported = true;
      }
    }

    // Widen relative to the captured native range. Idempotent: always derived from the stored native,
    // so reasserting each frame cannot compound. Both ends scale together so a pinned endpoint
    // (min==max) simply relocates farther out; a real range stretches outward.
    auto&      entry    = it->second;
    const auto observed = ReadRange(radius);
    if (observed != entry.applied)
      entry.native = observed;
    const NativeRange widened{entry.native.min * scale, entry.native.max * scale};
    if (std::isfinite(widened.min) && std::isfinite(widened.max)) {
      if (observed != widened)
        WriteRange(radius, widened);
      entry.applied    = widened;
      entry.owns_write = widened != entry.native;
    } else {
      Restore(entry);
      g_native.erase(it);
    }
  }

  original(provider, camera); // native clamp now runs against the (widened or restored) range
}
// Interior radius changes precede native clamping; Haven angles follow it.
void SharedOrbitConstraints(auto original, Il2CppObject* provider, void* camera)
{
  OrbitFrameProvider_UpdateConstrains_Hook(original, provider, camera);
  ApplyHavenOrbitConstraints(provider);
}
} // namespace

bool InstallOrbitConstraintHook()
{
  static bool attempted = false, installed = false;
  if (attempted)
    return installed;
  attempted = true;
  auto orbit = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitFrameProvider");
  const auto* method = method_contract::Resolve(orbit.get_cls(), "UpdateConstrains", false,
                                                 "System.Void", {"UnityEngine.Camera"});
  if (!method || method->has_full_generic_sharing_signature || !method->methodPointer)
    return false;
#if __APPLE__
  if (!native_hooks::MacHookFits(method_contract::Pointer(method)))
    return false;
#endif
  installed = SPUD_STATIC_DETOUR(method->methodPointer, SharedOrbitConstraints) != nullptr;
  spdlog::info("[OrbitConstraints] shared Haven/interior hook installed={}", installed);
  return installed;
}

void InstallInteriorZoomHooks()
{
  auto orbit = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitFrameProvider");
  auto abstract_orbit =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "AbstractOrbitFrameProvider");
  auto orbit_constraint =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitConstraint");
  auto constraint = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "Constraint");

  if (!orbit.isValidHelper()) {
    ErrorMsg::MissingHelper("CameraController", "OrbitFrameProvider");
    return;
  }

  if (abstract_orbit.isValidHelper()) {
    provider_constraint = abstract_orbit.GetField("_constraint").get_info();
  }
  if (orbit_constraint.isValidHelper()) {
    constraint_radius = orbit_constraint.GetField("_radius").get_info();
  }
  if (constraint.isValidHelper()) {
    Il2CppClass *float_data = nullptr;
    void        *iterator   = nullptr;
    while (auto *nested = il2cpp_class_get_nested_types(constraint.get_cls(), &iterator)) {
      if (std::strcmp(il2cpp_class_get_name(nested), "FloatData") == 0) {
        float_data = nested;
        break;
      }
    }
    if (float_data != nullptr) {
      IL2CppClassHelper fd(float_data);
      radius_enabled = fd.GetField("_enabled").get_info();
      radius_min     = fd.GetField("_min").get_info();
      radius_max     = fd.GetField("_max").get_info();
    }
  }

  fields_ok =
      provider_constraint != nullptr && constraint_radius != nullptr && radius_min != nullptr && radius_max != nullptr;
  if (!fields_ok) {
    spdlog::warn("[InteriorZoom] orbit-constraint fields not resolved; interior zoom disabled, native behavior kept");
    return;
  }

  auto ptr_update_constrains = orbit.GetMethod("UpdateConstrains");
  if (ptr_update_constrains == nullptr) {
    ErrorMsg::MissingMethod("OrbitFrameProvider", "UpdateConstrains");
    return;
  }
  if (InstallOrbitConstraintHook()) {
    spdlog::info("[InteriorZoom] installed interior orbit-endpoint hook (scale={})", InteriorScale());
  } else {
    spdlog::warn("[InteriorZoom] OrbitFrameProvider.UpdateConstrains detour failed; native behavior kept");
  }
}
