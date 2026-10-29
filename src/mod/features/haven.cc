// Haven, the planetary base: [graphics] haven_zoom extends camera zoom-out with reversible road depth bias;
// hide_haven_water controls water independently. [ui] reverse_haven_history lists help history newest first.

#include "config.h"
#include "features/features.h"
#include "haven_visuals_policy.h"

#include "game/camera.h"
#include "game/haven.h"
#include "game/haven_orbit.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/core.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <functional>
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

  explicit HavenVisualBackend(std::function<bool()> current = {})
      : current_(std::move(current))
  {
  }
  bool Current() const
  { return !current_ || current_(); }
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
  {
    if (index >= properties_.size())
      return nullptr;
    if (!properties_[index])
      properties_[index] = HoldString(il2cpp::NewString(index == 0 ? "_OffsetFactor" : "_OffsetUnits"));
    return reinterpret_cast<Il2CppString*>(properties_[index].Get());
  }

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
  mutable std::array<Weak, 2> properties_;
  Weak                        current_owner_, found_owner_;
  std::function<bool()>       current_;
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
  if (!backend_.Current())
    return nullptr;
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
  return renderer && backend_.Current() && renderer->TrySetEnabled(value).succeeded;
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
  return has && has.value && backend_.Current() && material->TrySetFloat(name, value).succeeded;
}

class HavenVisualOwner
{
public:
  void SceneChanging(bool ready = false, const char* reason = "native reset")
  {
    // Native callbacks can reenter an application. Only invalidate authority
    // here: the current episode may still hold spans into the cached vectors.
    // Coalesce streaming bursts without starvation; reentry must defer work.
    const bool already_pending_ready = ready && cache_.IsReady() && !cache_.IsPublished() && !busy_;
    cache_.Invalidate(ready);
    if (!already_pending_ready)
      changed_frame_ = unity::Time::frameCount();
    applied_epoch_ = 0;
    retry_         = {};
    dirty_reason_  = reason;
  }
  bool MaterialEventsRelevant() const
  { return cache_.IsReady() && bool(cache_.Owner()) && (desired_expanded_ || desired_hidden_); }
  uint64_t Generation() const
  { return cache_.Epoch(); }
  void StreamPayloadUnavailable(uint64_t epoch)
  {
    if (cache_.Epoch() == epoch && MaterialEventsRelevant())
      SceneChanging(true, "unavailable streamed payload custody");
  }
  void MaterialsChanged(unity::GameObject* object)
  {
    if (!object || !MaterialEventsRelevant())
      return;
    const auto epoch            = cache_.Epoch();
    auto       owner            = il2cpp::GcHandle::Strong(cache_.Owner().Get());
    auto       candidate        = il2cpp::GcHandle::Strong(object);
    const auto retry_membership = [&] {
      if (cache_.Epoch() == epoch)
        SceneChanging(true, "unavailable streamed-object ownership");
    };
    if (!owner || !candidate) {
      retry_membership();
      return;
    }
    if (!candidate.Get<il2cpp::Object>()->Is<unity::GameObject>())
      return;
    const auto owner_alive     = unity::TryAlive(owner.Get<unity::GameObject>());
    const auto candidate_alive = unity::TryAlive(candidate.Get<unity::GameObject>());
    if (!owner_alive || !owner_alive.value || !candidate_alive) {
      retry_membership();
      return;
    }
    if (!candidate_alive.value)
      return;
    const auto root_transform = owner.Get<unity::GameObject>()->TryCall<"get_transform", unity::Transform*>();
    auto       root           = il2cpp::GcHandle::Strong(root_transform ? root_transform.value : nullptr);
    const auto own_transform  = candidate.Get<unity::GameObject>()->TryCall<"get_transform", unity::Transform*>();
    auto       node           = il2cpp::GcHandle::Strong(own_transform ? own_transform.value : nullptr);
    if (!root || !node) {
      retry_membership();
      return;
    }
    std::unordered_set<const void*> visited;
    while (root && node && visited.insert(node.Get()).second) {
      if (node.Get() == root.Get()) {
        if (cache_.Epoch() == epoch)
          SceneChanging(true, "native mesh/material refresh");
        return;
      }
      const auto parent = node.Get<unity::Transform>()->TryCall<"get_parent", unity::Transform*>();
      if (!parent) {
        retry_membership();
        return;
      }
      node = il2cpp::GcHandle::Strong(parent ? parent.value : nullptr);
      if (parent.value && !node) {
        retry_membership();
        return;
      }
    }
    if (node)
      retry_membership();
  }
  void Apply(bool expanded, bool hidden)
  {
    qualified_since_frame_ = true;
    Attempt([&] {
      const auto now         = std::chrono::steady_clock::now();
      const bool was_enabled = desired_expanded_ || desired_hidden_;
      if (desired_expanded_ != expanded || desired_hidden_ != hidden)
        retry_ = {};
      desired_expanded_ = expanded;
      desired_hidden_   = hidden;
      if (!was_enabled && (expanded || hidden))
        cache_.Invalidate(false);
      if (!expanded && !hidden && !owned_.Pending())
        return;
      const auto         epoch = cache_.Epoch();
      HavenVisualBackend backend([this, epoch] { return cache_.Epoch() == epoch; });
      if (!expanded && !hidden) {
        Restore(backend, now, epoch);
        return;
      }
      if (!cache_.IsReady()) {
        const auto observed =
            haven_orbit_binding::Starbase::Class::TryCall<"get_Instance", haven_orbit_binding::Starbase*>();
        auto       manager = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
        const auto ready =
            manager && unity::Alive(manager.Get<haven_orbit_binding::Starbase>())
                ? manager.Get<haven_orbit_binding::Starbase>()->TryCall<"get_AreInitialVisualsReady", bool>()
                : il2cpp::CallResult<bool>{};
        if (!ready || !ready.value) {
          Restore(backend, now, epoch);
          return; // Cheap native readiness only while a generation is pending.
        }
        cache_.SetReady(epoch, true);
      }
      if (cache_.IsPublished() && applied_epoch_ == epoch && applied_expanded_ == expanded && applied_hidden_ == hidden)
        return;
      if (cache_.Epoch() != epoch || unity::Time::frameCount() == changed_frame_)
        return; // Let native streaming/lighting callers finish their frame.
      const bool was_published = cache_.IsPublished();
      const auto start         = Clock::now();
      if (!cache_.Refresh(backend, now)) {
        Restore(backend, now, epoch);
        return; // Failed-only cache recovery; never a periodic successful scan.
      }
      if (!was_published)
        spdlog::debug("[HavenZoom] visual generation={} published roads={} water={} reason={} scan_us={}", epoch,
                      cache_.Roads().size(), cache_.Water().size(), dirty_reason_,
                      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
      if (now < retry_)
        return;
      const bool verified = owned_.Apply(backend, cache_.Roads(), cache_.Water(), expanded, hidden);
      if (cache_.Epoch() != epoch || !cache_.IsPublished())
        return; // A callback during a setter must not acknowledge a stale cohort.
      applied_epoch_    = verified ? epoch : 0;
      applied_expanded_ = expanded;
      applied_hidden_   = hidden;
      retry_            = verified ? Clock::time_point{} : now + std::chrono::seconds(1);
    });
  }
  void Frame()
  {
    Attempt([&] {
      const bool qualified = std::exchange(qualified_since_frame_, false);
      if (!qualified && was_qualified_)
        SceneChanging(false, "left qualified Haven camera");
      was_qualified_ = qualified;
      if (qualified || !owned_.Pending())
        return;
      const auto now = Clock::now();
      if (now < retry_)
        return;
      const auto         epoch = cache_.Epoch();
      HavenVisualBackend backend([this, epoch] { return cache_.Epoch() == epoch; });
      Restore(backend, now, epoch);
      if (cache_.Epoch() != epoch)
        return;
      desired_expanded_ = desired_hidden_ = false;
    }); // No scene discovery outside a qualified Haven camera callback.
  }

private:
  void Restore(HavenVisualBackend& backend, std::chrono::steady_clock::time_point now, uint64_t epoch)
  {
    if (now < retry_ || !owned_.Pending())
      return;
    const bool restored = owned_.Apply(backend, {}, {}, false, false);
    if (cache_.Epoch() != epoch)
      return;
    applied_epoch_ = 0;
    retry_         = restored ? Clock::time_point{} : now + std::chrono::seconds(1);
  }
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
  using Clock = std::chrono::steady_clock;
  haven_visuals::SnapshotCache<HavenVisualBackend> cache_;
  haven_visuals::OwnedVisuals<HavenVisualBackend>  owned_;
  Clock::time_point                                retry_{};
  uint64_t                                         applied_epoch_ = 0;
  int32_t                                          changed_frame_ = -1;
  const char*                                      dirty_reason_  = "initial qualified scene";
  bool desired_expanded_ = false, desired_hidden_ = false, applied_expanded_ = false, applied_hidden_ = false;
  bool qualified_since_frame_ = false, was_qualified_ = false, reported_ = false, busy_ = false;
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

void PlanetaryStarbaseManager_ResetInitialVisualLoadState_Hook(auto original, haven_orbit_binding::Starbase* _this)
{
  if (auto* visuals = HavenVisuals())
    visuals->SceneChanging(false, "initial visual reset");
  original(_this);
}
void PlanetaryStarbaseManager_ClearMapModuleVisualState_Hook(auto original, haven_orbit_binding::Starbase* _this)
{
  if (auto* visuals = HavenVisuals())
    visuals->SceneChanging(false, "map module reset");
  original(_this);
}
void AssetLODStreaming_RefreshMeshAndMaterial_Hook(auto original, il2cpp::Object* _this, unity::GameObject* object)
{
  auto* visuals = HavenVisuals();
  if (!visuals || !visuals->MaterialEventsRelevant()) {
    original(_this, object);
    return; // No Unity lookups or GC handles for non-Haven streaming.
  }
  const auto epoch = visuals->Generation();
  auto       root  = il2cpp::GcHandle::Strong(object);
  original(_this, object);
  if (root)
    visuals->MaterialsChanged(root.Get<unity::GameObject>());
  else if (object)
    visuals->StreamPayloadUnavailable(epoch);
}

// Haven's zoom blends between two planetary orbits, each pinned to one radius (min == max) and sitting on it. Reads
// that radius, or nothing for any other kind of endpoint.
std::optional<float> HavenOrbitRadius(FrameProvider* provider)
{ return haven_orbit_binding::Radius(provider ? provider->As<AbstractOrbitFrameProvider>() : nullptr); }

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

  const auto source_observed = haven_binding::Field<"_sourceFrameProvider", FrameProvider*>(_this);
  const auto target_observed = haven_binding::Field<"_targetFrameProvider", FrameProvider*>(_this);
  auto*      source          = source_observed ? source_observed.value : nullptr;
  auto*      target          = target_observed ? target_observed.value : nullptr;
  const auto source_pinned   = HavenOrbitRadius(source);
  const auto target_pinned   = source_pinned ? HavenOrbitRadius(target) : std::nullopt;
  if (!target_pinned)
    return;
  const float source_radius = *source_pinned, target_radius = *target_pinned;
  if (source_radius == target_radius)
    return;

  auto*      source_orbit = source->As<AbstractOrbitFrameProvider>();
  auto*      target_orbit = target->As<AbstractOrbitFrameProvider>();
  const auto pivot_a      = haven_binding::Field<"_orbitPivot", TargetController*>(source_orbit);
  const auto pivot_b      = haven_binding::Field<"_orbitPivot", TargetController*>(target_orbit);
  const auto look_a       = haven_binding::Field<"_lookTarget", TargetController*>(source_orbit);
  const auto look_b       = haven_binding::Field<"_lookTarget", TargetController*>(target_orbit);
  if (!pivot_a || !pivot_b || !look_a || !look_b || !pivot_a.value || pivot_b.value != pivot_a.value
      || look_a.value != pivot_a.value || look_b.value != pivot_a.value)
    return;
  const auto source_frame_observed = haven_binding::Field<"_sourceFrame", CameraFrame*>(_this);
  const auto target_frame_observed = haven_binding::Field<"_targetFrame", CameraFrame*>(_this);
  const auto result_observed       = haven_binding::Field<"_blendedCameraFrame", EditableCameraFrame*>(_this);
  const auto curve_observed        = haven_binding::Field<"_blendCurve", unity::AnimationCurve*>(_this);
  const auto low                   = haven_binding::Field<"_softLimitMin", float>(_this);
  const auto high                  = haven_binding::Field<"_softLimitMax", float>(_this);
  const auto blend_ratio           = haven_binding::Field<"_blendRatio", float>(_this);
  if (!source_frame_observed || !target_frame_observed || !result_observed || !curve_observed || !low || !high
      || !blend_ratio)
    return;
  auto*        source_frame = source_frame_observed.value;
  auto*        target_frame = target_frame_observed.value;
  CameraFrame* result       = result_observed.value;
  auto*        curve        = curve_observed.value;
  const auto   minimum = low.value, maximum = high.value, ratio = blend_ratio.value;
  if (!source_frame || !target_frame || !result || !curve || source_frame == target_frame || result == source_frame
      || result == target_frame || !HavenFinite(minimum, maximum, ratio) || minimum < 0.0f || maximum > 1.0f
      || maximum <= minimum)
    return;

  const auto position_observed   = haven_binding::Field<"_position", unity::Vector3>(result);
  const auto rotation_a          = haven_binding::Field<"_rotation", unity::Quaternion>(source_frame);
  const auto rotation_b          = haven_binding::Field<"_rotation", unity::Quaternion>(target_frame);
  const auto fov_a               = haven_binding::Field<"_FOV", float>(source_frame);
  const auto fov_b               = haven_binding::Field<"_FOV", float>(target_frame);
  const auto orthographic_a      = haven_binding::Field<"_orthographic", bool>(source_frame);
  const auto orthographic_b      = haven_binding::Field<"_orthographic", bool>(target_frame);
  const auto orthographic_result = haven_binding::Field<"_orthographic", bool>(result);
  if (!position_observed || !rotation_a || !rotation_b || !fov_a || !fov_b || !orthographic_a || !orthographic_b
      || !orthographic_result)
    return;
  auto       position = position_observed.value;
  const auto ar = rotation_a.value, br = rotation_b.value;
  const auto af = fov_a.value, bf = fov_b.value;
  const auto rotation_dot           = ar.x * br.x + ar.y * br.y + ar.z * br.z + ar.w * br.w;
  const auto gap                    = std::abs(source_radius - target_radius);
  const auto rotation_length        = ar.x * ar.x + ar.y * ar.y + ar.z * ar.z + ar.w * ar.w;
  const auto target_rotation_length = br.x * br.x + br.y * br.y + br.z * br.z + br.w * br.w;
  // Qualify the common-pivot, same-facing perspective orbit measured in Haven: unit rotations that agree up to sign
  // (|dot| = 1) and one field of view. Only then is moving along the camera's back axis the same as a wider orbit.
  // Preserve native output when a client changes that geometry or camera mode.
  if (!HavenFinite(rotation_length, target_rotation_length, rotation_dot, af, bf)
      || std::abs(rotation_length - 1.0f) > 0.0001f || std::abs(target_rotation_length - 1.0f) > 0.0001f
      || std::abs(std::abs(rotation_dot) - 1.0f) > 0.0001f || af <= 0.0f || std::abs(af - bf) > 0.001f
      || orthographic_a.value || orthographic_b.value || orthographic_result.value)
    return;

  // Water belongs to the qualified Haven scene, independently of curve shape or requested zoom expansion.
  const bool expanded = std::isfinite(max_zoom) && max_zoom > std::max(source_radius, target_radius);
  if (auto* visuals = HavenVisuals())
    visuals->Apply(expanded, hidden);
  if (!expanded)
    return;

  const auto curve_ratio   = curve->TryCall<"Evaluate", float(float)>(ratio);
  const auto curve_minimum = curve->TryCall<"Evaluate", float(float)>(minimum);
  const auto curve_maximum = curve->TryCall<"Evaluate", float(float)>(maximum);
  if (!curve_ratio || !curve_minimum || !curve_maximum)
    return;
  const auto at_ratio = curve_ratio.value, at_minimum = curve_minimum.value, at_maximum = curve_maximum.value;
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
  const auto clip_observed = haven_binding::Field<"_farPlane", float>(result);
  if (!clip_observed)
    return;
  auto clip = clip_observed.value;
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

void ReverseHavenRecords(il2cpp::List<Il2CppObject*>* records)
{
  if (!records)
    return;
  auto owner = il2cpp::GcHandle::Strong(records);
  if (!owner)
    return;
  const auto count = records->TryCall<"get_Count", int32_t>();
  if (!count || count.value <= 1)
    return;
  // Preserve the source's complete snapshot before the first managed setter.
  // A failed getter is unavailable data, never a null record to publish.
  std::vector<il2cpp::GcHandle> items;
  items.reserve(count.value);
  for (int32_t i = 0; i < count.value; ++i) {
    const auto observed = records->TryCall<"get_Item", Il2CppObject*(int32_t)>(i);
    if (!observed)
      return;
    auto item = il2cpp::GcHandle::Strong(observed.value);
    if (observed.value && (!item || item.Get() != observed.value))
      return;
    items.push_back(std::move(item));
  }
  for (int32_t i = 0; i < count.value; ++i)
    if (!records->TryCall<"set_Item", void(int32_t, Il2CppObject*)>(i, items[count.value - i - 1].Get()))
      return; // Like the source, retain native delivery after a setter failure.
}

void PlanetaryHelpHistoryContext_ApplyRecords_Hook(auto original, PlanetaryHelpHistoryContext* _this,
                                                   il2cpp::List<Il2CppObject*>* records)
{
  if (Config::Get().ui.reverse_haven_history)
    ReverseHavenRecords(records);
  original(_this, records);
}

bool InstallHavenZoom()
{
  if (!unity::Time::FrameCountAvailable()
      || !haven_orbit_binding::Starbase::Class::Method<bool()>("get_AreInitialVisualsReady")) {
    spdlog::warn("[HavenZoom] visual lifecycle readiness unavailable; keeping native camera");
    return false;
  }
  il2cpp::HookSet hooks("HavenZoom");
  IL2CPP_STAGE(hooks, BlendFrameProvider::Class::Method<void(unity::Camera*)>("UpdateCameraFrame"),
               BlendFrameProvider_UpdateCameraFrame_Hook);
  IL2CPP_STAGE(hooks, haven_orbit_binding::Starbase::Class::Method<void()>("ResetInitialVisualLoadState"),
               PlanetaryStarbaseManager_ResetInitialVisualLoadState_Hook);
  IL2CPP_STAGE(hooks, haven_orbit_binding::Starbase::Class::Method<void()>("ClearMapModuleVisualState"),
               PlanetaryStarbaseManager_ClearMapModuleVisualState_Hook);
  IL2CPP_STAGE(hooks, HavenAssetLodStreaming::Method<void(unity::GameObject*)>("RefreshMeshAndMaterial"),
               AssetLODStreaming_RefreshMeshAndMaterial_Hook);
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
