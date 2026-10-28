#pragma once

// Production Galaxy presentation helpers/hooks in the existing controlled runtime.
// Only native layout/marker bodies are substituted. No Unity scene or player files are used.
#include "../src/mod/features/galaxy.cc"

namespace
{
struct GalaxyPresentationGroup {
  Il2CppObject header;
  float spacing = 10;
};
struct GalaxyPresentationStar {
  Il2CppObject header;
  galaxy_binding::Node context;
};
struct GalaxyPresentationMarker {
  Il2CppObject header;
  bool active = true;
};
int galaxy_presentation_spacing_calls = 0, galaxy_presentation_destroy_calls = 0;
bool galaxy_presentation_spacing_throws = false, galaxy_presentation_destroy_throws = false;
gb::Star* galaxy_presentation_replacement_key = nullptr;
std::shared_ptr<GalaxyStarBinding> galaxy_presentation_replacement;
Il2CppObject galaxy_presentation_world{};
int galaxy_presentation_minor_calls = 0;
bool galaxy_presentation_minor_throws = false;
Il2CppObject* GalaxyPresentationWorld(const MethodInfo*)
{ return &galaxy_presentation_world; }
bool GalaxyPresentationMinor(Il2CppObject*, gb::Node& node, const MethodInfo*)
{
  ++galaxy_presentation_minor_calls;
  if (galaxy_presentation_minor_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return node.index == 2;
}

void GalaxyPresentationSpacing(GalaxyPresentationGroup* group, float value, const MethodInfo*)
{
  Require(std::ranges::count_if(warp_roots, [group](const auto& root) { return root.second == &group->header; }) >= 2,
          "layout restoration holds operation-local custody in addition to the journal's weak handle");
  ++galaxy_presentation_spacing_calls;
  if (galaxy_presentation_spacing_throws)
    throw Il2CppExceptionWrapper(&thrown);
  group->spacing = value;
  if (galaxy_presentation_replacement_key)
    Galaxy().stars[galaxy_presentation_replacement_key] = galaxy_presentation_replacement;
}
bool GalaxyPresentationActive(GalaxyPresentationMarker* marker, const MethodInfo*)
{ return marker->active; }
void GalaxyPresentationSetActive(GalaxyPresentationMarker* marker, bool value, const MethodInfo*)
{ marker->active = value; }
void GalaxyPresentationDestroy(Il2CppObject*, const MethodInfo*)
{
  ++galaxy_presentation_destroy_calls;
  if (galaxy_presentation_destroy_throws)
    throw Il2CppExceptionWrapper(&thrown);
}
void GalaxyPresentationExpire(Il2CppObject* object)
{
  // Called only outside native operations, when this object's fixture handles are weak.
  for (auto& [_, target] : warp_roots)
    if (target == object)
      target = nullptr;
}
std::shared_ptr<GalaxyShift> GalaxyPresentationShift(GalaxyPresentationGroup& group, float before)
{
  auto shift = std::make_shared<GalaxyShift>();
  shift->group = il2cpp::GcHandle::Weak(&group.header);
  shift->spacing = before;
  return shift; // A collected moved child, with a surviving row needing restoration.
}

void GalaxyPresentationCases(std::string_view mode)
{
  auto& group_class = Define("UnityEngine.UI", "HorizontalLayoutGroup", IL2CPP_TYPE_CLASS, &component_class);
  AddMethod(group_class, "set_spacing", reinterpret_cast<void*>(&GalaxyPresentationSpacing), T(void_class),
            {T(float_class)});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&GalaxyPresentationActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&GalaxyPresentationSetActive), T(void_class),
            {T(bool_class)});
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&GalaxyPresentationDestroy), T(void_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto& node_class = Define("Digit.PrimeServer.Models", "GalaxyNode", IL2CPP_TYPE_VALUETYPE, nullptr,
                            sizeof(galaxy_binding::Node));
  auto& galaxy_class = Define("Digit.PrimeServer.Models", "OptimisedGalaxy", IL2CPP_TYPE_CLASS, &object_class);
  AddField(node_class, "_galaxy", galaxy_class, 0);
  AddField(node_class, "_index", int_class, offsetof(galaxy_binding::Node, index));
  auto& world_class = Define("Digit.Prime.Navigation", "GameWorldManager", IL2CPP_TYPE_CLASS, &object_class);
  galaxy_presentation_world.klass = &world_class.klass;
  const auto& world_instance = AddMethod(world_class, "get_Instance", reinterpret_cast<void*>(&GalaxyPresentationWorld),
                                          T(world_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(world_class, "Instance", &world_instance);
  AddMethod(world_class, "IsMinorNode", reinterpret_cast<void*>(&GalaxyPresentationMinor), T(bool_class), {Ref(node_class)});
  auto& star_class = Define("Digit.Prime.Navigation", "NavigationStarEntityWidget", IL2CPP_TYPE_CLASS, &component_class);
  AddField(star_class, "m_context", node_class, offsetof(GalaxyPresentationStar, context));
  Il2CppObject galaxy{&galaxy_class.klass};
  GalaxyPresentationStar native_star{{&star_class.klass}, {reinterpret_cast<gb::OptimisedGalaxy*>(&galaxy), 2}};
  auto* star = reinterpret_cast<gb::Star*>(&native_star);
  GalaxyPresentationGroup group{{&group_class.klass}};
  GalaxyPresentationMarker marker{{&game_object_class.klass}};
  const auto roots = warp_roots.size();
  Galaxy().installed = true;
  Galaxy().thread = std::this_thread::get_id();

  if (mode == "galaxy-presentation-alias") {
    std::array<void*, 8> entries{};
    for (size_t i = 0; i < entries.size(); ++i)
      entries[i] = reinterpret_cast<void*>(0x1000 + 0x100 * i);
    Require(GalaxyTargetsDistinct(entries), "distinct Galaxy callback entries pass their feature-specific preflight");
    for (size_t i = 1; i < entries.size(); ++i) {
      const auto saved = entries[i];
      entries[i] = entries[0];
      Require(!GalaxyTargetsDistinct(entries), "aliased receiver/argument callback entries decline before staging");
      entries[i] = saved;
    }
    entries[0] = entries[1] = nullptr;
    Require(GalaxyTargetsDistinct(entries), "missing targets remain the transaction's missing-target decision, not aliasing");
    Galaxy().installed = false;
    Require(warp_roots.size() == roots, "alias preflight performs no native mutations or ownership acquisition");
    std::cout << "Galaxy callback-family entry alias preflight passed; full native installer remains separate\n";
    return;
  }

  if (mode == "galaxy-presentation-profiles") {
    auto& graphics = Config::Get().graphics;
    Galaxy().zoom_known = true;
    Galaxy().normalized = .25f;
    Galaxy().level = ZoomLevels::Middle;
    graphics.galaxy_label_major = {ZoomMode::Native, .5f};
    graphics.galaxy_label_minor = {ZoomMode::Always, .5f};
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Near,
            "actual minor classification applies the independent minor Always profile");
    native_star.context.index = 1;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Middle,
            "major Native profile retains the observed native tier instead of inheriting minor Always");
    graphics.galaxy_label_major = {ZoomMode::Always, .5f};
    graphics.galaxy_label_minor = {ZoomMode::Native, .5f};
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Near,
            "independent major Always profile expands the major system");
    native_star.context.index = 2;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Middle,
            "minor Native profile does not inherit major Always");
    galaxy_presentation_minor_throws = true;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Far,
            "unavailable minor classification retains the native callback tier rather than guessing major/minor");
    galaxy_presentation_minor_throws = false;
    warp_root_failure = true;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Far,
            "classification custody failure does not authorize a forced profile");
    warp_root_failure = false;
    graphics.galaxy_label_major = graphics.galaxy_label_minor = {ZoomMode::Threshold, .5f};
    const auto calls = galaxy_presentation_minor_calls;
    Galaxy().normalized = .5f;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Near,
            "measured threshold endpoint is inclusive for the native presentation consumer");
    Galaxy().normalized = .5001f;
    Require(GalaxyEffective(star, ZoomLevels::Near) == ZoomLevels::Far,
            "threshold changes inside one native tier immediately change effective label detail");
    Require(galaxy_presentation_minor_calls == calls,
            "identical profile decisions avoid unnecessary managed minor-node classification");
    Galaxy().zoom_known = false;
    Require(GalaxyEffective(star, ZoomLevels::Middle) == ZoomLevels::Middle,
            "unknown normalized zoom retains native presentation instead of using zero as a measurement");
    graphics.galaxy_label_major = graphics.galaxy_label_minor = {ZoomMode::Always, .5f};
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Near,
            "explicit Always remains independent of a normalized zoom observation");
    Galaxy().installed = false;
    Require(warp_roots.size() == roots, "profile classification releases every temporary native owner");
    std::cout << "Production Galaxy major/minor profiles, within-tier thresholds and unknown classification passed\n";
    return;
  }

  if (mode == "galaxy-presentation-overlays") {
    Config::Get().graphics.galaxy_multi_select = true;
    for (unsigned mask = 1; mask < 16; ++mask) {
      const auto before = OverlaySelection::FromFlags(mask & 1, mask & 2, mask & 4, mask & 8);
      for (int32_t clicked = 0; clicked < 4; ++clicked) {
        for (int32_t i = 0; i < 4; ++i)
          Config::Get().graphics.galaxy_overlays[i] = before.Contains(i);
        int originals = 0, native_mode = -1;
        const auto expected = before.Toggle(clicked);
        const auto revision = Galaxy().mode_revision;
        HudNavigationViewController_ChangeGalaxyViewInfo_Hook(
            [&](auto*, int32_t value) { ++originals; native_mode = value; }, nullptr, clicked);
        Require(originals == 1 && native_mode == expected.Representative(clicked)
                    && GalaxySelection().Bits() == expected.Bits() && Galaxy().mode_revision > revision,
                "production HUD click preserves every layer combination, native representative and selection revision");
        std::array<bool, 4> animation{};
        GalaxyToggleHandler_UpdateSelectedAnimation_Hook(
            [&](auto*, bool normal, bool hostile, bool mining, bool hazard) {
              animation = {normal, mining, hostile, hazard};
            }, nullptr, false, false, false, false);
        for (int32_t i = 0; i < 4; ++i)
          Require(animation[i] == expected.Contains(i), "native selected highlight agrees with effective saved flags");
        Require(fleet_native_saved_section == "graphics" && fleet_native_saved_key == "galaxy_overlay_hazards"
                    && fleet_native_saved_value == expected.Contains(3),
                "overlay mutation reaches existing registered save sink for the effective flags");
      }
    }
    Config::Get().graphics.galaxy_multi_select = false;
    int native_mode = -1;
    HudNavigationViewController_ChangeGalaxyViewInfo_Hook(
        [&](auto*, int32_t value) { native_mode = value; }, nullptr, 2);
    Require(native_mode == 2, "disabled multi-selection keeps the native single-mode action");
    Galaxy().installed = false;
    Require(warp_roots.size() == roots, "overlay control workflow creates no retained native owners");
    std::cout << "Production Galaxy HUD/save/highlight workflow covered all 15 nonempty combinations\n";
    return;
  }

  // The moved text can be gone while its native row remains alive. Restore the row before discarding its journal.
  auto binding = std::make_shared<GalaxyStarBinding>();
  auto shift = GalaxyPresentationShift(group, 2.5f);
  binding->shifts.push_back(shift);
  Require(GalaxyRestore(binding) && binding->shifts.empty() && group.spacing == 2.5f,
          "collected moved child does not discard its surviving native row's spacing snapshot");
  shift.reset();

  shift = GalaxyPresentationShift(group, 3.5f);
  binding->shifts.push_back(shift);
  warp_root_failure = true;
  const auto spacing_calls = galaxy_presentation_spacing_calls;
  Require(!GalaxyRestore(binding) && binding->shifts.size() == 1
              && galaxy_presentation_spacing_calls == spacing_calls,
          "failed strong custody retains restoration journal without invoking a borrowed layout receiver");
  warp_root_failure = false;
  galaxy_presentation_spacing_throws = true;
  Require(!GalaxyRestore(binding) && binding->shifts.size() == 1,
          "native restoration failure retains the exact journal for retry");
  galaxy_presentation_spacing_throws = false;
  Require(GalaxyRestore(binding) && group.spacing == 3.5f, "restoration retry completes the retained snapshot");
  shift.reset();

  // A weak owner for a pooled address can expire before the next bind. Its old journal still must be discharged.
  auto old = std::make_shared<GalaxyStarBinding>();
  old->shifts.push_back(GalaxyPresentationShift(group, 4.5f));
  Galaxy().stars[star] = old;
  warp_root_failure = true;
  Require(!GalaxyTrack(star) && Galaxy().stars.at(star) == old && old->shifts.size() == 1,
          "new pooled binding cannot overwrite an expired owner's unfinished journal on custody failure");
  warp_root_failure = false;
  auto replacement = GalaxyTrack(star);
  Require(replacement && replacement != old && old->shifts.empty() && group.spacing == 4.5f,
          "same-address pooled record is replaced only after its surviving child layout is restored");
  Galaxy().stars.clear();
  replacement.reset();

  // If native cleanup rebinds the address, that new record owns the source and cannot be overwritten afterward.
  old->shifts.push_back(GalaxyPresentationShift(group, 5.5f));
  Galaxy().stars[star] = old;
  galaxy_presentation_replacement_key = star;
  galaxy_presentation_replacement = std::make_shared<GalaxyStarBinding>();
  Require(!GalaxyTrack(star) && Galaxy().stars.at(star) == galaxy_presentation_replacement,
          "reentrant cleanup replacement wins over the outer pooled bind");
  galaxy_presentation_replacement_key = nullptr;
  Galaxy().stars.clear();
  galaxy_presentation_replacement.reset();

  std::map<Il2CppObject*, il2cpp::GcHandle> registry;
  registry.emplace(&group.header, il2cpp::GcHandle::Weak(&group.header));
  int refreshes = 0;
  warp_root_failure = true;
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(registry.size() == 1 && refreshes == 0, "failed refresh custody is not evidence that a weak owner expired");
  warp_root_failure = false;
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(refreshes == 1 && registry.size() == 1, "surviving registered source refreshes once custody recovers");
  GalaxyPresentationExpire(&group.header);
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(registry.empty() && refreshes == 1, "confirmed expired registry owner is retired without a callback");

  binding->owner = il2cpp::GcHandle::Weak(&native_star.header);
  binding->node = native_star.context;
  Galaxy().stars[star] = binding;
  warp_root_failure = true;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding, "failed star-refresh custody cannot silently erase a live source");
  warp_root_failure = false;
  visual_throws = true;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding, "unavailable Unity liveness remains unknown rather than destruction");
  visual_throws = false;
  GalaxyPresentationExpire(&native_star.header);
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.empty(), "expired weak star record can complete cleanup without an owner-filtered lookup");
  binding->owner.Reset();

  binding->housing = il2cpp::GcHandle::Strong(&marker.header);
  binding->housing_ready = true;
  galaxy_presentation_destroy_throws = true;
  Require(!GalaxyRemoveHousing(*binding) && binding->housing && !marker.active,
          "failed native marker destruction keeps hidden cleanup custody instead of dropping the owned asset");
  galaxy_presentation_destroy_throws = false;
  Require(GalaxyRemoveHousing(*binding) && !binding->housing && !binding->housing_ready,
          "confirmed native destruction releases the completed housing owner");

  // Settings may disable presentation with inactive stars and no current Galaxy camera.
  binding->shifts.push_back(GalaxyPresentationShift(group, 6.5f));
  binding->housing = il2cpp::GcHandle::Strong(&marker.header);
  binding->housing_ready = true;
  Galaxy().stars[star] = binding;
  Galaxy().last_multi = Galaxy().last_housing = true;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = false;
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = true;
  galaxy_controls::Refresh();
  Require(binding->shifts.size() == 1 && binding->housing
              && Galaxy().pending_layout_cleanup && Galaxy().pending_housing_cleanup
              && !Galaxy().last_multi && !Galaxy().last_housing,
          "failed Settings-disable cleanup retains pending work after the enabled-to-disabled edge is consumed");
  galaxy_presentation_spacing_throws = false;
  galaxy_controls::AfterZoom(nullptr);
  Require(binding->shifts.empty() && binding->housing && group.spacing == 6.5f
              && !Galaxy().pending_layout_cleanup && Galaxy().pending_housing_cleanup,
          "later existing zoom callback retries inactive layout cleanup even without a camera or a new settings edge");
  const auto completed_spacing_calls = galaxy_presentation_spacing_calls;
  galaxy_presentation_destroy_throws = false;
  galaxy_controls::AfterZoom(nullptr);
  Require(binding->shifts.empty() && !binding->housing && group.spacing == 6.5f,
          "later zoom callback retires disabled housing independently of already completed layout cleanup");
  Require(!Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup
              && galaxy_presentation_spacing_calls == completed_spacing_calls,
          "completed cleanup clears both retry latches without repeating the discharged layout restoration");

  // Re-enabling should not leave a delayed disabled-only teardown armed against the enabled presentation.
  binding->shifts.push_back(GalaxyPresentationShift(group, 7.5f));
  binding->housing = il2cpp::GcHandle::Strong(&marker.header);
  binding->housing_ready = true;
  Galaxy().last_multi = Galaxy().last_housing = true;
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = true;
  galaxy_controls::Refresh();
  Require(Galaxy().pending_layout_cleanup && Galaxy().pending_housing_cleanup,
          "new disabled presentation failures are retained for a later existing lifecycle callback");
  const auto failed_spacing_calls = galaxy_presentation_spacing_calls;
  const auto failed_destroy_calls = galaxy_presentation_destroy_calls;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = true;
  galaxy_controls::Refresh();
  Require(!Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup
              && binding->shifts.size() == 1 && binding->housing
              && galaxy_presentation_spacing_calls == failed_spacing_calls
              && galaxy_presentation_destroy_calls == failed_destroy_calls,
          "re-enable cancels disabled-only retries while preserving retained journals/assets for normal native rebinding");
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = false;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = false;
  galaxy_controls::Refresh();
  Require(binding->shifts.empty() && !binding->housing && group.spacing == 7.5f
              && !Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup,
          "a later disable edge still cleans retained inactive presentation after an intervening re-enable");
  Galaxy().stars.clear();
  old.reset();
  binding.reset();
  Galaxy().installed = false;
  Require(warp_roots.size() == roots, "Galaxy presentation fixture releases journals, markers and weak registries");
  std::cout << "Production Galaxy layout, pooled identity, weak custody and marker lifecycle fixtures passed\n";
}
} // namespace
