#pragma once

// Production Galaxy presentation helpers/hooks in the existing controlled runtime.
// Only native layout/marker bodies are substituted. No Unity scene or player files are used.
#include "../src/mod/features/galaxy.cc"

#if __APPLE__
#include <dlfcn.h>
#endif

namespace
{
// The fleet consumer is included later in this controlled-runtime translation unit.
bool InstallBaseLabels();

struct GalaxyPresentationGroup {
  Il2CppObject header;
  float        spacing = 10;
};
struct GalaxyPresentationStar {
  Il2CppObject         header;
  galaxy_binding::Node context;
};
struct GalaxyPresentationMarker {
  Il2CppObject header;
  bool         active = true;
};
int           galaxy_presentation_spacing_calls = 0, galaxy_presentation_destroy_calls = 0;
bool          galaxy_presentation_spacing_throws = false, galaxy_presentation_destroy_throws = false;
Il2CppObject* galaxy_presentation_destroy_failure    = nullptr;
bool          galaxy_presentation_replace_on_destroy = false, galaxy_presentation_enable_starbase_on_destroy = false;
gb::Star*     galaxy_presentation_replacement_key = nullptr;
std::shared_ptr<GalaxyStarBinding> galaxy_presentation_replacement;
Il2CppObject                       galaxy_presentation_world{};
int                                galaxy_presentation_minor_calls  = 0;
bool                               galaxy_presentation_minor_throws = false;
Il2CppObject*                      GalaxyPresentationWorld(const MethodInfo*)
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
void GalaxyPresentationDestroy(Il2CppObject* object, const MethodInfo*)
{
  Require(std::ranges::count_if(warp_roots, [object](const auto& root) { return root.second == object; }) >= 2,
          "each marker destruction retains operation-local custody alongside its owned slot");
  ++galaxy_presentation_destroy_calls;
  if (galaxy_presentation_destroy_throws || galaxy_presentation_destroy_failure == object)
    throw Il2CppExceptionWrapper(&thrown);
  if (galaxy_presentation_replace_on_destroy && galaxy_presentation_replacement_key)
    Galaxy().stars[galaxy_presentation_replacement_key] = galaxy_presentation_replacement;
  if (galaxy_presentation_enable_starbase_on_destroy)
    Config::Get().graphics.galaxy_starbase_systems = true;
}
void GalaxyPresentationExpire(Il2CppObject* object)
{
  // Called only outside native operations, when this object's fixture handles are weak.
  for (auto& [_, target] : warp_roots)
    if (target == object)
      target = nullptr;
}
void* GalaxyPresentationVisibilityMetadata(bool alias_hud, [[maybe_unused]] const char* native_image)
{
  auto& level   = Define("Digit.Prime.Navigation", "ZoomLevels", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  level.is_enum = true;
  auto& handler = Define("Digit.Prime.Navigation", "NavigationZoomEventHandler", IL2CPP_TYPE_CLASS, &component_class);
  // These are metadata-only addresses: neither signature may be invoked or patched. Zig 0.16 leaves the Mac
  // executable's LC_FUNCTION_STARTS empty, so reuse the Apple-linked image from the native detour fixture.
#if __APPLE__
  Require(native_image, "Galaxy visibility metadata requires the Apple-linked native fixture");
  auto* image = dlopen(native_image, RTLD_NOW | RTLD_LOCAL);
  Require(image, "the Apple-linked native fixture must load");
  auto* entry = dlsym(image, "AaiDetourTarget");
  Require(entry && il2cpp::CheckedNativeEntry(entry) == entry,
          "the fixture entry must pass the unchanged production native-fit check");
  // Cached production entries retain this address until process exit; keep the image loaded for that lifetime.
#else
  auto* entry = reinterpret_cast<void*>(&GalaxyPresentationSpacing);
#endif
  AddMethod(handler, "OnZoomChanged", entry, T(void_class), {T(level)});
  if (alias_hud) {
    auto& hud = Define("Digit.Prime.Navigation.UI", "HudNavigationViewController", IL2CPP_TYPE_CLASS, &component_class);
    AddMethod(hud, "ChangeGalaxyViewInfo", entry, T(void_class), {T(int_class)});
  }
  return entry;
}
std::shared_ptr<GalaxyShift> GalaxyPresentationShift(GalaxyPresentationGroup& group, float before)
{
  auto shift     = std::make_shared<GalaxyShift>();
  shift->group   = il2cpp::GcHandle::Weak(&group.header);
  shift->spacing = before;
  return shift; // A collected moved child, with a surviving row needing restoration.
}

void GalaxyPresentationCases(std::string_view mode, const char* native_image = nullptr)
{
  auto& group_class = Define("UnityEngine.UI", "HorizontalLayoutGroup", IL2CPP_TYPE_CLASS, &component_class);
  AddMethod(group_class, "set_spacing", reinterpret_cast<void*>(&GalaxyPresentationSpacing), T(void_class),
            {T(float_class)});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&GalaxyPresentationActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&GalaxyPresentationSetActive), T(void_class),
            {T(bool_class)});
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&GalaxyPresentationDestroy), T(void_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto& node_class =
      Define("Digit.PrimeServer.Models", "GalaxyNode", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(galaxy_binding::Node));
  auto& galaxy_class = Define("Digit.PrimeServer.Models", "OptimisedGalaxy", IL2CPP_TYPE_CLASS, &object_class);
  AddField(node_class, "_galaxy", galaxy_class, 0);
  AddField(node_class, "_index", int_class, offsetof(galaxy_binding::Node, index));
  auto& world_class = Define("Digit.Prime.Navigation", "GameWorldManager", IL2CPP_TYPE_CLASS, &object_class);
  galaxy_presentation_world.klass = &world_class.klass;
  const auto& world_instance = AddMethod(world_class, "get_Instance", reinterpret_cast<void*>(&GalaxyPresentationWorld),
                                         T(world_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(world_class, "Instance", &world_instance);
  AddMethod(world_class, "IsMinorNode", reinterpret_cast<void*>(&GalaxyPresentationMinor), T(bool_class),
            {Ref(node_class)});
  auto& star_class =
      Define("Digit.Prime.Navigation", "NavigationStarEntityWidget", IL2CPP_TYPE_CLASS, &component_class);
  AddField(star_class, "m_context", node_class, offsetof(GalaxyPresentationStar, context));
  Il2CppObject             galaxy{&galaxy_class.klass};
  GalaxyPresentationStar   native_star{{&star_class.klass}, {reinterpret_cast<gb::OptimisedGalaxy*>(&galaxy), 2}};
  auto*                    star = reinterpret_cast<gb::Star*>(&native_star);
  GalaxyPresentationGroup  group{{&group_class.klass}};
  GalaxyPresentationMarker marker{{&game_object_class.klass}};
  GalaxyPresentationMarker starbase_marker{{&game_object_class.klass}};
  GalaxyPresentationMarker replacement_housing{{&game_object_class.klass}};
  GalaxyPresentationMarker replacement_starbase{{&game_object_class.klass}};
  Il2CppObject             housing_sprite{&unity_object_class.klass};
  Il2CppObject             starbase_sprite{&unity_object_class.klass};
  const auto               roots = warp_roots.size();
  Galaxy().installed             = true;
  Galaxy().thread                = std::this_thread::get_id();

  if (mode == "galaxy-presentation-alias") {
    std::array<void*, 8> entries{};
    for (size_t i = 0; i < entries.size(); ++i)
      entries[i] = reinterpret_cast<void*>(0x1000 + 0x100 * i);
    Require(GalaxyTargetsDistinct(entries), "distinct Galaxy callback entries pass their feature-specific preflight");
    for (size_t i = 1; i < entries.size(); ++i) {
      const auto saved = entries[i];
      entries[i]       = entries[0];
      Require(!GalaxyTargetsDistinct(entries), "aliased receiver/argument callback entries decline before staging");
      entries[i] = saved;
    }
    entries[0] = entries[1] = nullptr;
    Require(GalaxyTargetsDistinct(entries),
            "missing targets remain the transaction's missing-target decision, not aliasing");
    auto* target = GalaxyPresentationVisibilityMetadata(true, native_image);
    Require(NavigationVisibilityEntry() == target && GalaxyHookEntries()[4] == target
                && GalaxyHookEntries()[6] == target && !galaxy_controls::VisibilityHookCompatible(),
            "typed metadata exposes the incompatible HUD/shared-visibility alias before either feature installs");
    std::array<unsigned char, 16> original_code{};
    std::memcpy(original_code.data(), target, original_code.size());
    Require(!InstallBaseLabels() && !InstallGalaxyLabels(),
            "base-first startup rejects a shared visibility alias before installing any wrong-contract detour");
    Require(std::memcmp(original_code.data(), target, original_code.size()) == 0,
            "base-first rejection leaves the original shared native instructions untouched");
    Require(!InstallGalaxyLabels() && !InstallBaseLabels() && !EnsureNavigationVisibilityUpdates(),
            "Galaxy-first startup and repeated shared-owner requests retain the same safe refusal");
    Require(std::memcmp(original_code.data(), target, original_code.size()) == 0,
            "Galaxy-first and repeated refusal never patch the incompatible native entry");
    Galaxy().installed = false;
    Require(warp_roots.size() == roots, "alias preflight performs no native mutations or ownership acquisition");
    std::cout << "Galaxy callback-family entry alias preflight passed; full native installer remains separate\n";
    return;
  }

  if (mode == "galaxy-presentation-profiles") {
    GalaxyPresentationVisibilityMetadata(false, native_image);
    Require(galaxy_controls::VisibilityHookCompatible(),
            "missing unrelated Galaxy targets do not block an independent compatible base visibility entry");
    auto& graphics              = Config::Get().graphics;
    Galaxy().zoom_known         = true;
    Galaxy().normalized         = .25f;
    Galaxy().level              = ZoomLevels::Middle;
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
    warp_root_failure                = true;
    Require(GalaxyEffective(star, ZoomLevels::Far) == ZoomLevels::Far,
            "classification custody failure does not authorize a forced profile");
    warp_root_failure           = false;
    graphics.galaxy_label_major = graphics.galaxy_label_minor = {ZoomMode::Threshold, .5f};
    const auto calls                                          = galaxy_presentation_minor_calls;
    Galaxy().normalized                                       = .5f;
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
        int        originals = 0, native_mode = -1;
        const auto expected = before.Toggle(clicked);
        const auto revision = Galaxy().mode_revision;
        HudNavigationViewController_ChangeGalaxyViewInfo_Hook(
            [&](auto*, int32_t value) {
              ++originals;
              native_mode = value;
            },
            nullptr, clicked);
        Require(originals == 1 && native_mode == expected.Representative(clicked)
                    && GalaxySelection().Bits() == expected.Bits() && Galaxy().mode_revision > revision,
                "production HUD click preserves every layer combination, native representative and selection revision");
        std::array<bool, 4> animation{};
        GalaxyToggleHandler_UpdateSelectedAnimation_Hook(
            [&](auto*, bool normal, bool hostile, bool mining, bool hazard) {
              animation = {normal, mining, hostile, hazard};
            },
            nullptr, false, false, false, false);
        for (int32_t i = 0; i < 4; ++i)
          Require(animation[i] == expected.Contains(i), "native selected highlight agrees with effective saved flags");
        Require(fleet_native_saved_section == "graphics" && fleet_native_saved_key == "galaxy_overlay_hazards"
                    && fleet_native_saved_value == expected.Contains(3),
                "overlay mutation reaches existing registered save sink for the effective flags");
      }
    }
    Config::Get().graphics.galaxy_multi_select = false;
    int native_mode                            = -1;
    HudNavigationViewController_ChangeGalaxyViewInfo_Hook([&](auto*, int32_t value) { native_mode = value; }, nullptr,
                                                          2);
    Require(native_mode == 2, "disabled multi-selection keeps the native single-mode action");
    Galaxy().installed = false;
    Require(warp_roots.size() == roots, "overlay control workflow creates no retained native owners");
    std::cout << "Production Galaxy HUD/save/highlight workflow covered all 15 nonempty combinations\n";
    return;
  }

  const auto embedded_starbase = StarbaseData::Parse(g_embeddedStarbaseSystems, g_embeddedStarbaseSystems_SIZE);
  const auto embedded_housing  = HousingData::Parse(g_embeddedStationHousing, g_embeddedStationHousing_SIZE);
  Require(
      embedded_starbase && embedded_starbase->collisional.size() == 42 && embedded_starbase->magnetic.size() == 35
          && embedded_starbase->active.size() == 30
          && embedded_starbase->Find(1263278966) == std::optional(PlasmaKindMask::Collisional)
          && !embedded_starbase->Find(54),
      "actual embedded community data retains its 42/35/30 positives, Jishui Collisional mapping and unknown omission");
  std::vector<int64_t> embedded_ids = embedded_starbase->collisional;
  embedded_ids.insert(embedded_ids.end(), embedded_starbase->magnetic.begin(), embedded_starbase->magnetic.end());
  embedded_ids.insert(embedded_ids.end(), embedded_starbase->active.begin(), embedded_starbase->active.end());
  std::ranges::sort(embedded_ids);
  embedded_ids.erase(std::unique(embedded_ids.begin(), embedded_ids.end()), embedded_ids.end());
  Require(
      embedded_ids.size() == 107 && embedded_housing && !embedded_housing->housing.empty()
          && embedded_housing->Contains(embedded_housing->housing.front()) == std::optional(true),
      "checked-in asset embedding supplies 107 unique starbase positives while the frozen housing asset stays usable");

  // Native tier and exact membership are independent for the two datasets.
  auto& graphics                  = Config::Get().graphics;
  Galaxy().housing                = HousingData{{11, 33}, {11, 22, 33, 44}};
  Galaxy().starbase               = StarbaseData{{22, 33}, {}, {}};
  graphics.galaxy_station_housing = graphics.galaxy_starbase_systems = true;
  Require(
      GalaxyMarkerEligibility(11, ZoomLevels::Near) == std::array{true, false}
          && GalaxyMarkerEligibility(22, ZoomLevels::Near) == std::array{false, true}
          && GalaxyMarkerEligibility(33, ZoomLevels::Near) == std::array{true, true}
          && GalaxyMarkerEligibility(44, ZoomLevels::Near) == std::array{false, false}
          && GalaxyMarkerEligibility(33, ZoomLevels::Far) == std::array{false, false},
      "housing-only, starbase-only, overlap and unlisted systems use independent positives at the native Near tier");
  Require(HousingAvailable() && StarbaseAvailable(), "both independently loaded marker datasets are available");
  Galaxy().housing.reset();
  Require(!HousingAvailable() && StarbaseAvailable()
              && GalaxyMarkerEligibility(22, ZoomLevels::Near) == std::array{false, true},
          "unavailable housing data does not disable community starbase positives");
  Galaxy().housing = HousingData{{11, 33}, {11, 22, 33, 44}};
  Galaxy().starbase.reset();
  Require(HousingAvailable() && !StarbaseAvailable()
              && GalaxyMarkerEligibility(11, ZoomLevels::Near) == std::array{true, false},
          "unavailable starbase data leaves cyan housing available");
  Galaxy().starbase = StarbaseData{{22, 33}, {}, {}};
  const auto cyan = kGalaxyMarkerStyles[0].color, yellow = kGalaxyMarkerStyles[1].color;
  Require(static_cast<size_t>(GalaxyMarkerKind::Housing) == 0 && static_cast<size_t>(GalaxyMarkerKind::Starbase) == 1
              && cyan.r == .45f && cyan.g == .9f && cyan.b == 1.f && cyan.a == 1.f && yellow.r == 1.f
              && yellow.g == .85f && yellow.b == .2f && yellow.a == 1.f,
          "bounded insertion order is cyan housing then yellow starbase, with the existing cyan tint preserved");
  graphics.galaxy_station_housing = graphics.galaxy_starbase_systems = false;

  // The moved text can be gone while its native row remains alive. Restore the row before discarding its journal.
  auto binding = std::make_shared<GalaxyStarBinding>();
  auto shift   = GalaxyPresentationShift(group, 2.5f);
  binding->shifts.push_back(shift);
  Require(GalaxyRestore(binding) && binding->shifts.empty() && group.spacing == 2.5f,
          "collected moved child does not discard its surviving native row's spacing snapshot");
  shift.reset();

  shift = GalaxyPresentationShift(group, 3.5f);
  binding->shifts.push_back(shift);
  warp_root_failure        = true;
  const auto spacing_calls = galaxy_presentation_spacing_calls;
  Require(!GalaxyRestore(binding) && binding->shifts.size() == 1 && galaxy_presentation_spacing_calls == spacing_calls,
          "failed strong custody retains restoration journal without invoking a borrowed layout receiver");
  warp_root_failure                  = false;
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
  warp_root_failure    = true;
  Require(!GalaxyTrack(star) && Galaxy().stars.at(star) == old && old->shifts.size() == 1,
          "new pooled binding cannot overwrite an expired owner's unfinished journal on custody failure");
  warp_root_failure = false;
  auto replacement  = GalaxyTrack(star);
  Require(replacement && replacement != old && old->shifts.empty() && group.spacing == 4.5f,
          "same-address pooled record is replaced only after its surviving child layout is restored");
  Galaxy().stars.clear();
  replacement.reset();

  // If native cleanup rebinds the address, that new record owns the source and cannot be overwritten afterward.
  old->shifts.push_back(GalaxyPresentationShift(group, 5.5f));
  Galaxy().stars[star]                = old;
  galaxy_presentation_replacement_key = star;
  galaxy_presentation_replacement     = std::make_shared<GalaxyStarBinding>();
  Require(!GalaxyTrack(star) && Galaxy().stars.at(star) == galaxy_presentation_replacement,
          "reentrant cleanup replacement wins over the outer pooled bind");
  galaxy_presentation_replacement_key = nullptr;
  Galaxy().stars.clear();
  galaxy_presentation_replacement.reset();

  std::map<Il2CppObject*, il2cpp::GcHandle> registry;
  registry.emplace(&group.header, il2cpp::GcHandle::Weak(&group.header));
  int refreshes     = 0;
  warp_root_failure = true;
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(registry.size() == 1 && refreshes == 0, "failed refresh custody is not evidence that a weak owner expired");
  warp_root_failure = false;
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(refreshes == 1 && registry.size() == 1, "surviving registered source refreshes once custody recovers");
  GalaxyPresentationExpire(&group.header);
  GalaxyRefreshRegistered(registry, [&](auto*) { ++refreshes; });
  Require(registry.empty() && refreshes == 1, "confirmed expired registry owner is retired without a callback");

  binding->owner       = il2cpp::GcHandle::Weak(&native_star.header);
  binding->node        = native_star.context;
  Galaxy().stars[star] = binding;
  warp_root_failure    = true;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding, "failed star-refresh custody cannot silently erase a live source");
  warp_root_failure = false;
  visual_throws     = true;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding, "unavailable Unity liveness remains unknown rather than destruction");
  visual_throws = false;
  GalaxyPresentationExpire(&native_star.header);
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.empty(), "expired weak star record can complete cleanup without an owner-filtered lookup");
  binding->owner.Reset();

  binding->markers[0].object         = il2cpp::GcHandle::Strong(&marker.header);
  binding->markers[0].sprite         = il2cpp::GcHandle::Strong(&housing_sprite);
  binding->markers[0].ready          = true;
  galaxy_presentation_destroy_throws = true;
  Require(!GalaxyRemoveMarker(binding->markers[0]) && binding->markers[0].object
              && binding->markers[0].sprite.Get() == &housing_sprite && !marker.active,
          "failed native marker destruction keeps hidden object and sprite custody instead of dropping the asset");
  galaxy_presentation_destroy_throws = false;
  Require(GalaxyRemoveMarker(binding->markers[0]) && !binding->markers[0].object && !binding->markers[0].sprite
              && !binding->markers[0].ready,
          "confirmed native destruction releases the completed marker and its assigned sprite together");

  binding->markers[1].sprite = il2cpp::GcHandle::Strong(&starbase_sprite);
  binding->markers[1].ready  = true;
  Require(GalaxyRemoveMarker(binding->markers[1]) && !binding->markers[1].object && !binding->markers[1].sprite
              && !binding->markers[1].ready,
          "a missing marker object discharges its remaining sprite custody and readiness");
  binding->markers[1].object           = il2cpp::GcHandle::Strong(&starbase_marker.header);
  binding->markers[1].sprite           = il2cpp::GcHandle::Strong(&starbase_sprite);
  binding->markers[1].ready            = true;
  visual_dead_object                   = &starbase_marker.header;
  const auto dead_marker_destroy_calls = galaxy_presentation_destroy_calls;
  Require(GalaxyRemoveMarker(binding->markers[1]) && !binding->markers[1].object && !binding->markers[1].sprite
              && !binding->markers[1].ready && galaxy_presentation_destroy_calls == dead_marker_destroy_calls,
          "confirmed native marker destruction releases its sprite without scheduling another Destroy");
  visual_dead_object = nullptr;

  // Settings may disable presentation with inactive stars and no current Galaxy camera.
  binding->shifts.push_back(GalaxyPresentationShift(group, 6.5f));
  binding->markers[0].object = il2cpp::GcHandle::Strong(&marker.header);
  binding->markers[0].ready  = true;
  Galaxy().stars[star]       = binding;
  Galaxy().last_multi = Galaxy().last_housing = true;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = false;
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = true;
  galaxy_controls::Refresh();
  Require(binding->shifts.size() == 1 && binding->markers[0].object && Galaxy().pending_layout_cleanup
              && Galaxy().pending_housing_cleanup && !Galaxy().last_multi && !Galaxy().last_housing,
          "failed Settings-disable cleanup retains pending work after the enabled-to-disabled edge is consumed");
  galaxy_presentation_spacing_throws = false;
  galaxy_controls::AfterZoom(nullptr);
  Require(binding->shifts.empty() && binding->markers[0].object && group.spacing == 6.5f
              && !Galaxy().pending_layout_cleanup && Galaxy().pending_housing_cleanup,
          "later existing zoom callback retries inactive layout cleanup even without a camera or a new settings edge");
  const auto completed_spacing_calls = galaxy_presentation_spacing_calls;
  galaxy_presentation_destroy_throws = false;
  galaxy_controls::AfterZoom(nullptr);
  Require(binding->shifts.empty() && !binding->markers[0].object && group.spacing == 6.5f,
          "later zoom callback retires disabled housing independently of already completed layout cleanup");
  Require(!Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup
              && galaxy_presentation_spacing_calls == completed_spacing_calls,
          "completed cleanup clears both retry latches without repeating the discharged layout restoration");

  // Re-enabling should not leave a delayed disabled-only teardown armed against the enabled presentation.
  binding->shifts.push_back(GalaxyPresentationShift(group, 7.5f));
  binding->markers[0].object = il2cpp::GcHandle::Strong(&marker.header);
  binding->markers[0].ready  = true;
  Galaxy().last_multi = Galaxy().last_housing = true;
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = true;
  galaxy_controls::Refresh();
  Require(Galaxy().pending_layout_cleanup && Galaxy().pending_housing_cleanup,
          "new disabled presentation failures are retained for a later existing lifecycle callback");
  const auto failed_spacing_calls            = galaxy_presentation_spacing_calls;
  const auto failed_destroy_calls            = galaxy_presentation_destroy_calls;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = true;
  galaxy_controls::Refresh();
  Require(
      !Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup && binding->shifts.size() == 1
          && binding->markers[0].object && galaxy_presentation_spacing_calls == failed_spacing_calls
          && galaxy_presentation_destroy_calls == failed_destroy_calls,
      "re-enable cancels disabled-only retries while preserving retained journals/assets for normal native rebinding");
  galaxy_presentation_spacing_throws = galaxy_presentation_destroy_throws = false;
  Config::Get().graphics.galaxy_multi_select = Config::Get().graphics.galaxy_station_housing = false;
  galaxy_controls::Refresh();
  Require(binding->shifts.empty() && !binding->markers[0].object && group.spacing == 7.5f
              && !Galaxy().pending_layout_cleanup && !Galaxy().pending_housing_cleanup,
          "a later disable edge still cleans retained inactive presentation after an intervening re-enable");

  const auto own_markers = [&](const std::shared_ptr<GalaxyStarBinding>& record, bool ready = true) {
    record->markers[0].object = il2cpp::GcHandle::Strong(&marker.header);
    record->markers[1].object = il2cpp::GcHandle::Strong(&starbase_marker.header);
    record->markers[0].sprite = il2cpp::GcHandle::Strong(&housing_sprite);
    record->markers[1].sprite = il2cpp::GcHandle::Strong(&starbase_sprite);
    record->markers[0].ready = record->markers[1].ready = ready;
    marker.active = starbase_marker.active = true;
  };
  own_markers(binding, false);
  galaxy_presentation_destroy_failure = &marker.header;
  const auto partial_destroy_calls    = galaxy_presentation_destroy_calls;
  Require(!GalaxyRemoveMarkers(*binding) && binding->markers[0].object && !binding->markers[0].ready
              && binding->markers[0].sprite.Get() == &housing_sprite && !binding->markers[1].object
              && !binding->markers[1].sprite && !binding->markers[1].ready && !marker.active && !starbase_marker.active
              && galaxy_presentation_destroy_calls == partial_destroy_calls + 2,
          "failed cleanup of a partial cyan slot still independently retires the partial yellow slot");
  galaxy_presentation_destroy_failure = nullptr;
  Require(GalaxyRemoveMarkers(*binding) && GalaxyMarkersEmpty(*binding) && !binding->markers[0].sprite
              && !binding->markers[1].sprite,
          "partial marker cleanup retries retained object and sprite custody without leaking the other slot");

  own_markers(binding);
  warp_root_failure                = true;
  const auto custody_destroy_calls = galaxy_presentation_destroy_calls;
  Require(!GalaxyRemoveMarkers(*binding) && binding->markers[0].object && binding->markers[1].object
              && binding->markers[0].sprite.Get() == &housing_sprite
              && binding->markers[1].sprite.Get() == &starbase_sprite
              && galaxy_presentation_destroy_calls == custody_destroy_calls,
          "failed operation-local custody keeps both marker objects and sprites without invoking borrowed receivers");
  warp_root_failure = false;
  visual_throws     = true;
  Require(!GalaxyRemoveMarkers(*binding) && binding->markers[0].object && binding->markers[1].object
              && binding->markers[0].sprite.Get() == &housing_sprite
              && binding->markers[1].sprite.Get() == &starbase_sprite,
          "unknown Unity liveness retains both marker objects and sprites rather than treating either as destroyed");
  visual_throws = false;
  GalaxyHideMarkers(*binding);
  Require(!marker.active && !starbase_marker.active && GalaxyRemoveMarkers(*binding) && !binding->markers[0].sprite
              && !binding->markers[1].sprite,
          "the shared hide/removal path clears both sprite owners after native liveness recovers");

  // Each live setting transition owns only its own disabled slot, including inactive pooled widgets.
  own_markers(binding);
  Galaxy().last_housing = Galaxy().last_starbase = true;
  graphics.galaxy_station_housing                = false;
  graphics.galaxy_starbase_systems               = true;
  galaxy_controls::Refresh();
  Require(!binding->markers[0].object && binding->markers[1].object && starbase_marker.active
              && !Galaxy().pending_housing_cleanup && !Galaxy().pending_starbase_cleanup,
          "disabling cyan housing preserves the enabled yellow slot and its visibility");
  binding->markers[0].object = il2cpp::GcHandle::Strong(&marker.header);
  binding->markers[0].ready  = true;
  marker.active              = true;
  Galaxy().last_housing = Galaxy().last_starbase = true;
  graphics.galaxy_station_housing                = true;
  graphics.galaxy_starbase_systems               = false;
  galaxy_presentation_destroy_failure            = &starbase_marker.header;
  galaxy_controls::Refresh();
  Require(binding->markers[0].object && marker.active && binding->markers[1].object && !starbase_marker.active
              && !Galaxy().pending_housing_cleanup && Galaxy().pending_starbase_cleanup,
          "failed yellow-disable cleanup stays pending without tearing down enabled cyan housing");
  const auto yellow_retry_calls    = galaxy_presentation_destroy_calls;
  graphics.galaxy_starbase_systems = true;
  galaxy_controls::Refresh();
  Require(!Galaxy().pending_starbase_cleanup && binding->markers[1].object
              && galaxy_presentation_destroy_calls == yellow_retry_calls,
          "re-enabling yellow cancels its disabled-only retry while retaining its owned marker");
  galaxy_presentation_destroy_failure = nullptr;
  graphics.galaxy_station_housing = graphics.galaxy_starbase_systems = false;
  galaxy_controls::Refresh();
  Require(GalaxyMarkersEmpty(*binding) && !Galaxy().pending_housing_cleanup && !Galaxy().pending_starbase_cleanup,
          "a later disable edge cleans both retained markers independently");

  own_markers(binding);
  Galaxy().last_housing = Galaxy().last_starbase = true;
  galaxy_presentation_destroy_failure            = &marker.header;
  galaxy_controls::Refresh();
  Require(binding->markers[0].object && !binding->markers[1].object && Galaxy().pending_housing_cleanup
              && !Galaxy().pending_starbase_cleanup,
          "simultaneous disable attempts yellow destruction even when cyan destruction fails");
  galaxy_presentation_destroy_failure = nullptr;
  galaxy_controls::AfterZoom(nullptr);
  Require(GalaxyMarkersEmpty(*binding) && !Galaxy().pending_housing_cleanup && !Galaxy().pending_starbase_cleanup,
          "the existing zoom owner retries inactive failed marker cleanup without another settings edge");

  own_markers(binding);
  Galaxy().last_housing = Galaxy().last_starbase = true;
  galaxy_presentation_enable_starbase_on_destroy = true;
  galaxy_controls::Refresh();
  galaxy_presentation_enable_starbase_on_destroy = false;
  Require(!binding->markers[0].object && binding->markers[1].object && starbase_marker.active
              && !Galaxy().pending_starbase_cleanup,
          "a reentrant yellow re-enable during cyan cleanup prevents the stale disable snapshot destroying yellow");
  graphics.galaxy_starbase_systems = false;
  galaxy_controls::Refresh();
  Require(GalaxyMarkersEmpty(*binding), "the next explicit disable retires the reentrantly re-enabled yellow owner");

  // Release and pooled replacement preserve both independent cleanup journals.
  binding->owner = il2cpp::GcHandle::Weak(&native_star.header);
  binding->node  = native_star.context;
  own_markers(binding);
  galaxy_presentation_destroy_failure = &marker.header;
  int releases                        = 0;
  NavigationStarEntityWidget_OnAboutToReleaseContext_Hook([&](auto*) { ++releases; }, star);
  Require(
      releases == 1 && Galaxy().stars.at(star) == binding && binding->markers[0].object && !binding->markers[1].object
          && !GalaxyTrack(star),
      "release retains failed cyan custody, cleans yellow and prevents same-address overwrite until cleanup succeeds");
  galaxy_presentation_destroy_failure = nullptr;
  replacement                         = GalaxyTrack(star);
  Require(replacement && replacement != binding && GalaxyMarkersEmpty(*binding),
          "pooled rebinding replaces a released record only after both old marker slots are discharged");

  own_markers(replacement);
  galaxy_presentation_replacement                    = std::make_shared<GalaxyStarBinding>();
  galaxy_presentation_replacement->owner             = il2cpp::GcHandle::Weak(&native_star.header);
  galaxy_presentation_replacement->node              = native_star.context;
  galaxy_presentation_replacement->markers[0].object = il2cpp::GcHandle::Strong(&replacement_housing.header);
  galaxy_presentation_replacement->markers[1].object = il2cpp::GcHandle::Strong(&replacement_starbase.header);
  galaxy_presentation_replacement_key                = star;
  galaxy_presentation_replace_on_destroy             = true;
  NavigationStarEntityWidget_OnAboutToReleaseContext_Hook([&](auto*) { ++releases; }, star);
  Require(Galaxy().stars.at(star) == galaxy_presentation_replacement && GalaxyMarkersEmpty(*replacement)
              && galaxy_presentation_replacement->markers[0].object
              && galaxy_presentation_replacement->markers[1].object,
          "reentrant release replacement owns both new slots; outer cleanup touches only its retained old record");
  galaxy_presentation_replace_on_destroy = false;
  galaxy_presentation_replacement_key    = nullptr;
  NavigationStarEntityWidget_OnAboutToReleaseContext_Hook([&](auto*) { ++releases; }, star);
  Require(Galaxy().stars.empty() && GalaxyMarkersEmpty(*galaxy_presentation_replacement),
          "ordinary release retires both replacement markers and removes their completed record");
  galaxy_presentation_replacement.reset();
  replacement.reset();

  binding->releasing = false;
  binding->owner     = il2cpp::GcHandle::Weak(&native_star.header);
  own_markers(binding);
  Galaxy().stars[star] = binding;
  visual_throws        = true;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding && binding->markers[0].object && binding->markers[1].object,
          "unknown star liveness cannot retire either slot");
  visual_throws = false;
  GalaxyPresentationExpire(&native_star.header);
  galaxy_presentation_destroy_failure = &marker.header;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.at(star) == binding && binding->markers[0].object && !binding->markers[1].object,
          "expired-owner cleanup still attempts both slots and retains the failed slot before retiring its record");
  galaxy_presentation_destroy_failure = nullptr;
  GalaxyRefreshStars(ZoomLevels::Near);
  Require(Galaxy().stars.empty() && GalaxyMarkersEmpty(*binding),
          "expired-owner retry retires the record after both independent marker journals complete");
  Galaxy().housing.reset();
  Galaxy().starbase.reset();
  Galaxy().stars.clear();
  old.reset();
  binding.reset();
  Galaxy().installed = false;
  Require(warp_roots.size() == roots, "Galaxy presentation fixture releases journals, markers and weak registries");
  std::cout << "Production Galaxy layout, pooled identity, weak custody and marker lifecycle fixtures passed\n";
}
} // namespace
