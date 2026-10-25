// Haven, the planetary base: [graphics] haven_zoom extends camera zoom-out with reversible road depth bias;
// hide_haven_water controls water independently. [ui] reverse_haven_history lists help history newest first.

#include "config.h"
#include "features/features.h"
#include "haven_visuals_policy.h"

#include "game/camera.h"
#include "game/haven.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/core.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <new>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
// --- Haven zoom ---

// Discovery is shared by road bias and water. Persistent records hold weak references; every native operation
// reacquires local strong custody and a checked identity. Unavailable observations never become false or zero.
class HavenVisualBackend;
class HavenVisualAccess
{
public:
  HavenVisualAccess(const il2cpp::GcHandle& weak, const HavenVisualBackend& backend);
  haven_visuals::AccessState State() const
  { return state_; }
  const void* Token() const
  { return root_.Get(); }
  std::optional<int32_t> Id();
  std::optional<bool>    ReadEnabled();
  bool                   WriteEnabled(bool value);
  std::optional<float>   ReadDepth(size_t property);
  bool                   WriteDepth(size_t property, float value);

private:
  il2cpp::Object*            Live() const;
  il2cpp::GcHandle           root_;
  const HavenVisualBackend&  backend_;
  haven_visuals::AccessState state_ = haven_visuals::AccessState::Unavailable;
};

class HavenVisualBackend
{
public:
  using Weak   = il2cpp::GcHandle;
  using Object = unity::GameObject*;

  HavenVisualBackend()
  {
    for (size_t i = 0; i < properties_.size(); ++i)
      properties_[i] = HoldString(il2cpp::NewString(i == 0 ? "_OffsetFactor" : "_OffsetUnits"));
  }
  HavenVisualAccess Acquire(const Weak& weak)
  { return {weak, *this}; }
  Weak Retain(const HavenVisualAccess& access)
  { return Weak::Weak(reinterpret_cast<Il2CppObject*>(const_cast<void*>(access.Token()))); }
  Weak Retain(Object object)
  { return Weak::Weak(object); }

  Object GetOwner(const Weak& weak)
  {
    // Keep both the previous and newly found owners rooted through the complete cache refresh.
    current_owner_ = Weak::Strong(weak.Get());
    auto* object   = current_owner_.Get<unity::GameObject>();
    return IsActiveOwner(object) ? object : nullptr;
  }
  Object FindOwner()
  {
    auto path = HoldString(il2cpp::NewString("StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder"));
    if (!path)
      return nullptr;
    const auto found = unity::GameObject::TryFind(reinterpret_cast<Il2CppString*>(path.Get()));
    if (!found || !found.value)
      return nullptr;
    found_owner_ = Weak::Strong(found.value);
    auto* object = found_owner_.Get<unity::GameObject>();
    return IsActiveOwner(object) ? object : nullptr;
  }

  bool ScanOwner(Object object, std::vector<Weak>& roads, std::vector<Weak>& water)
  {
    auto owner = Weak::Strong(object);
    if (!owner || !IsActiveOwner(owner.Get<unity::GameObject>()))
      return false;
    auto* type      = unity::Renderer::Class::TypeObject();
    auto  type_root = Weak::Strong(type ? &type->object : nullptr);
    if (!type_root)
      return false;
    const auto observed = owner.Get<unity::GameObject>()->TryComponentsInChildren(
        reinterpret_cast<Il2CppReflectionType*>(type_root.Get()), true);
    if (!observed || !observed.value)
      return false;
    auto components = Weak::Strong(observed.value);
    if (!components)
      return false;
    auto checked = il2cpp::TryReferenceArray<unity::Component*>(components.Get());
    if (!checked || !checked.value || checked.value->size() > 4096)
      return false; // Admission failure, never a silently truncated scene snapshot.
    const auto                      count = checked.value->size();
    std::unordered_set<const void*> road_seen, water_seen;
    for (size_t i = 0; i < count; ++i) {
      auto* candidate = (*reinterpret_cast<il2cpp::Array<unity::Component*>*>(components.Get()))[i];
      if (!candidate)
        continue;
      auto component = Weak::Strong(candidate);
      if (!component)
        return false;
      auto* renderer = component.Get<il2cpp::Object>()->As<unity::Renderer>();
      if (!renderer)
        continue; // The actual non-generic result is Component[], not Renderer[].
      const auto materials_observed = renderer->TrySharedMaterials();
      if (!materials_observed || !materials_observed.value)
        return false;
      auto materials = Weak::Strong(materials_observed.value);
      if (!materials)
        return false;
      const auto material_array = il2cpp::TryReferenceArray<unity::Material*>(materials.Get());
      if (!material_array || !material_array.value || material_array.value->size() > 16)
        return false;
      bool       has_water      = false;
      const auto material_count = material_array.value->size();
      for (size_t j = 0; j < material_count; ++j) {
        auto* item = (*reinterpret_cast<il2cpp::Array<unity::Material*>*>(materials.Get()))[j];
        if (!item)
          continue;
        auto material = Weak::Strong(item);
        if (!material)
          return false;
        const auto name = Name(material.Get<il2cpp::Object>());
        if (!name)
          return false;
        const bool road_candidate = IsRoadName(*name), water_candidate = *name == "mat_PB_water";
        if (!road_candidate && !water_candidate)
          continue;
        const auto shader_observed = material.Get<unity::Material>()->TryShader();
        if (!shader_observed || !shader_observed.value)
          return false;
        auto shader = Weak::Strong(shader_observed.value);
        if (!shader || !shader.Get<il2cpp::Object>()->Is<unity::Shader>())
          return false;
        const auto shader_name = Name(shader.Get<il2cpp::Object>());
        if (!shader_name)
          return false;
        if (road_candidate && *shader_name == "Custom/LitMSAE" && road_seen.insert(material.Get()).second) {
          if (roads.size() == haven_visuals::kRoadLimit)
            return false;
          auto weak = Weak::Weak(material.Get());
          if (!weak)
            return false;
          roads.push_back(std::move(weak));
        }
        has_water |= water_candidate && *shader_name == "Custom/WaterLite";
      }
      if (has_water && water_seen.insert(component.Get()).second) {
        if (water.size() == haven_visuals::kWaterLimit)
          return false;
        auto weak = Weak::Weak(component.Get());
        if (!weak)
          return false;
        water.push_back(std::move(weak));
      }
    }
    return IsActiveOwner(owner.Get<unity::GameObject>());
  }
  Il2CppString* Property(size_t index) const
  { return index < properties_.size() ? reinterpret_cast<Il2CppString*>(properties_[index].Get()) : nullptr; }

private:
  static Weak HoldString(Il2CppString* value)
  { return Weak::Strong(value ? &value->object : nullptr); }
  static bool IsActiveOwner(Object object)
  {
    if (!object || !object->Is<unity::GameObject>())
      return false;
    const auto alive  = unity::TryAlive(object);
    const auto active = alive && alive.value ? object->TryActiveInHierarchy() : il2cpp::CallResult<bool>{};
    return active && active.value;
  }
  static std::optional<std::string> Name(il2cpp::Object* object)
  {
    const auto observed = unity::TryName(object);
    if (!observed || !observed.value)
      return std::nullopt;
    auto text = HoldString(observed.value);
    return text && text.Get<il2cpp::Object>()->Is<Il2CppString>()
               ? std::optional{il2cpp::ToUtf8(reinterpret_cast<Il2CppString*>(text.Get()))}
               : std::nullopt;
  }
  static bool IsRoadName(std::string_view name)
  {
    static constexpr std::string_view prefixes[] = {
        "mat_PB_3101101_asphalt_", "mat_PB_3101201_asphalt_", "mat_PB_3101301_asphalt_", "mat_PB_3101401_asphalt_",
        "mat_PB_3101501_asphalt_", "mat_PB_3101601_asphalt_", "mat_PB_3101701_asphalt_"};
    return std::ranges::any_of(prefixes, [&](auto prefix) { return name.starts_with(prefix); });
  }
  std::array<Weak, 2> properties_;
  Weak                current_owner_, found_owner_;
};

HavenVisualAccess::HavenVisualAccess(const il2cpp::GcHandle& weak, const HavenVisualBackend& backend)
    : backend_(backend)
{
  if (!weak.Get()) {
    state_ = haven_visuals::AccessState::Gone;
    return;
  }
  root_ = il2cpp::GcHandle::Strong(weak.Get());
  if (!root_)
    return;
  auto* object = root_.Get<il2cpp::Object>();
  if (!object || !object->Is<unity::UnityObject>())
    return;
  const auto alive = unity::TryAlive(object);
  if (alive)
    state_ = alive.value ? haven_visuals::AccessState::Ready : haven_visuals::AccessState::Gone;
}
il2cpp::Object* HavenVisualAccess::Live() const
{
  auto*      object = root_.Get<il2cpp::Object>();
  const auto alive  = object ? unity::TryAlive(object) : il2cpp::CallResult<bool>{};
  return alive && alive.value ? object : nullptr;
}
std::optional<int32_t> HavenVisualAccess::Id()
{
  const auto observed = unity::TryInstanceId(Live());
  return observed ? std::optional{observed.value} : std::nullopt;
}
std::optional<bool> HavenVisualAccess::ReadEnabled()
{
  auto*      object   = Live();
  auto*      renderer = object ? object->As<unity::Renderer>() : nullptr;
  const auto observed = renderer ? renderer->TryEnabled() : il2cpp::CallResult<bool>{};
  return observed ? std::optional{observed.value} : std::nullopt;
}
bool HavenVisualAccess::WriteEnabled(bool value)
{
  auto* object   = Live();
  auto* renderer = object ? object->As<unity::Renderer>() : nullptr;
  return renderer && renderer->TrySetEnabled(value).succeeded;
}
std::optional<float> HavenVisualAccess::ReadDepth(size_t property)
{
  auto*      object   = Live();
  auto*      material = object ? object->As<unity::Material>() : nullptr;
  auto*      name     = backend_.Property(property);
  const auto has      = material && name ? material->TryHasProperty(name) : il2cpp::CallResult<bool>{};
  const auto observed = has && has.value ? material->TryGetFloat(name) : il2cpp::CallResult<float>{};
  return observed ? std::optional{observed.value} : std::nullopt;
}
bool HavenVisualAccess::WriteDepth(size_t property, float value)
{
  auto*      object   = Live();
  auto*      material = object ? object->As<unity::Material>() : nullptr;
  auto*      name     = backend_.Property(property);
  const auto has      = material && name ? material->TryHasProperty(name) : il2cpp::CallResult<bool>{};
  return has && has.value && material->TrySetFloat(name, value).succeeded;
}

class HavenVisualOwner
{
public:
  void Apply(bool expanded, bool hidden)
  {
    Attempt([&] {
      qualified_since_frame_ = true;
      if (!expanded && !hidden && !owned_.Pending())
        return;
      if (!due_.Due())
        return;
      HavenVisualBackend backend;
      if (!expanded && !hidden) {
        owned_.Apply(backend, {}, {}, false, false);
        return;
      }
      cache_.Refresh(backend, std::chrono::steady_clock::now());
      owned_.Apply(backend, cache_.Roads(), cache_.Water(), expanded, hidden);
    });
  }
  void Frame()
  {
    Attempt([&] {
      if (std::exchange(qualified_since_frame_, false) || !owned_.Pending() || !due_.Due())
        return;
      HavenVisualBackend backend;
      owned_.Apply(backend, {}, {}, false, false);
    }); // No scene discovery outside a qualified Haven camera callback.
  }

private:
  template <class F> void Attempt(F&& action)
  {
    try {
      (void)haven_visuals::RunExclusive(busy_, std::forward<F>(action));
    } catch (...) {
      if (!std::exchange(reported_, true)) {
        try {
          spdlog::warn("[HavenZoom] Visual observations unavailable; retained changes await checked restoration");
        } catch (...) {
        }
      }
    }
  }
  Throttle                                         due_{std::chrono::seconds(1)};
  haven_visuals::SnapshotCache<HavenVisualBackend> cache_;
  haven_visuals::OwnedVisuals<HavenVisualBackend>  owned_;
  bool                                             qualified_since_frame_ = false, reported_ = false, busy_ = false;
};
HavenVisualOwner* HavenVisuals()
{
  // Runtime-owned weak handles must not be destroyed after Unity shuts down.
  static auto* visuals = new (std::nothrow) HavenVisualOwner;
  return visuals;
}
void HavenVisualsFrame()
{
  if (auto* visuals = HavenVisuals())
    visuals->Frame();
}

// Haven's zoom blends between two planetary orbits, each pinned to one radius (min == max) and sitting on it. Reads
// that radius, or nothing for any other kind of endpoint.
std::optional<float> HavenOrbitRadius(FrameProvider* provider)
{
  if (!provider || !provider->Is<PlanetaryOrbitFrameProvider>() || !provider->Is<AbstractOrbitFrameProvider>())
    return std::nullopt;
  auto* orbit      = provider->As<AbstractOrbitFrameProvider>();
  auto* constraint = orbit->constraint();
  auto* data       = constraint ? constraint->radius() : nullptr;
  if (!data || !data->enabled())
    return std::nullopt;
  const auto minimum = data->min(), maximum = data->max();
  const auto radius = orbit->radius();
  if (std::isfinite(radius) && std::isfinite(maximum) && maximum > 0.0f && minimum == maximum
      && std::abs(radius - maximum) < 0.01f)
    return radius;
  return std::nullopt;
}

// Every value is finite: NaN and infinity both fail.
bool HavenFinite(std::floating_point auto... values)
{ return (std::isfinite(values) && ...); }

void BlendFrameProvider_UpdateCameraFrame_Hook(auto original, BlendFrameProvider* _this, unity::Camera* camera)
{
  // Native input, constraints and endpoint updates run first. Only this blend's newly generated output is extended;
  // endpoint assets and normalized LOD stay native.
  original(_this, camera);
  const auto max_zoom = Config::Get().graphics.haven_zoom;
  const bool hidden   = Config::Get().graphics.hide_haven_water;
  if ((!std::isfinite(max_zoom) || max_zoom <= 0.0f) && !hidden)
    return; // Pending interventions still drain through this feature's existing frame callback.

  auto*      source        = _this->sourceFrameProvider();
  auto*      target        = _this->targetFrameProvider();
  const auto source_pinned = HavenOrbitRadius(source);
  const auto target_pinned = source_pinned ? HavenOrbitRadius(target) : std::nullopt;
  if (!target_pinned)
    return;
  const float source_radius = *source_pinned, target_radius = *target_pinned;
  if (source_radius == target_radius)
    return;

  auto* source_orbit = source->As<AbstractOrbitFrameProvider>();
  auto* target_orbit = target->As<AbstractOrbitFrameProvider>();
  auto* pivot        = source_orbit->orbitPivot();
  if (!pivot || target_orbit->orbitPivot() != pivot || source_orbit->lookTarget() != pivot
      || target_orbit->lookTarget() != pivot)
    return;
  auto*        source_frame = _this->sourceFrame();
  auto*        target_frame = _this->targetFrame();
  CameraFrame* result       = _this->blendedCameraFrame();
  auto*        curve        = _this->blendCurve();
  const auto   minimum = _this->softLimitMin(), maximum = _this->softLimitMax(), ratio = _this->blendRatio();
  if (!source_frame || !target_frame || !result || !curve || source_frame == target_frame || result == source_frame
      || result == target_frame || !HavenFinite(minimum, maximum, ratio) || minimum < 0.0f || maximum > 1.0f
      || maximum <= minimum)
    return;

  auto       position = result->position();
  const auto ar = source_frame->rotation(), br = target_frame->rotation();
  const auto af = source_frame->fov(), bf = target_frame->fov();
  const auto rotation_dot    = ar.x * br.x + ar.y * br.y + ar.z * br.z + ar.w * br.w;
  const auto gap             = std::abs(source_radius - target_radius);
  const auto rotation_length = ar.x * ar.x + ar.y * ar.y + ar.z * ar.z + ar.w * ar.w;
  // Qualify the common-pivot, same-facing perspective orbit measured in Haven: unit rotations that agree up to sign
  // (|dot| = 1) and one field of view. Only then is moving along the camera's back axis the same as a wider orbit.
  // Preserve native output when a client changes that geometry or camera mode.
  if (!HavenFinite(rotation_length, rotation_dot, af, bf) || std::abs(rotation_length - 1.0f) > 0.0001f
      || std::abs(std::abs(rotation_dot) - 1.0f) > 0.0001f || af <= 0.0f || std::abs(af - bf) > 0.001f
      || source_frame->orthographic() || target_frame->orthographic() || result->orthographic())
    return;

  // Water belongs to the qualified Haven scene, independently of curve shape or requested zoom expansion.
  const bool expanded = std::isfinite(max_zoom) && max_zoom > std::max(source_radius, target_radius);
  if (auto* visuals = HavenVisuals())
    visuals->Apply(expanded, hidden);
  if (!expanded)
    return;

  const auto at_ratio   = curve->Evaluate(ratio);
  const auto at_minimum = curve->Evaluate(minimum);
  const auto at_maximum = curve->Evaluate(maximum);
  if (!HavenFinite(at_ratio, at_minimum, at_maximum))
    return;
  // Weights run from 0 at the inner orbit to 1 at the outer one, whichever way the blend goes. outer_weight and
  // inner_weight are the curve at the soft limits, where the native zoom stops; progress is how far the current
  // ratio is between them. The extra distance grows with progress up to max_zoom - native_outer at the outer stop,
  // and never takes the camera past max_zoom.
  const bool source_is_far = source_radius > target_radius;
  const auto outer_weight =
      source_is_far ? 1.0f - std::clamp(at_minimum, 0.0f, 1.0f) : std::clamp(at_maximum, 0.0f, 1.0f);
  const auto inner_weight =
      source_is_far ? 1.0f - std::clamp(at_maximum, 0.0f, 1.0f) : std::clamp(at_minimum, 0.0f, 1.0f);
  if (outer_weight <= inner_weight)
    return;
  const auto weight       = source_is_far ? 1.0 - std::clamp(at_ratio, 0.0f, 1.0f) : std::clamp(at_ratio, 0.0f, 1.0f);
  const auto inner_radius = std::min(source_radius, target_radius);
  const auto native_outer = inner_radius + gap * outer_weight;
  const auto native_distance = inner_radius + gap * weight;
  const auto progress        = std::clamp((weight - inner_weight) / (outer_weight - inner_weight), 0.0, 1.0);
  const auto extra           = std::min(max_zoom - native_distance, (max_zoom - native_outer) * progress);
  if (!std::isfinite(extra))
    return;
  if (extra <= 0.0)
    return;

  // Sequential endpoint updates can cache different positions of their shared pivot during pan. Use the camera back
  // axis so expansion remains steady: the rotation's forward vector is (2(xz + wy), 2(yz - wx), 1 - 2(x^2 + y^2)), and
  // the camera moves `extra` against it. The far plane moves out by the same amount.
  position.x += static_cast<float>(-2.0 * (ar.x * ar.z + ar.w * ar.y) * extra);
  position.y += static_cast<float>(-2.0 * (ar.y * ar.z - ar.w * ar.x) * extra);
  position.z += static_cast<float>((2.0 * (ar.x * ar.x + ar.y * ar.y) - 1.0) * extra);
  auto clip = result->farPlane();
  if (!std::isfinite(clip) || clip <= 0.0f)
    return;
  clip += static_cast<float>(extra);
  if (!HavenFinite(position.x, position.y, position.z, clip) || clip <= 0.0f)
    return;
  result->SetPosition(position);
  result->SetFarPlane(clip);
  static bool reported = false;
  if (!std::exchange(reported, true))
    spdlog::info("[HavenZoom] expanded native outer distance {} to maximum {}", native_outer, max_zoom);
}

// --- Help history ---

void PlanetaryHelpHistoryContext_ApplyRecords_Hook(auto original, PlanetaryHelpHistoryContext* _this,
                                                   il2cpp::List<Il2CppObject*>* records)
{
  if (records && Config::Get().ui.reverse_haven_history) {
    // Swap through the managed setter so reference writes keep their GC barriers.
    for (int32_t i = 0, j = records->Count() - 1; i < j; ++i, --j) {
      auto* first = (*records)[i];
      auto* last  = (*records)[j];
      records->Set(i, last);
      records->Set(j, first);
    }
  }
  original(_this, records);
}

bool InstallHavenZoom()
{
  il2cpp::HookSet hooks("HavenZoom");
  IL2CPP_STAGE(hooks, BlendFrameProvider::Class::Method<void(unity::Camera*)>("UpdateCameraFrame"),
               BlendFrameProvider_UpdateCameraFrame_Hook);
  return hooks.Commit();
}

bool InstallHavenHistory()
{
  il2cpp::HookSet hooks("HavenHistory");
  IL2CPP_STAGE(hooks, PlanetaryHelpHistoryContext::Class::Method("ApplyRecords", 1),
               PlanetaryHelpHistoryContext_ApplyRecords_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kHavenZoom{"HavenZoom", &Config::Patches::havenzoomhooks, InstallHavenZoom, HavenVisualsFrame};
const Feature kHavenHistory{"HavenHistory", &Config::Patches::havenhistoryhooks, InstallHavenHistory};
