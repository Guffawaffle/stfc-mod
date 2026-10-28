// Galaxy labels: independent major/minor zoom profiles, native overlay multi-selection, housing markers.
// The camera and NavigationLOD detours are shared with existing system-fleet labels.
#include "features/galaxy.h"
#include "config.h"
#include "embedded_station_housing.h"
#include "features/features.h"
#include "galaxy_housing_data.h"
#include "game/galaxy.h"
#include "runtime_config.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <thread>
#include <unity/ui.h>
#include <vector>

namespace
{
namespace gb = galaxy_binding;
using namespace galaxy_controls;
struct GalaxyShift {
  il2cpp::GcHandle target, parent, group, element;
  unity::Vector2   position{}, anchor_min{}, anchor_max{}, size{}, pivot{};
  int32_t          sibling = 0;
  float            spacing = 0, min_width = -1, preferred_width = -1;
  bool             ignore_layout = false;
};
struct GalaxyStarBinding {
  il2cpp::GcHandle                          owner;
  gb::Node                                  node;
  uint64_t                                  generation = 0;
  std::vector<std::shared_ptr<GalaxyShift>> shifts;
  il2cpp::GcHandle                          housing;
  bool                                      housing_ready = false;
  bool                                      rendering = false, restoring = false, releasing = false;
};
struct GalaxyState {
  bool                       installed = false, refreshing = false, maintaining = false, zoom_known = false, refresh = true;
  bool                       last_multi = false, last_housing = false;
  bool                       pending_layout_cleanup = false, pending_housing_cleanup = false;
  std::thread::id            thread;
  uint64_t                   generation = 0, mode_revision = 0;
  il2cpp::GcHandle           camera;
  float                      normalized = 0;
  ZoomLevels                 level      = ZoomLevels::None;
  const Config*              config     = nullptr;
  std::array<ZoomProfile, 2> profiles{};
  std::map<gb::Star*, std::shared_ptr<GalaxyStarBinding>> stars;
  std::map<gb::Filter*, il2cpp::GcHandle>                 filters;
  std::map<NavigationLOD*, il2cpp::GcHandle>              lods;
  std::map<gb::ZoomHandler*, il2cpp::GcHandle>            handlers;
  std::optional<HousingData>                              housing;
  gb::Star*                                               batching = nullptr;
  std::array<bool, 4>                                     combined{};
  bool                                                    saw_name = false;
};
GalaxyState& Galaxy()
{
  static auto* state = new GalaxyState;
  return *state;
}
bool GalaxyThread()
{ return Galaxy().installed && std::this_thread::get_id() == Galaxy().thread; }
std::shared_ptr<GalaxyStarBinding> GalaxyBinding(gb::Star* star)
{
  const auto it = Galaxy().stars.find(star);
  return it == Galaxy().stars.end() || it->second->owner.Get() != star ? nullptr : it->second;
}
bool SameGalaxyBinding(gb::Star* star, const std::shared_ptr<GalaxyStarBinding>& binding)
{
  if (!binding || GalaxyBinding(star) != binding || binding->releasing)
    return false;
  const auto node = star->Context();
  return node.galaxy == binding->node.galaxy && node.index == binding->node.index;
}
bool GalaxyActive(unity::GameObject* object)
{
  const auto value = object ? object->TryCall<"get_activeSelf", bool>() : il2cpp::CallResult<bool>{};
  return value && value.value;
}
bool GalaxySetActive(unity::GameObject* object, bool desired)
{
  if (!object)
    return false;
  const auto alive = unity::TryAlive(object);
  if (!alive || !alive.value)
    return false;
  auto value = object->TryCall<"get_activeSelf", bool>();
  if (value && value.value == desired)
    return true;
  (void)object->TryCall<"SetActive", void(bool)>(desired);
  value = object->TryCall<"get_activeSelf", bool>();
  return value && value.value == desired;
}
bool GalaxyRemoveHousing(GalaxyStarBinding& binding)
{
  auto* observed = binding.housing.Get();
  if (!observed) {
    binding.housing.Reset();
    binding.housing_ready = false;
    return true;
  }
  auto root = il2cpp::GcHandle::Strong(observed);
  if (!root)
    return false;
  const auto alive = unity::TryAlive(root.Get());
  if (!alive)
    return false;
  if (!alive.value) {
    binding.housing.Reset();
    binding.housing_ready = false;
    return true;
  }
  (void)GalaxySetActive(root.Get<unity::GameObject>(), false);
  if (!unity::UnityObject::Class::TryCall<"Destroy", void(unity::UnityObject*)>(
          reinterpret_cast<unity::UnityObject*>(root.Get())))
    return false;
  binding.housing.Reset(); // Unity owns the successfully scheduled deferred destruction.
  binding.housing_ready = false;
  return true;
}
bool GalaxyRestore(const std::shared_ptr<GalaxyStarBinding>& binding)
{
  if (!binding || binding->restoring)
    return false;
  binding->restoring = true;
  struct Done {
    bool& value;
    ~Done()
    { value = false; }
  } done{binding->restoring};
  // A collected moved child does not imply its surviving layout group/element disappeared. Restore those
  // snapshots first, just as the frozen implementation did, and retain the journal on unavailable custody.
  const auto restore_component = [](const il2cpp::GcHandle& weak, auto restore) {
    auto* observed = weak.Get();
    if (!observed)
      return true;
    auto keep = il2cpp::GcHandle::Strong(observed);
    if (!keep)
      return false;
    const auto alive = unity::TryAlive(keep.Get());
    return alive && (!alive.value || restore(keep.Get()));
  };
  while (!binding->shifts.empty()) {
    auto shift = binding->shifts.back(); // Native callbacks may mutate other records; retain this snapshot itself.
    if (!restore_component(shift->element, [&](Il2CppObject* object) {
          auto* element = reinterpret_cast<unity::LayoutElement*>(object);
          return element->TryCall<"set_minWidth", void(float)>(shift->min_width)
                 && element->TryCall<"set_preferredWidth", void(float)>(shift->preferred_width)
                 && element->TryCall<"set_ignoreLayout", void(bool)>(shift->ignore_layout);
        })
        || !restore_component(shift->group, [&](Il2CppObject* object) {
             return reinterpret_cast<unity::HorizontalLayoutGroup*>(object)
                 ->TryCall<"set_spacing", void(float)>(shift->spacing).succeeded;
           }))
      return false;
    auto* observed = shift->target.Get();
    if (!observed) {
      binding->shifts.pop_back();
      continue;
    }
    auto root = il2cpp::GcHandle::Strong(observed);
    if (!root)
      return false;
    auto* target = root.Get<unity::RectTransform>();
    const auto alive = unity::TryAlive(target);
    if (!alive)
      return false;
    if (!alive.value) {
      binding->shifts.pop_back();
      continue;
    }
    auto parent_root = il2cpp::GcHandle::Strong(shift->parent.Get());
    auto* parent = parent_root.Get<unity::Transform>();
    const auto parent_alive = parent ? unity::TryAlive(parent) : il2cpp::CallResult<bool>{};
    if (!parent_alive || !parent_alive.value
        || !target->TryCall<"SetParent", void(unity::Transform*, bool)>(parent, false)
        || !target->TryCall<"set_anchorMin", void(unity::Vector2)>(shift->anchor_min)
        || !target->TryCall<"set_anchorMax", void(unity::Vector2)>(shift->anchor_max)
        || !target->TryCall<"set_sizeDelta", void(unity::Vector2)>(shift->size)
        || !target->TryCall<"set_pivot", void(unity::Vector2)>(shift->pivot)
        || !target->TryCall<"SetSiblingIndex", void(int32_t)>(shift->sibling)
        || !target->TryCall<"set_anchoredPosition", void(unity::Vector2)>(shift->position))
      return false;
    binding->shifts.pop_back();
  }
  return true;
}
std::shared_ptr<GalaxyStarBinding> GalaxyTrack(gb::Star* star)
{
  if (!star->Bound())
    return {};
  const auto previous = Galaxy().stars.find(star);
  auto existing = previous == Galaxy().stars.end() ? nullptr : previous->second;
  if (existing && SameGalaxyBinding(star, existing))
    return existing;
  // An expired/replaced weak owner at the same address still owns its old cleanup journal. Do not overwrite it.

  if (existing && (!GalaxyRestore(existing) || !GalaxyRemoveHousing(*existing)))
    return {};
  const auto after_cleanup = Galaxy().stars.find(star);
  if (existing && (after_cleanup == Galaxy().stars.end() || after_cleanup->second != existing))
    return {}; // A reentrant bind/release owns its replacement record.
  auto next   = std::make_shared<GalaxyStarBinding>();
  next->owner = il2cpp::GcHandle::Weak(star);
  if (!next->owner)
    return {};
  next->node       = star->Context();
  next->generation = ++Galaxy().generation;
  Galaxy().stars.insert_or_assign(star, next);
  return next;
}
OverlaySelection GalaxySelection()
{
  const auto flags = Config::Get().graphics.galaxy_overlays;
  return OverlaySelection::FromFlags(flags[0], flags[1], flags[2], flags[3]);
}
ZoomLevels GalaxyEffective(gb::Star* star, ZoomLevels native)
{
  const auto& g      = Config::Get().graphics;
  auto        choice = ResolveDetail(g.galaxy_label_major, Galaxy().zoom_known, Galaxy().normalized);
  const auto  minor  = ResolveDetail(g.galaxy_label_minor, Galaxy().zoom_known, Galaxy().normalized);
  if (choice != minor) {
    auto world = il2cpp::GcHandle::Strong(gb::World::Instance());
    if (!world || !star->Bound())
      return native;
    const auto observed = world.Get<gb::World>()->Minor(star->Context());
    if (!observed)
      return native;
    if (observed.value)
      choice = minor;
  }
  return choice == Detail::Expanded  ? ZoomLevels::Near
         : choice == Detail::Compact ? ZoomLevels::Far
         : Galaxy().zoom_known       ? Galaxy().level
                                     : native;
}
std::shared_ptr<GalaxyShift> GalaxyCaptureShift(const std::shared_ptr<GalaxyStarBinding>& binding,
                                                unity::Transform*                         transform)
{
  if (!transform || binding->restoring || binding->releasing)
    return {};
  for (const auto& old : binding->shifts)
    if (old->target.Get() == transform)
      return {};
  auto  root = il2cpp::GcHandle::Strong(transform);
  auto* rect = root ? root.Get<unity::Transform>()->As<unity::RectTransform>() : nullptr;
  if (!rect)
    return {};
  auto shift          = std::make_shared<GalaxyShift>();
  shift->target       = il2cpp::GcHandle::Weak(rect);
  shift->parent       = il2cpp::GcHandle::Weak(rect->parent());
  const auto position = rect->TryCall<"get_anchoredPosition", unity::Vector2>();
  const auto amin     = rect->TryCall<"get_anchorMin", unity::Vector2>();
  const auto amax     = rect->TryCall<"get_anchorMax", unity::Vector2>();
  const auto size     = rect->TryCall<"get_sizeDelta", unity::Vector2>();
  const auto pivot    = rect->TryCall<"get_pivot", unity::Vector2>();
  const auto sibling  = rect->TryCall<"GetSiblingIndex", int32_t>();
  if (!shift->target || !shift->parent || !position || !amin || !amax || !size || !pivot || !sibling
      || !gb::Finite(position.value) || !gb::Finite(amin.value) || !gb::Finite(amax.value) || !gb::Finite(size.value)
      || !gb::Finite(pivot.value))
    return {};
  shift->position   = position.value;
  shift->anchor_min = amin.value;
  shift->anchor_max = amax.value;
  shift->size       = size.value;
  shift->pivot      = pivot.value;
  shift->sibling    = sibling.value;
  return shift;
}
void GalaxyTextLayout(gb::Star* star, const std::shared_ptr<GalaxyStarBinding>& binding, bool names, bool level)
{
  auto       localizer = il2cpp::GcHandle::Strong(star->HostileText());
  auto*      text      = localizer ? localizer.Get<TextLocalizer>()->Target() : nullptr;
  auto       text_root = il2cpp::GcHandle::Strong(text);
  auto*      graphic   = text ? text->As<unity::Component>() : nullptr;
  auto*      object    = graphic ? graphic->gameObject() : nullptr;
  const bool visible   = GalaxyActive(object);
  if (names) {
    star->UpdateName(false, false, level, false);
    (void)GalaxySetActive(object, visible);
  }
  if (!visible || !SameGalaxyBinding(star, binding))
    return;
  auto*      transform  = graphic->transform();
  auto       shift      = GalaxyCaptureShift(binding, transform);
  auto       row_root   = il2cpp::GcHandle::Strong(gb::HostileRow(star));
  auto*      row        = row_root.Get<unity::Transform>();
  auto*      row_object = row ? row->gameObject() : nullptr;
  auto*      group      = row_object ? row_object->GetComponent<unity::HorizontalLayoutGroup>() : nullptr;
  const auto spacing    = group ? group->TryCall<"get_spacing", float>() : il2cpp::CallResult<float>{};
  const auto width      = text ? text->TryCall<"get_preferredWidth", float>() : il2cpp::CallResult<float>{};
  const auto height     = text ? text->TryCall<"get_preferredHeight", float>() : il2cpp::CallResult<float>{};
  if (!shift || !row || !group || !spacing || !width || !height || !std::isfinite(spacing.value)
      || !std::isfinite(width.value) || !std::isfinite(height.value) || width.value <= 0 || height.value <= 0
      || !SameGalaxyBinding(star, binding))
    return;
  shift->group   = il2cpp::GcHandle::Weak(group);
  shift->spacing = spacing.value;
  if (!shift->group)
    return;
  binding->shifts.push_back(shift); // Record restoration before the first hierarchy mutation.
  auto* rect = shift->target.Get<unity::RectTransform>();
  rect->SetParent(row, false);
  group->Call<"set_spacing">(std::max(spacing.value, 10.f));
  rect->Call<"set_anchorMin">(unity::Vector2{0, .5f});
  rect->Call<"set_anchorMax">(unity::Vector2{0, .5f});
  rect->Call<"set_pivot">(unity::Vector2{0, .5f});
  rect->Call<"set_sizeDelta">(unity::Vector2{width.value, height.value});
  rect->Call<"SetAsLastSibling">();
}
void GalaxyIconLayout(gb::Star* star, const std::shared_ptr<GalaxyStarBinding>& binding, unity::Component* icon,
                      unity::GameObject* visibility)
{
  if (!icon || !GalaxyActive(visibility) || !SameGalaxyBinding(star, binding))
    return;
  auto  icon_root  = il2cpp::GcHandle::Strong(icon);
  auto  label_root = il2cpp::GcHandle::Strong(star->Name());
  auto* text       = label_root ? label_root.Get<TextLocalizer>()->Target() : nullptr;
  auto  text_root  = il2cpp::GcHandle::Strong(text);
  auto* label      = text ? text->As<unity::Component>() : nullptr;
  auto* name       = label ? label->transform() : nullptr;
  auto* row        = name ? name->parent() : nullptr;
  auto* target     = icon->transform();
  auto  shift      = GalaxyCaptureShift(binding, target);
  if (!shift || !row)
    return;
  auto* object  = icon->gameObject();
  auto* element = object ? object->GetComponent<unity::LayoutElement>() : nullptr;
  if (!element && object)
    element = object->AddComponent<unity::LayoutElement>();
  const auto min       = element ? element->TryCall<"get_minWidth", float>() : il2cpp::CallResult<float>{};
  const auto preferred = element ? element->TryCall<"get_preferredWidth", float>() : il2cpp::CallResult<float>{};
  const auto ignore    = element ? element->TryCall<"get_ignoreLayout", bool>() : il2cpp::CallResult<bool>{};
  if (!element || !min || !preferred || !ignore || !std::isfinite(min.value) || !std::isfinite(preferred.value)
      || !SameGalaxyBinding(star, binding))
    return;
  shift->element = il2cpp::GcHandle::Weak(element);
  if (!shift->element)
    return;
  shift->min_width       = min.value;
  shift->preferred_width = preferred.value;
  shift->ignore_layout   = ignore.value;
  binding->shifts.push_back(shift);
  element->Call<"set_ignoreLayout">(false);
  element->Call<"set_minWidth">(36.f);
  element->Call<"set_preferredWidth">(36.f);
  target->SetParent(row, false);
  auto index = name->TryCall<"GetSiblingIndex", int32_t>();
  auto own   = target->TryCall<"GetSiblingIndex", int32_t>();
  if (index && own)
    target->Call<"SetSiblingIndex">(index.value - (own.value < index.value ? 1 : 0));
}
void GalaxyHousing(gb::Star* star, const std::shared_ptr<GalaxyStarBinding>& binding, ZoomLevels level)
{
  auto& state = Galaxy();
  if (auto* marker = binding->housing.Get<unity::GameObject>())
    (void)GalaxySetActive(marker, false);
  if (!Config::Get().graphics.galaxy_station_housing || !state.housing || level != ZoomLevels::Near
      || !SameGalaxyBinding(star, binding))
    return;
  const auto id = gb::NodeId(star->Context());
  if (!id || state.housing->Contains(*id) != std::optional(true))
    return;
  auto  label_root = il2cpp::GcHandle::Strong(star->Name());
  auto  graphic    = il2cpp::GcHandle::Strong(label_root ? label_root.Get<TextLocalizer>()->Target() : nullptr);
  auto* label      = graphic ? graphic.Get<il2cpp::Object>()->As<unity::Component>() : nullptr;
  auto  name_root  = il2cpp::GcHandle::Strong(label ? label->transform() : nullptr);
  auto* name       = name_root.Get<unity::Transform>();
  auto  row_root   = il2cpp::GcHandle::Strong(name ? name->parent() : nullptr);
  auto* row        = row_root.Get<unity::Transform>();
  if (!label || !label->gameObject() || !label->gameObject()->activeInHierarchy() || !row || !row->gameObject()
      || !row->gameObject()->GetComponent<unity::HorizontalLayoutGroup>())
    return;
  if (binding->housing && !binding->housing_ready && !GalaxyRemoveHousing(*binding))
    return;
  auto* existing_marker = binding->housing.Get();
  auto  marker_root     = il2cpp::GcHandle::Strong(existing_marker);
  if (existing_marker && !marker_root)
    return;
  const auto marker_alive = marker_root ? unity::TryAlive(marker_root.Get()) : il2cpp::CallResult<bool>{true, false};
  if (!marker_alive)
    return;
  if (marker_root && !marker_alive.value) {
    binding->housing.Reset();
    binding->housing_ready = false;
    marker_root.Reset();
  }
  if (!marker_root) {
    auto  home_root  = il2cpp::GcHandle::Strong(star->Home());
    auto* home       = home_root.Get<unity::GameObject>();
    auto* type       = unity::Image::Class::TypeObject();
    auto  image_root = il2cpp::GcHandle::Strong(
        home && type ? home->Call<"GetComponentInChildren", unity::Image*>(type, true) : nullptr);
    auto* image       = image_root.Get<unity::Image>();
    auto  sprite_root = il2cpp::GcHandle::Strong(image ? image->sprite() : nullptr);
    if (!sprite_root || !unity::Alive(sprite_root.Get()) || !SameGalaxyBinding(star, binding))
      return;
    auto* rect = unity::CreateUiObject("CommunityMod.StationHousing", row, marker_root);
    if (!rect || !marker_root)
      return;
    // Publish custody before native setters; release/rebind can safely destroy a partially built marker.
    binding->housing = std::move(marker_root);
    marker_root      = il2cpp::GcHandle::Strong(binding->housing.Get());
    if (!marker_root)
      return;
    auto* object = marker_root.Get<unity::GameObject>();
    object->Call<"set_layer">(home->Get<"layer", int32_t>());
    rect->Call<"set_sizeDelta">(unity::Vector2{36.f, 32.f});
    auto* element = object->AddComponent<unity::LayoutElement>();
    if (!element) {
      (void)GalaxyRemoveHousing(*binding);
      return;
    }
    element->Call<"set_minWidth">(36.f);
    element->Call<"set_preferredWidth">(36.f);
    element->Call<"set_preferredHeight">(32.f);
    element->Call<"set_flexibleWidth">(0.f);
    il2cpp::GcHandle glyph_root;
    auto*            glyph = unity::CreateUiObject("HousingIcon", rect, glyph_root);
    if (!glyph || !glyph_root) {
      (void)GalaxyRemoveHousing(*binding);
      return;
    }
    auto* glyph_object = glyph_root.Get<unity::GameObject>();
    glyph_object->Call<"set_layer">(home->Get<"layer", int32_t>());
    glyph->SetAnchorMin({.5f, .5f});
    glyph->SetAnchorMax({.5f, .5f});
    glyph->Call<"set_pivot">(unity::Vector2{.5f, .5f});
    glyph->Call<"set_sizeDelta">(unity::Vector2{26.f, 26.f});
    glyph->Call<"set_anchoredPosition">(unity::Vector2{});
    auto* icon = glyph_object->AddComponent<unity::Image>();
    if (!icon) {
      (void)GalaxyRemoveHousing(*binding);
      return;
    }
    if (!icon->TryCall<"set_sprite", void(unity::Sprite*)>(sprite_root.Get<unity::Sprite>())
        || !icon->TryCall<"set_color", void(unity::Color)>(unity::Color{.45f, .9f, 1.f, 1.f})
        || !icon->TryCall<"set_preserveAspect", void(bool)>(true)
        || !icon->TryCall<"set_raycastTarget", void(bool)>(false) || !GalaxySetActive(glyph_object, true))
      return; // Custody remains incomplete; the next render cleans/rebuilds this marker.
    const auto assigned = icon->TryCall<"get_sprite", unity::Sprite*>();
    if (!assigned || assigned.value != sprite_root.Get<unity::Sprite>())
      return;
    if (!SameGalaxyBinding(star, binding))
      return;
    binding->housing_ready = true;
  }
  if (!SameGalaxyBinding(star, binding))
    return;
  auto* object    = marker_root.Get<unity::GameObject>();
  auto* transform = object->transform();
  transform->SetParent(row, false);
  const auto index = name->TryCall<"GetSiblingIndex", int32_t>();
  const auto own   = transform->TryCall<"GetSiblingIndex", int32_t>();
  if (index && own)
    transform->Call<"SetSiblingIndex">(index.value - (own.value < index.value ? 1 : 0));
  (void)GalaxySetActive(object, true);
}

void NavigationStarEntityWidget_UpdateStarData_Hook(auto original, gb::Star* star, ZoomLevels level)
{
  if (!GalaxyThread())
    return original(star, level);
  auto binding = GalaxyTrack(star);
  if (!binding || binding->rendering || binding->restoring || binding->releasing)
    return original(star, level);
  auto keep          = il2cpp::GcHandle::Strong(star);
  binding->rendering = true;
  struct Done {
    bool& value;
    ~Done()
    { value = false; }
  } done{binding->rendering};
  if (auto* marker = binding->housing.Get<unity::GameObject>())
    (void)GalaxySetActive(marker, false);
  if (!GalaxyRestore(binding) || !SameGalaxyBinding(star, binding))
    return original(star, level);
  const auto effective = GalaxyEffective(star, level);
  const bool multi     = Config::Get().graphics.galaxy_multi_select;
  auto       mode      = gb::Director::Mode();
  if (multi && mode && !Galaxy().batching) {
    gb::Director::SetMode(GalaxySelection().Representative(*mode));
    mode = gb::Director::Mode();
  }
  const auto selection = multi ? GalaxySelection() : OverlaySelection::FromNativeMode(mode.value_or(-1));
  if (!multi || !selection.Multiple() || !mode || Galaxy().batching) {
    original(star, effective);
    if (!SameGalaxyBinding(star, binding))
      return;
    if (multi && selection.Contains(2))
      GalaxyTextLayout(star, binding, false, false);
  } else {
    struct ModeScope {
      int32_t              original;
      gb::Star*            previous;
      TemporaryOverlayMode lease;
      const Config*        config;
      bool                 Current() const
      { return config == &Config::Get() && lease.Owns(Galaxy().mode_revision, gb::Director::Mode()); }
      bool Set(int32_t value)
      {
        if (!Current())
          return false;
        gb::Director::SetMode(value);
        lease.value = value;
        return Current();
      }
      void Restore()
      {
        if (lease.Owns(Galaxy().mode_revision, gb::Director::Mode()))
          gb::Director::SetMode(original);
        Galaxy().batching = previous;
      }
      ~ModeScope()
      { Restore(); }
    } scope{*mode, Galaxy().batching, {Galaxy().mode_revision, *mode}, &Config::Get()};
    Galaxy().batching = star;
    Galaxy().combined = {};
    Galaxy().saw_name = false;
    std::array<il2cpp::GcHandle, 4> roots;
    roots[0]        = il2cpp::GcHandle::Strong(star->ResourceContainer());
    roots[1]        = il2cpp::GcHandle::Strong(star->HostileContainer());
    roots[2]        = il2cpp::GcHandle::Strong(star->HazardContainer());
    auto* resources = star->Resources();
    roots[3] = il2cpp::GcHandle::Strong(resources ? resources->Call<"get_gameObject", unity::GameObject*>() : nullptr);
    std::array<bool, 4> active{};
    bool                composed = false;
    // Preserve PLAY's single-pass path where the client's own helpers are available; otherwise use its bounded
    // native-per-overlay fallback. Both paths retain native visited/range/status eligibility decisions.
    static const bool helpers = bool(gb::Star::Class::Method<void(bool, bool, bool)>("UpdateResourcesList"))
                                && bool(gb::Star::Class::Method<void(bool, bool)>("UpdateHostilesList"));
    if (helpers) {
      if (!scope.Set(3))
        return;
      original(star, effective);
      if (!scope.Current() || !SameGalaxyBinding(star, binding))
        return;
      const auto flags = Galaxy().combined;
      if (Galaxy().saw_name && !flags[0] && !flags[1]) {
        const bool mining    = selection.Contains(1) && effective == ZoomLevels::Near;
        const bool hostiles  = selection.Contains(2) && effective == ZoomLevels::Near;
        const auto resources = star->TryCall<"UpdateResourcesList", void(bool, bool, bool)>(mining, false, flags[2]);
        const auto enemies   = resources ? star->TryCall<"UpdateHostilesList", void(bool, bool)>(hostiles, flags[2])
                                         : il2cpp::CallResult<void>{};
        if (!scope.Current())
          return;
        if (resources && enemies && SameGalaxyBinding(star, binding)) {
          for (size_t i = 0; i < roots.size(); ++i)
            active[i] = GalaxyActive(roots[i].Get<unity::GameObject>());
          active[2]         = selection.Contains(3) && active[2];
          Galaxy().combined = {selection.Contains(1) && flags[3], selection.Contains(2) && flags[3], flags[2],
                               selection.Contains(3) && flags[3]};
          composed          = true;
        }
      }
    }
    if (!composed) {
      Galaxy().combined = {};
      Galaxy().saw_name = false;
      active            = {};
    }
    for (int32_t pass = 0; !composed && pass < 4; ++pass) {
      if (!selection.Contains(pass))
        continue;
      if (!scope.Set(pass))
        return;
      original(star, effective);
      if (!scope.Current() || !SameGalaxyBinding(star, binding))
        return;
      active[0] |= GalaxyActive(roots[0].Get<unity::GameObject>());
      if (pass >= 2)
        active[pass - 1] = GalaxyActive(roots[pass - 1].Get<unity::GameObject>());
      if (pass == 1)
        active[3] = GalaxyActive(roots[3].Get<unity::GameObject>());
    }
    if (!scope.Current())
      return;
    scope.Restore();
    for (size_t i = 0; i < roots.size(); ++i)
      if (roots[i])
        (void)GalaxySetActive(roots[i].Get<unity::GameObject>(), active[i]);
    const auto flags = Galaxy().combined;
    if (Galaxy().saw_name)
      star->UpdateName(flags[0] && !flags[1], flags[1], flags[2], flags[3] && !flags[0] && !flags[1]);
    if (!SameGalaxyBinding(star, binding))
      return;
    GalaxyTextLayout(star, binding, selection.Contains(0), flags[2]);
  }
  if (!SameGalaxyBinding(star, binding))
    return;
  if (multi || Config::Get().graphics.galaxy_station_housing) {
    auto* anomaly = star->Anomaly();
    GalaxyIconLayout(star, binding, anomaly ? anomaly->GetComponent<unity::Component>() : nullptr, anomaly);
    if (selection.Contains(3))
      GalaxyIconLayout(star, binding, star->Hazard(), star->HazardContainer());
  }
  GalaxyHousing(star, binding, effective);
}
void NavigationStarEntityWidget_UpdateStarName_Hook(auto original, gb::Star* star, bool resources, bool hostiles,
                                                    bool level, bool hazards)
{
  if (GalaxyThread() && Galaxy().batching == star) {
    Galaxy().saw_name = true;
    auto& flags       = Galaxy().combined;
    flags[0] |= resources;
    flags[1] |= hostiles;
    flags[2] |= level;
    flags[3] |= hazards;
  }
  original(star, resources, hostiles, level, hazards);
}
void NavigationStarEntityWidget_OnDidBindContext_Hook(auto original, gb::Star* star)
{
  if (GalaxyThread())
    if (auto binding = GalaxyBinding(star)) {
      (void)GalaxyRemoveHousing(*binding);
      (void)GalaxyRestore(binding);
    }
  original(star);
  if (GalaxyThread())
    (void)GalaxyTrack(star);
}
void NavigationStarEntityWidget_OnAboutToReleaseContext_Hook(auto original, gb::Star* star)
{
  auto binding = GalaxyThread() ? GalaxyBinding(star) : nullptr;
  if (binding) {
    binding->releasing = true;
    (void)GalaxyRemoveHousing(*binding);
    (void)GalaxyRestore(binding);
  }
  original(star);
  if (binding && GalaxyBinding(star) == binding && binding->shifts.empty() && !binding->housing)
    Galaxy().stars.erase(star);
}
void HudNavigationViewController_ChangeGalaxyViewInfo_Hook(auto original, il2cpp::Object* hud, int32_t mode)
{
  if (GalaxyThread() && mode >= 0 && mode <= 3)
    ++Galaxy().mode_revision;
  if (GalaxyThread() && Config::Get().graphics.galaxy_multi_select && mode >= 0 && mode <= 3) {
    galaxy_controls::SetOverlay(mode, !GalaxySelection().Contains(mode));
    original(hud, GalaxySelection().Representative(mode));
  } else
    original(hud, mode);
}
void GalaxyToggleHandler_UpdateSelectedAnimation_Hook(auto original, il2cpp::Object* handler, bool normal,
                                                      bool hostiles, bool mining, bool hazards)
{
  if (GalaxyThread() && Config::Get().graphics.galaxy_multi_select) {
    const auto selection = GalaxySelection();
    normal               = selection.Contains(0);
    mining               = selection.Contains(1);
    hostiles             = selection.Contains(2);
    hazards              = selection.Contains(3);
  }
  original(handler, normal, hostiles, mining, hazards);
}
void GalaxyDataFilterSystem_RebuildCullingGroup_Hook(auto original, gb::Filter* filter)
{
  if (!GalaxyThread())
    return original(filter);
  auto root = il2cpp::GcHandle::Strong(filter);
  Galaxy().filters.insert_or_assign(filter, il2cpp::GcHandle::Weak(filter));
  const auto current = filter->Current();
  struct Restore {
    gb::Filter* filter;
    ZoomLevels  level;
    ~Restore()
    { filter->SetCurrent(level); }
  } restore{filter, current};
  if (Galaxy().zoom_known && current == ZoomLevels::Far
      && ResolveDetail(Config::Get().graphics.galaxy_label_minor, true, Galaxy().normalized) == Detail::Expanded)
    filter->SetCurrent(ZoomLevels::Middle);
  original(filter);
}
void NavigationZoomEventHandler_OnZoomChanged_Hook(auto original, gb::ZoomHandler* handler, ZoomLevels level)
{
  if (!GalaxyThread())
    return original(handler, level);
  auto  keep = il2cpp::GcHandle::Strong(handler);
  auto* star = gb::StarFor(handler);
  if (!star)
    star = gb::StarFor(handler->Lod());
  if (!star || !star->Bound())
    return original(handler, level);
  Galaxy().handlers.insert_or_assign(handler, il2cpp::GcHandle::Weak(handler));
  original(handler, GalaxyEffective(star, level));
}
// This family uses different receiver and argument contracts. Native entry aliasing must not chain a star
// lifecycle callback into a HUD/filter callback, even on platforms where the generic HookSet allows chains.
bool GalaxyTargetsDistinct(std::span<void* const> entries)
{
  for (size_t i = 0; i < entries.size(); ++i)
    if (entries[i] && std::ranges::contains(entries.first(i), entries[i]))
      return false;
  return true;
}
bool InstallGalaxyLabels()
{
  if (!gb::Star::LayoutReady() || !gb::World::MinorMethod() || !gb::Director::ModeField()
      || !gb::Filter::Class::HasField<"_currentLevel", ZoomLevels>()
      || !gb::Filter::Class::HasField<"_cullingDirty", bool>()
      || !gb::ZoomHandler::Class::HasField<"_navigationLod", NavigationLOD*>()
      || !BaseListContainer::Class::HasField<"m_container", unity::Transform*>() || !EnsureNavigationLodUpdates()
      || !EnsureNavigationZoomUpdates())
    return false;
  for (const char* key :
       {"galaxy_overlay_default", "galaxy_overlay_mining", "galaxy_overlay_hostiles", "galaxy_overlay_hazards"})
    (void)runtime_config::RegisterSetting("graphics", key);
  const std::array entries{
      il2cpp::CheckedEntry(gb::Star::Class::Method<void(ZoomLevels)>("UpdateStarData")),
      il2cpp::CheckedEntry(gb::Star::Class::Method<void(bool, bool, bool, bool)>("UpdateStarName")),
      il2cpp::CheckedEntry(gb::Star::Class::Method<void()>("OnDidBindContext")),
      il2cpp::CheckedEntry(gb::Star::Class::Method<void()>("OnAboutToReleaseContext")),
      il2cpp::CheckedEntry(gb::Hud::Method<void(int32_t)>("ChangeGalaxyViewInfo")),
      il2cpp::CheckedEntry(gb::ToggleHandler::Method<void(bool, bool, bool, bool)>("UpdateSelectedAnimation")),
      il2cpp::CheckedEntry(gb::ZoomHandler::Class::Method<void(ZoomLevels)>("OnZoomChanged")),
      il2cpp::CheckedEntry(gb::Filter::Class::Method<void()>("RebuildCullingGroup"))};
  if (!GalaxyTargetsDistinct(entries)) {
    spdlog::warn("[GalaxyLabels] incompatible callbacks share a native entry; leaving native labels unchanged");
    return false;
  }
  il2cpp::HookSet hooks("GalaxyLabels");
  IL2CPP_STAGE_AT(hooks, entries[0], NavigationStarEntityWidget_UpdateStarData_Hook);
  IL2CPP_STAGE_AT(hooks, entries[1], NavigationStarEntityWidget_UpdateStarName_Hook);
  IL2CPP_STAGE_AT(hooks, entries[2], NavigationStarEntityWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, entries[3], NavigationStarEntityWidget_OnAboutToReleaseContext_Hook);
  IL2CPP_STAGE_AT(hooks, entries[4], HudNavigationViewController_ChangeGalaxyViewInfo_Hook);
  IL2CPP_STAGE_AT(hooks, entries[5], GalaxyToggleHandler_UpdateSelectedAnimation_Hook);
  IL2CPP_STAGE_AT(hooks, entries[6], NavigationZoomEventHandler_OnZoomChanged_Hook);
  IL2CPP_STAGE_AT(hooks, entries[7], GalaxyDataFilterSystem_RebuildCullingGroup_Hook);
  if (!hooks.Commit())
    return false;
  auto& state     = Galaxy();
  state.thread    = std::this_thread::get_id();
  state.installed = true;
  state.last_multi = Config::Get().graphics.galaxy_multi_select;
  state.last_housing = Config::Get().graphics.galaxy_station_housing;
  state.housing   = HousingData::Parse(g_embeddedStationHousing, g_embeddedStationHousing_SIZE);
  if (!state.housing)
    spdlog::warn("[GalaxyLabels] housing dataset unavailable; native labels and overlays remain available");
  return true;
}
// Shared by the three refresh registries. A failed new strong handle is not proof that a weak target expired.
// Snapshot custody before invoking callbacks, which can synchronously release or replace registry entries.
template <class Registry, class Action> void GalaxyRefreshRegistered(Registry& registry, Action action)
{
  std::vector<il2cpp::GcHandle> roots;
  for (auto it = registry.begin(); it != registry.end();) {
    auto* observed = it->second.Get();
    if (!observed) {
      it = registry.erase(it);
      continue;
    }
    auto root = il2cpp::GcHandle::Strong(observed);
    if (root)
      roots.push_back(std::move(root));
    ++it;
  }
  for (auto& root : roots)
    action(root.Get());
}

void GalaxyRefreshStars(ZoomLevels level)
{
  auto& state = Galaxy();
  std::vector<std::pair<gb::Star*, std::shared_ptr<GalaxyStarBinding>>> stars;
  for (const auto& entry : state.stars)
    stars.push_back(entry);
  for (const auto& [star, binding] : stars) {
    auto* observed = binding->owner.Get();
    auto root = il2cpp::GcHandle::Strong(observed);
    if (observed && !root)
      continue;
    const auto alive = root ? unity::TryAlive(root.Get()) : il2cpp::CallResult<bool>{true, false};
    if (!alive)
      continue;
    if (!alive.value) {
      const auto current = state.stars.find(star);
      if (current != state.stars.end() && current->second == binding && GalaxyRestore(binding)
          && GalaxyRemoveHousing(*binding)) {
        const auto after_cleanup = state.stars.find(star);
        if (after_cleanup != state.stars.end() && after_cleanup->second == binding)
          state.stars.erase(after_cleanup);
      }
      continue;
    }
    if (SameGalaxyBinding(star, binding) && unity::ActiveAndEnabled(star))
      star->Update(level);
  }
}
// Settings disable/reload must undo pooled inactive widgets too; they may not receive another Data callback.
// Reuse the existing refresh/zoom owner. A failed transition remains pending until its retained journals/assets
// are discharged; observing the disabled setting once must not consume the only opportunity to clean them up.
void GalaxyRefreshDisabledPresentation()
{
  auto& state = Galaxy();
  if (state.maintaining)
    return;
  const bool multi = Config::Get().graphics.galaxy_multi_select;
  const bool housing = Config::Get().graphics.galaxy_station_housing;
  state.pending_layout_cleanup = !multi && (state.pending_layout_cleanup || state.last_multi);
  state.pending_housing_cleanup = !housing && (state.pending_housing_cleanup || state.last_housing);
  state.last_multi = multi;
  state.last_housing = housing;
  const bool restore_layouts = state.pending_layout_cleanup;
  const bool remove_housing = state.pending_housing_cleanup;
  if (!restore_layouts && !remove_housing)
    return;
  state.maintaining = true;
  struct Done {
    bool& active;
    ~Done() { active = false; }
  } done{state.maintaining};
  std::vector<std::pair<gb::Star*, std::shared_ptr<GalaxyStarBinding>>> records;
  for (const auto& record : state.stars)
    records.push_back(record);
  for (const auto& [star, binding] : records) {
    const auto current = state.stars.find(star);
    if (current == state.stars.end() || current->second != binding)
      continue;
    if (restore_layouts && !Config::Get().graphics.galaxy_multi_select)
      (void)GalaxyRestore(binding);
    if (remove_housing && !Config::Get().graphics.galaxy_station_housing)
      (void)GalaxyRemoveHousing(*binding);
  }
  // Native callbacks can replace records or re-enable presentation. Inspect current ownership rather than
  // clearing a latch based only on a stale snapshot or a setter's return value. Re-enabling cancels disabled-only
  // work; the ordinary Data/bind/release paths still restore old journals before reusing their layout/assets.
  state.pending_layout_cleanup = restore_layouts && !Config::Get().graphics.galaxy_multi_select
                                 && std::ranges::any_of(state.stars, [](const auto& entry) {
                                      return !entry.second->shifts.empty();
                                    });
  state.pending_housing_cleanup = remove_housing && !Config::Get().graphics.galaxy_station_housing
                                  && std::ranges::any_of(state.stars, [](const auto& entry) {
                                       return bool(entry.second->housing);
                                     });
}
} // namespace

bool galaxy_controls::LabelsAvailable()
{ return Galaxy().installed; }
bool galaxy_controls::HousingAvailable()
{ return Galaxy().installed && Galaxy().housing.has_value(); }
void galaxy_controls::Refresh()
{
  Galaxy().refresh = true;
  if (GalaxyThread() && !Galaxy().refreshing)
    GalaxyRefreshDisabledPresentation();
}
void galaxy_controls::SetOverlay(int32_t mode, bool enabled)
{
  if (!GalaxyThread() || mode < 0 || mode > 3)
    return;
  auto before = GalaxySelection();
  if (before.Contains(mode) == enabled)
    return;
  ++Galaxy().mode_revision;
  const auto           after = before.Toggle(mode);
  constexpr std::array keys{"galaxy_overlay_default", "galaxy_overlay_mining", "galaxy_overlay_hostiles",
                            "galaxy_overlay_hazards"};
  for (size_t i = 0; i < keys.size(); ++i) {
    Config::Get().graphics.galaxy_overlays[i] = after.Contains(static_cast<int32_t>(i));
    runtime_config::SaveSetting("graphics", keys[i], Config::Get().graphics.galaxy_overlays[i]);
  }
  Galaxy().refresh = true;
}
ZoomLevels galaxy_controls::LodLevel(NavigationLOD* lod, ZoomLevels native)
{
  if (!GalaxyThread())
    return native;
  const auto& graphics = Config::Get().graphics;
  if (graphics.galaxy_label_major.mode == ZoomMode::Native && graphics.galaxy_label_minor.mode == ZoomMode::Native)
    return native;
  auto* star = gb::StarFor(lod);
  if (!star || !star->Bound())
    return native;
  auto keep = il2cpp::GcHandle::Strong(star);
  Galaxy().lods.insert_or_assign(lod, il2cpp::GcHandle::Weak(lod));
  return GalaxyEffective(star, native);
}
void galaxy_controls::AfterZoom(NavigationZoom* zoom)
{
  auto& state = Galaxy();
  if (!GalaxyThread() || state.refreshing)
    return;
  GalaxyRefreshDisabledPresentation(); // Also runs before leaving the Galaxy scope.
  const auto depth = zoom ? zoom->TryDepth() : il2cpp::CallResult<NodeDepth>{};
  if (!depth) {
    state.zoom_known = false;
    return;
  }
  if (depth.value != NodeDepth::Galaxy) {
    if (state.camera.Get() == zoom) {
      state.camera.Reset();
      state.zoom_known = false;
    }
    return;
  }
  const auto normalized_observation = zoom->TryNormalizedZoom();
  const auto level_observation = zoom->TryZoomLevel();
  if (!normalized_observation || !level_observation || !std::isfinite(normalized_observation.value)
      || level_observation.value < ZoomLevels::Near || level_observation.value > ZoomLevels::Far) {
    state.zoom_known = false;
    return;
  }
  const auto normalized = normalized_observation.value;
  const auto level = level_observation.value;
  const auto&      config = Config::Get();
  const std::array profiles{config.graphics.galaxy_label_major, config.graphics.galaxy_label_minor};
  bool             changed = !state.zoom_known || state.camera.Get() != zoom || state.level != level || state.refresh
                             || state.config != &config;
  for (size_t i = 0; i < profiles.size(); ++i)
    changed |= profiles[i] != state.profiles[i]
               || ResolveDetail(profiles[i], state.zoom_known, state.normalized)
                      != ResolveDetail(profiles[i], true, normalized);
  state.camera     = il2cpp::GcHandle::Weak(zoom);
  state.zoom_known = bool(state.camera);
  state.level      = level;
  state.normalized = normalized;
  state.config     = &config;
  state.profiles   = profiles;
  state.refresh    = false;
  if (!changed)
    return;
  if (config.graphics.galaxy_multi_select) {
    const auto mode = gb::Director::Mode();
    if (mode)
      gb::Director::SetMode(GalaxySelection().Representative(*mode));
  }
  state.refreshing = true;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{state.refreshing};
  // Every refresh takes operation-local custody before invoking reentrant native callbacks.
  GalaxyRefreshRegistered(state.filters, [](Il2CppObject* object) { reinterpret_cast<gb::Filter*>(object)->Dirty(); });
  GalaxyRefreshRegistered(state.lods, [level](Il2CppObject* object) {
    auto* lod = reinterpret_cast<NavigationLOD*>(object);
    if (unity::Alive(lod) && gb::StarFor(lod)) {
      lod->SetTargetLevel(level);
      lod->UpdateLOD(level);
    }
  });
  GalaxyRefreshRegistered(state.handlers, [level](Il2CppObject* object) {
    auto* handler = reinterpret_cast<gb::ZoomHandler*>(object);
    if (unity::ActiveAndEnabled(handler))
      handler->Call<"OnZoomChanged">(level);
  });
  GalaxyRefreshStars(level);
}
const Feature kGalaxyLabels{"GalaxyLabels", &Config::Patches::zoomhooks, InstallGalaxyLabels};
