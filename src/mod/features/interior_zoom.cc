#include "config.h"
#include "features/features.h"

#include "game/navigation.h"
#include <il2cpp/hook.h>

#include <game/app.h>

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstring>
#include <unordered_map>

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
struct NativeRange {
  float min, max;
};
// Native (pre-widen) radius range per live interior OrbitFrameProvider. The pointer is ONLY a map key,
// never dereferenced from the stored entry, so a retired scene instance cannot cause a stale-object
// access. Keyed by provider; the interior has a single orbit endpoint so this stays tiny.
std::unordered_map<OrbitFrameProvider*, NativeRange> g_native;
bool                                                 g_reported = false;

float InteriorScale()
{
  const auto scale = Config::Get().graphics.interior_zoom_scale;
  return (std::isfinite(scale) && scale > 1.0f) ? scale : 1.0f;
}

void OrbitFrameProvider_UpdateConstrains_Hook(auto original, OrbitFrameProvider* provider, void* camera)
{
  if (provider == nullptr) {
    original(provider, camera);
    return;
  }

  auto* constraint = provider->As<InteriorOrbitProvider>()->Constraint();
  auto* radius     = constraint ? constraint->Radius() : nullptr;
  if (radius == nullptr) {
    original(provider, camera);
    return;
  }

  auto*      section_manager = Hub::Sections();
  const bool in_interior =
      section_manager != nullptr && section_manager->CurrentSection() == SectionID::Starbase_Interior;
  const float scale = InteriorScale();

  if (in_interior && scale > 1.0f) {
    // Capture the native range once (before any widening), keyed by this provider.
    auto it = g_native.find(provider);
    if (it == g_native.end()) {
      NativeRange nr{0.0f, 0.0f};
      nr.min = radius->Minimum();
      nr.max = radius->Maximum();
      it     = g_native.emplace(provider, nr).first;
      if (!g_reported) {
        const bool enabled = radius->Enabled();
        spdlog::info("[InteriorZoom] interior orbit endpoint native radius min={} max={} enabled={} (scale={})", nr.min,
                     nr.max, enabled, scale);
        g_reported = true;
      }
    }

    // Widen relative to the captured native range. Idempotent: always derived from the stored native,
    // so reasserting each frame cannot compound. Both ends scale together so a pinned endpoint
    // (min==max) simply relocates farther out; a real range stretches outward.
    float new_min = it->second.min * scale;
    float new_max = it->second.max * scale;
    radius->SetMinimum(new_min);
    radius->SetMaximum(new_max);
  } else {
    // Not the interior (or feature off): the far endpoint is SHARED with the exterior station view, so
    // restore the native radius on any provider we previously widened. Without this the exterior stays
    // zoomed out and its component-level labels drop out of LOD range.
    auto it = g_native.find(provider);
    if (it != g_native.end()) {
      float native_min = it->second.min, native_max = it->second.max;
      radius->SetMinimum(native_min);
      radius->SetMaximum(native_max);
    }
  }

  original(provider, camera); // native clamp now runs against the (widened or restored) range
}
} // namespace

namespace
{
void InstallInteriorZoomHooks()
{
  const bool ready = InteriorOrbitProvider::Class::HasField<"_constraint", InteriorOrbitConstraint*>()
                     && InteriorOrbitConstraint::Class::HasField<"_radius", InteriorRadiusConstraint*>()
                     && InteriorRadiusConstraint::Class::HasField<"_min", float>()
                     && InteriorRadiusConstraint::Class::HasField<"_max", float>();
  if (!ready) {
    spdlog::warn("[InteriorZoom] orbit-constraint fields not resolved; native behavior kept");
    return;
  }
  IL2CPP_HOOK(OrbitFrameProvider::Class::Method("UpdateConstrains"), OrbitFrameProvider_UpdateConstrains_Hook);
}
} // namespace

const Feature kInteriorZoom{"InteriorZoom", &Config::Patches::interiorzoomhooks, InstallInteriorZoomHooks};
