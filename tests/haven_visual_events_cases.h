#pragma once

// Included after haven_native_cases.h: its actual haven.cc consumer is shared.
// Only managed metadata, scene objects and native method bodies are controlled.
// No installed detours, real streaming, rendered-depth or player claim.
namespace
{
struct HavenEventsArray {
  Il2CppObject        header{};
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 0;
  Il2CppObject*       values[2]{};
};
struct HavenEventsNode {
  Il2CppObject      header{};
  int32_t           id = 0;
  bool              alive = true, active = true, enabled = true;
  std::string       name;
  HavenEventsNode *transform = nullptr, *parent = nullptr, *shader = nullptr;
  HavenEventsArray *components = nullptr, *materials = nullptr;
  std::array<float, 2> depth{0.25f, 0.5f};
};
std::deque<HavenEventsNode> haven_events_nodes;
HavenEventsNode *haven_events_found = nullptr, *haven_events_manager = nullptr;
bool haven_events_ready = false, haven_events_ready_failure = false;
bool haven_events_components_failure = false;
bool haven_events_check_post_original = false, haven_events_native_finished = false;
int haven_events_finds = 0, haven_events_scans = 0, haven_events_readiness = 0;
int haven_events_depth_reads = 0, haven_events_depth_writes = 0, haven_events_water_writes = 0;
int haven_events_alive_reads = 0, haven_events_active_reads = 0;
int haven_events_visual_reads = 0;
std::function<void()> haven_events_on_scan;
std::function<void()> haven_events_on_parent;
std::function<void(HavenEventsNode*, size_t, float)> haven_events_on_write;

HavenEventsNode* HavenEventsNative(Il2CppObject* object)
{ return reinterpret_cast<HavenEventsNode*>(object); }
HavenEventsNode* HavenEventsNew(FakeClass& type, std::string name = {})
{
  auto& node        = haven_events_nodes.emplace_back();
  node.header.klass = &type.klass;
  node.id           = static_cast<int32_t>(haven_events_nodes.size());
  node.name         = std::move(name);
  return &node;
}
bool HavenEventsAlive(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_alive_reads;
  return object && HavenEventsNative(object)->alive;
}
int32_t HavenEventsId(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_visual_reads;
  return HavenEventsNative(object)->id;
}
Il2CppString* HavenEventsName(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_visual_reads;
  return il2cpp::NewString(HavenEventsNative(object)->name);
}
bool HavenEventsActive(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_active_reads;
  return HavenEventsNative(object)->active;
}
Il2CppObject* HavenEventsFind(Il2CppString* path, const MethodInfo*)
{
  ++haven_events_finds;
  Require(il2cpp::ToUtf8(path) == "StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder",
          "Haven discovery uses the production exact owner path");
  return haven_events_found ? &haven_events_found->header : nullptr;
}
Il2CppObject* HavenEventsComponents(Il2CppObject* object, Il2CppReflectionType* type, bool inactive,
                                    const MethodInfo*)
{
  ++haven_events_scans;
  Require(type && inactive, "Haven scans actual renderer metadata including inactive children");
  if (haven_events_components_failure)
    throw Il2CppExceptionWrapper(&thrown);
  if (auto callback = std::exchange(haven_events_on_scan, {}))
    callback();
  return reinterpret_cast<Il2CppObject*>(HavenEventsNative(object)->components);
}
Il2CppObject* HavenEventsMaterials(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_visual_reads;
  return reinterpret_cast<Il2CppObject*>(HavenEventsNative(object)->materials);
}
Il2CppObject* HavenEventsShader(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_visual_reads;
  return &HavenEventsNative(object)->shader->header;
}
bool HavenEventsEnabled(Il2CppObject* object, const MethodInfo*)
{
  ++haven_events_visual_reads;
  return HavenEventsNative(object)->enabled;
}
void HavenEventsSetEnabled(Il2CppObject* object, bool value, const MethodInfo*)
{
  ++haven_events_water_writes;
  HavenEventsNative(object)->enabled = value;
}
size_t HavenEventsProperty(Il2CppString* name)
{
  const auto text = il2cpp::ToUtf8(name);
  Require(text == "_OffsetFactor" || text == "_OffsetUnits", "Haven writes only its two native depth properties");
  return text == "_OffsetFactor" ? 0 : 1;
}
bool HavenEventsHasProperty(Il2CppObject*, Il2CppString* name, const MethodInfo*)
{
  ++haven_events_visual_reads;
  (void)HavenEventsProperty(name);
  return true;
}
float HavenEventsGetDepth(Il2CppObject* object, Il2CppString* name, const MethodInfo*)
{
  ++haven_events_depth_reads;
  return HavenEventsNative(object)->depth[HavenEventsProperty(name)];
}
void HavenEventsSetDepth(Il2CppObject* object, Il2CppString* name, float value, const MethodInfo*)
{
  auto* node      = HavenEventsNative(object);
  const auto key = HavenEventsProperty(name);
  ++haven_events_depth_writes;
  node->depth[key] = value;
  // Copy the callback so a one-shot can safely clear itself during invocation.
  if (auto callback = haven_events_on_write)
    callback(node, key, value);
}
Il2CppObject* HavenEventsTransform(Il2CppObject* object, const MethodInfo*)
{
  Require(!haven_events_check_post_original || haven_events_native_finished,
          "material owner qualification runs after the native replacement body");
  auto* transform = HavenEventsNative(object)->transform;
  return transform ? &transform->header : nullptr;
}
Il2CppObject* HavenEventsParent(Il2CppObject* object, const MethodInfo*)
{
  auto* parent = HavenEventsNative(object)->parent;
  if (auto callback = std::exchange(haven_events_on_parent, {}))
    callback();
  return parent ? &parent->header : nullptr;
}
Il2CppObject* HavenEventsManager(const MethodInfo*)
{ return &haven_events_manager->header; }
bool HavenEventsReady(Il2CppObject*, const MethodInfo*)
{
  ++haven_events_readiness;
  if (haven_events_ready_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return haven_events_ready;
}

template <class F>
void HavenEventsMethod(FakeClass& owner, const char* name, F body, const Il2CppType* result,
                       std::vector<const Il2CppType*> parameters, bool is_static = false)
{
  for (const auto* existing : owner.methods) {
    if (std::string_view(existing->name) != name || existing->parameters_count != parameters.size())
      continue;
    auto* method                 = const_cast<MethodInfo*>(existing);
    method->methodPointer        = reinterpret_cast<Il2CppMethodPointer>(body);
    method->virtualMethodPointer = method->methodPointer;
    return;
  }
  AddMethod(owner, name, reinterpret_cast<void*>(body), result, std::move(parameters),
            METHOD_ATTRIBUTE_PUBLIC | (is_static ? METHOD_ATTRIBUTE_STATIC : 0));
}

struct HavenEventsScene {
  HavenEventsNode *owner, *holder, *road_renderer, *water_renderer, *road, *water;
  HavenEventsArray components{}, road_materials{}, water_materials{};

  HavenEventsScene(FakeClass& transform, HavenEventsNode* road_shader, HavenEventsNode* water_shader)
  {
    owner                 = HavenEventsNew(game_object_class);
    owner->transform      = HavenEventsNew(transform);
    holder                = HavenEventsNew(game_object_class);
    holder->transform     = HavenEventsNew(transform);
    holder->transform->parent = owner->transform;
    road_renderer         = HavenEventsNew(renderer_class);
    water_renderer        = HavenEventsNew(renderer_class);
    road                  = HavenEventsNew(material_class, "mat_PB_3101101_asphalt_fixture");
    water                 = HavenEventsNew(material_class, "mat_PB_water");
    road->shader          = road_shader;
    water->shader         = water_shader;
    components            = {{&component_array_class.klass}, nullptr, 2,
                             {&road_renderer->header, &water_renderer->header}};
    road_materials        = {{&material_array_class.klass}, nullptr, 1, {&road->header}};
    water_materials       = {{&material_array_class.klass}, nullptr, 1, {&water->header}};
    owner->components     = &components;
    road_renderer->materials = &road_materials;
    water_renderer->materials = &water_materials;
  }
};

void HavenVisualEventsCases(std::string_view mode)
{
  Require(mode == "haven-visual-events", "unknown Haven event fixture mode");
  HavenFixtureGeometry camera("haven-zoom-native");
  auto& transform = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &component_class);
  auto& manager = Define("Digit.Prime.PlanetaryBase.PlanetaryStarbase", "PlanetaryStarbaseManager",
                         IL2CPP_TYPE_CLASS, &component_class);
  HavenEventsMethod(unity_object_class, "op_Implicit", &HavenEventsAlive, T(bool_class), {T(unity_object_class)}, true);
  HavenEventsMethod(unity_object_class, "GetInstanceID", &HavenEventsId, T(int_class), {});
  HavenEventsMethod(unity_object_class, "get_name", &HavenEventsName, T(string_class), {});
  HavenEventsMethod(game_object_class, "Find", &HavenEventsFind, T(game_object_class), {T(string_class)}, true);
  HavenEventsMethod(game_object_class, "get_activeInHierarchy", &HavenEventsActive, T(bool_class), {});
  HavenEventsMethod(game_object_class, "GetComponentsInChildren", &HavenEventsComponents, T(component_array_class),
                    {T(type_class), T(bool_class)});
  HavenEventsMethod(game_object_class, "get_transform", &HavenEventsTransform, T(transform), {});
  HavenEventsMethod(transform, "get_parent", &HavenEventsParent, T(transform), {});
  HavenEventsMethod(renderer_class, "get_sharedMaterials", &HavenEventsMaterials, T(material_array_class), {});
  HavenEventsMethod(renderer_class, "get_enabled", &HavenEventsEnabled, T(bool_class), {});
  HavenEventsMethod(renderer_class, "set_enabled", &HavenEventsSetEnabled, T(void_class), {T(bool_class)});
  HavenEventsMethod(material_class, "get_shader", &HavenEventsShader, T(shader_class), {});
  HavenEventsMethod(material_class, "HasProperty", &HavenEventsHasProperty, T(bool_class), {T(string_class)});
  HavenEventsMethod(material_class, "GetFloat", &HavenEventsGetDepth, T(float_class), {T(string_class)});
  HavenEventsMethod(material_class, "SetFloat", &HavenEventsSetDepth, T(void_class), {T(string_class), T(float_class)});
  HavenEventsMethod(manager, "get_Instance", &HavenEventsManager, T(manager), {}, true);
  HavenEventsMethod(manager, "get_AreInitialVisualsReady", &HavenEventsReady, T(bool_class), {});
  haven_events_manager = HavenEventsNew(manager);
  auto* road_shader    = HavenEventsNew(shader_class, "Custom/LitMSAE");
  auto* water_shader   = HavenEventsNew(shader_class, "Custom/WaterLite");
  HavenEventsScene a(transform, road_shader, water_shader), b(transform, road_shader, water_shader);
  auto* foreign       = HavenEventsNew(game_object_class);
  foreign->transform  = HavenEventsNew(transform);
  auto* native_manager = reinterpret_cast<haven_orbit_binding::Starbase*>(haven_events_manager);
  Il2CppObject streamer{&component_class.klass};
  int reset_originals = 0, clear_originals = 0, material_originals = 0, camera_originals = 0;
  auto reset = [&] {
    PlanetaryStarbaseManager_ResetInitialVisualLoadState_Hook(
        [&](auto* passed) {
          Require(passed == native_manager, "reset preserves its original native receiver");
          ++reset_originals;
          haven_events_ready = false;
        }, native_manager);
  };
  auto clear = [&] {
    PlanetaryStarbaseManager_ClearMapModuleVisualState_Hook(
        [&](auto* passed) {
          Require(passed == native_manager, "map clear preserves its original native receiver");
          ++clear_originals;
          haven_events_ready = false;
        }, native_manager);
  };
  auto material = [&](HavenEventsNode* holder, auto&& native_change) {
    haven_events_check_post_original = true;
    haven_events_native_finished    = false;
    AssetLODStreaming_RefreshMeshAndMaterial_Hook(
        [&](auto* passed, auto* object) {
          Require(passed == reinterpret_cast<il2cpp::Object*>(&streamer)
                      && object == reinterpret_cast<unity::GameObject*>(holder) && HavenFixtureRooted(holder),
                  "streaming preserves native arguments and roots the payload across its original");
          ++material_originals;
          native_change();
          haven_events_native_finished = true;
        }, reinterpret_cast<il2cpp::Object*>(&streamer), reinterpret_cast<unity::GameObject*>(holder));
    haven_events_check_post_original = false;
  };
  Config::Get().graphics.haven_zoom       = 4000;
  Config::Get().graphics.hide_haven_water = true;
  auto tick = [&] {
    BlendFrameProvider_UpdateCameraFrame_Hook(
        [&](auto*, auto*) {
          ++camera_originals;
          camera.result.position = {10, 20, 30};
          camera.result.clip     = 500;
        }, reinterpret_cast<BlendFrameProvider*>(&camera.blend), nullptr);
    HavenVisualsFrame();
  };
  auto verify_active = [&](HavenEventsScene& scene, HavenEventsNode* road) {
    Require(road->depth == std::array<float, 2>{-1, -1} && !scene.water_renderer->enabled,
            "actual discovery classifies and applies both road properties and independent water");
  };
  auto verify_idle = [&] {
    const auto finds = haven_events_finds, scans = haven_events_scans, readiness = haven_events_readiness;
    const auto reads = haven_events_depth_reads, writes = haven_events_depth_writes, water = haven_events_water_writes;
    const auto alive = haven_events_alive_reads, active = haven_events_active_reads;
    const auto visual_reads = haven_events_visual_reads;
    const auto strings = settings_values_strings.size();
    for (int i = 0; i < 20; ++i) {
      ++input_frame;
      tick();
    }
    Require(haven_events_finds == finds && haven_events_scans == scans && haven_events_readiness == readiness
                && haven_events_depth_reads == reads && haven_events_depth_writes == writes
                && haven_events_water_writes == water && haven_events_alive_reads == alive
                && haven_events_active_reads == active && haven_events_visual_reads == visual_reads
                && settings_values_strings.size() == strings,
            "verified idle frames do no Find, scans, readiness/material reads, writes or managed string allocation");
  };

  haven_events_found = a.owner;
  input_frame       = 100;
  reset();
  tick();
  haven_events_ready_failure = true;
  tick();
  haven_events_ready_failure = false;
  Require(haven_events_readiness == 2 && haven_events_finds == 0 && haven_events_scans == 0,
          "native false and unavailable readiness cannot authorize discovery");
  haven_events_ready = true;
  tick();
  Require(haven_events_finds == 0, "a ready generation waits until its native reset frame has finished");
  ++input_frame;
  tick();
  Require(reset_originals == 1 && haven_events_finds == 1 && haven_events_scans == 1,
          "initial ready generation performs one production discovery pass");
  verify_active(a, a.road);
  verify_idle();

  // Multiple native reset notifications coalesce, then visit B and return to A.
  reset();
  clear();
  haven_events_found = b.owner;
  a.owner->active    = false;
  tick();
  haven_events_ready = true;
  tick();
  Require(haven_events_finds == 1, "coalesced resets cannot scan during the native transition frame");
  ++input_frame;
  tick();
  Require(haven_events_finds == 2 && haven_events_scans == 2
              && a.road->depth == std::array<float, 2>{0.25f, 0.5f} && a.water_renderer->enabled,
          "A to B publishes once and restores A's checked original visual state");
  verify_active(b, b.road);
  clear();
  haven_events_found = a.owner;
  a.owner->active    = true;
  b.owner->active    = false;
  haven_events_ready = true;
  tick();
  ++input_frame;
  tick();
  Require(haven_events_scans == 3 && b.road->depth == std::array<float, 2>{0.25f, 0.5f}
              && b.water_renderer->enabled, "B to A reuses the holder safely and restores B");
  verify_active(a, a.road);

  material(foreign, [] {});
  tick();
  Require(haven_events_scans == 3, "a foreign streamed holder does not invalidate current Haven ownership");
  auto* replacement = HavenEventsNew(material_class, "mat_PB_3101701_asphalt_replacement");
  replacement->shader = road_shader;
  material(a.holder, [&] { a.road_materials.values[0] = &replacement->header; });
  material(a.holder, [] {}); // A batch can report several relevant holders in one frame.
  tick();
  Require(haven_events_scans == 3 && replacement->depth[0] == 0.25f,
          "native replacement is deferred until after its frame");
  ++input_frame;
  tick();
  Require(haven_events_scans == 4 && a.road->depth == std::array<float, 2>{0.25f, 0.5f},
          "same-holder material replacement rescans once and restores the retired material");
  verify_active(a, replacement);
  verify_idle();

  // A reentrant streaming event withdraws publication without clearing held spans.
  auto* after_scan = HavenEventsNew(material_class, "mat_PB_3101201_asphalt_during_scan");
  after_scan->shader = road_shader;
  reset();
  haven_events_ready = true;
  haven_events_on_scan = [&] {
    material(a.holder, [&] { a.road_materials.values[0] = &after_scan->header; });
  };
  const auto before_scan_writes = haven_events_depth_writes;
  ++input_frame;
  tick();
  Require(haven_events_scans == 5 && haven_events_depth_writes == before_scan_writes,
          "an event during discovery cannot publish or apply its superseded snapshot");
  tick();
  Require(haven_events_scans == 5, "reentrant scan invalidation cannot retry in the same native frame");
  ++input_frame;
  tick();
  Require(haven_events_scans == 6 && replacement->depth == std::array<float, 2>{0.25f, 0.5f},
          "superseding scan event gets one immediate next-frame discovery without waiting for failure recovery");
  verify_active(a, after_scan);
  verify_idle();

  auto* during_write = HavenEventsNew(material_class, "mat_PB_3101301_asphalt_during_write");
  during_write->shader = road_shader;
  material(a.holder, [&] { a.road_materials.values[0] = &during_write->header; });
  haven_events_on_write = [&](HavenEventsNode* node, size_t property, float value) {
    if (node != during_write || property != 0 || value != -1)
      return;
    haven_events_on_write = {};
    reset();
    haven_events_found = b.owner;
    a.owner->active    = false;
    b.owner->active    = true;
    HavenVisuals()->Apply(true, true); // Actual whole-episode reentry guard.
  };
  ++input_frame;
  tick();
  Require(haven_events_scans == 7 && during_write->depth == std::array<float, 2>{-1, 0.5f}
              && b.road->depth == std::array<float, 2>{0.25f, 0.5f},
          "reset during a setter blocks second stale property writes and recursive application");
  haven_events_ready = true;
  tick();
  Require(haven_events_scans == 7, "setter-time generation change cannot be acknowledged or retried that frame");
  ++input_frame;
  tick();
  Require(haven_events_scans == 8 && during_write->depth == std::array<float, 2>{0.25f, 0.5f},
          "next ready generation restores the retained partial write and discovers the new owner immediately");
  verify_active(b, b.road);
  verify_idle();
  Require(reset_originals == 4 && clear_originals == 2 && material_originals == 5,
          "all reset, clear and streaming callbacks deliver native original exactly once");

  // A failed pre-original payload root must not suppress native or lose the event.
  auto* without_payload = HavenEventsNew(material_class, "mat_PB_3101401_asphalt_payload_custody");
  without_payload->shader = road_shader;
  const auto payload_epoch = HavenVisuals()->Generation();
  const auto payload_scans = haven_events_scans;
  int payload_originals = 0;
  warp_root_failure = true;
  AssetLODStreaming_RefreshMeshAndMaterial_Hook(
      [&](auto* passed, auto* object) {
        ++payload_originals;
        Require(passed == reinterpret_cast<il2cpp::Object*>(&streamer)
                    && object == reinterpret_cast<unity::GameObject*>(b.holder) && !HavenFixtureRooted(b.holder),
                "failed payload custody still delivers the untouched native arguments once");
        Require(HavenVisuals()->Generation() == payload_epoch,
                "payload custody recovery does not invalidate ahead of native original");
        warp_root_failure = false;
        b.road_materials.values[0] = &without_payload->header;
      }, reinterpret_cast<il2cpp::Object*>(&streamer), reinterpret_cast<unity::GameObject*>(b.holder));
  Require(payload_originals == 1 && HavenVisuals()->Generation() != payload_epoch,
          "unavailable streamed payload custody queues recovery for the still-current relevant generation");
  tick();
  Require(haven_events_scans == payload_scans, "unknown payload ownership still defers until native finishes its frame");
  ++input_frame;
  tick();
  Require(haven_events_scans == payload_scans + 1 && b.road->depth == std::array<float, 2>{0.25f, 0.5f},
          "payload custody recovery immediately discovers native's replacement and restores the retired material");
  verify_active(b, without_payload);
  verify_idle();

  // Native may replace the generation while a failed payload remains unowned.
  const auto superseded_scans = haven_events_scans;
  uint64_t native_reset_epoch = 0;
  warp_root_failure = true;
  AssetLODStreaming_RefreshMeshAndMaterial_Hook(
      [&](auto*, auto*) {
        ++payload_originals;
        warp_root_failure = false;
        reset();
        native_reset_epoch = HavenVisuals()->Generation();
      }, reinterpret_cast<il2cpp::Object*>(&streamer), reinterpret_cast<unity::GameObject*>(b.holder));
  Require(payload_originals == 2 && HavenVisuals()->Generation() == native_reset_epoch,
          "an unavailable old payload cannot invalidate or declare ready the generation created by native");
  tick();
  Require(haven_events_scans == superseded_scans, "a native replacement generation retains its false readiness gate");
  haven_events_ready = true;
  ++input_frame;
  tick();
  Require(haven_events_scans == superseded_scans + 1,
          "superseding native reset recovers through its own next ready generation");
  verify_active(b, without_payload);

  // A known nonnull parent whose strong acquisition fails is unknown, not foreign.
  auto* without_parent = HavenEventsNew(material_class, "mat_PB_3101501_asphalt_parent_custody");
  without_parent->shader = road_shader;
  const auto parent_epoch = HavenVisuals()->Generation();
  const auto parent_scans = haven_events_scans, parent_originals = material_originals;
  haven_events_on_parent = [&] {
    Require(b.holder->transform->parent == b.owner->transform,
            "parent custody fault follows a successful known nonnull ancestry observation");
    warp_root_failure = true;
  };
  material(b.holder, [&] { b.road_materials.values[0] = &without_parent->header; });
  warp_root_failure = false;
  Require(material_originals == parent_originals + 1 && HavenVisuals()->Generation() != parent_epoch,
          "failed custody of a nonnull parent withdraws idle authority rather than accepting foreign ownership");
  tick();
  Require(haven_events_scans == parent_scans, "parent custody recovery respects the native frame boundary");
  ++input_frame;
  tick();
  Require(haven_events_scans == parent_scans + 1
              && without_payload->depth == std::array<float, 2>{0.25f, 0.5f},
          "parent custody recovery performs one immediate checked replacement discovery");
  verify_active(b, without_parent);
  verify_idle();

  // Discovery failure must release old live interventions even while the new camera remains qualified.
  const auto failure_scans = haven_events_scans, failure_finds = haven_events_finds;
  const auto failure_originals = reset_originals;
  reset();
  haven_events_found = a.owner;
  a.owner->active = true;
  b.owner->active = false;
  haven_events_ready = true;
  haven_events_components_failure = true;
  ++input_frame;
  tick();
  Require(reset_originals == failure_originals + 1 && haven_events_scans == failure_scans + 1
              && haven_events_finds == failure_finds + 1 && camera.result.position.z == -2970
              && without_parent->depth == std::array<float, 2>{0.25f, 0.5f} && b.water_renderer->enabled
              && during_write->depth == std::array<float, 2>{0.25f, 0.5f} && a.water_renderer->enabled,
          "failed discovery of a new ready owner restores old live road and water ownership in the qualified camera");
  const auto failure_writes = haven_events_depth_writes, failure_water = haven_events_water_writes;
  for (int i = 0; i < 3; ++i) {
    ++input_frame;
    tick();
  }
  Require(haven_events_scans == failure_scans + 1 && haven_events_finds == failure_finds + 1
              && haven_events_depth_writes == failure_writes && haven_events_water_writes == failure_water,
          "failed-only recovery bounds repeated full discovery and does not repeat verified restoration writes");
  haven_events_components_failure = false;
  clear(); // A new native generation must bypass the previous failed-scan retry delay.
  haven_events_ready = true;
  ++input_frame;
  tick();
  Require(haven_events_scans == failure_scans + 2 && haven_events_finds == failure_finds + 2,
          "a new ready native generation immediately bypasses only the superseded failure retry delay");
  verify_active(a, during_write);
  verify_idle();

  const auto final_scans = haven_events_scans, final_originals = camera_originals;
  Config::Get().graphics.haven_zoom       = 0;
  Config::Get().graphics.hide_haven_water = false;
  ++input_frame;
  tick();
  Require(haven_events_scans == final_scans && camera_originals == final_originals + 1
              && b.road->depth == std::array<float, 2>{0.25f, 0.5f} && b.water_renderer->enabled
              && during_write->depth == std::array<float, 2>{0.25f, 0.5f} && a.water_renderer->enabled,
          "leaving the qualified visual consumer restores ownership without another scene discovery");
  const auto disabled_epoch = HavenVisuals()->Generation();
  const auto disabled_roots = warp_next_root;
  int disabled_originals = 0;
  AssetLODStreaming_RefreshMeshAndMaterial_Hook(
      [&](auto*, auto*) { ++disabled_originals; }, reinterpret_cast<il2cpp::Object*>(&streamer),
      reinterpret_cast<unity::GameObject*>(b.holder));
  Require(disabled_originals == 1 && HavenVisuals()->Generation() == disabled_epoch
              && warp_next_root == disabled_roots && haven_events_scans == final_scans,
          "irrelevant streaming delivers native once without payload roots or generation recovery");
  std::cout << "Haven production visual events: readiness, next-frame coalescing, A-B-A, native material replacement, "
               "scan/write reentry and idle discovery suppression passed\n";
}
} // namespace
