// Focused local fixture: reuse the controlled runtime without modifying its existing driver or CI registration.
// Native pointer/rig observations are substituted; actual orbit callbacks, handles, focus/frame and camera calls run.
#define main ExistingBindingsMain
#define STFC_ORBIT_FIXTURE
#include "il2cpp_bindings.cc"
#undef STFC_ORBIT_FIXTURE
#undef main

#include "game/navigation_orbit.h"

namespace orbit_fixture
{
using Vector3 = unity::Vector3;
struct Rig {
  Il2CppObject header;
  Vector3      angles{45, 0, 0};
  Vector3      position{0, 0, -30};
};
struct Scene {
  Il2CppObject header;
  float        clip = 100, fov = 60;
  int32_t      height = 1000;
};
struct Zoom {
  Il2CppObject   header;
  NodeDepth      depth    = NodeDepth::SolarSystem;
  unity::Camera* camera   = nullptr;
  float          distance = 30, normalized = 0;
};
struct Pan {
  Il2CppObject                   header;
  NodeDepth                      depth    = NodeDepth::SolarSystem;
  unity::Camera*                 camera   = nullptr;
  navigation_orbit_binding::Poi* tracking = nullptr;
  Vector3                        center{};
  float                          radius = 100, speed = .5f;
  unity::Vector2                 drag{4, 2};
};
inline Rig*   rig       = nullptr;
inline Scene* scene     = nullptr;
inline Zoom*  zoom      = nullptr;
inline bool   in_system = true, pointer_allowed = true, pointer_known = true, binding_available = true;
inline bool   field_available = true, view_available = true, binding_present = true;
inline bool   clip_read_fails = false, clip_write_fails = false;
inline int    view_sets = 0, pointer_reads = 0, anchor_sets = 0, clip_sets = 0, clip_reads = 0;
inline int    transform_reads = 0, vector_reads = 0, position_reads = 0;
inline void (*clip_callback)() = nullptr;
inline MethodInfo angles_tag{}, position_tag{}, forward_tag{};

bool Focus(const MethodInfo*)
{ return true; }
float Clip(unity::Camera* object, const MethodInfo*)
{
  ++clip_reads;
  if (auto callback = std::exchange(clip_callback, nullptr))
    callback();
  if (clip_read_fails)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<Scene*>(object)->clip;
}
void SetClip(unity::Camera* object, float value, const MethodInfo*)
{
  ++clip_sets;
  if (clip_write_fails)
    throw Il2CppExceptionWrapper(&thrown);
  reinterpret_cast<Scene*>(object)->clip = value;
}
float Fov(unity::Camera* object, const MethodInfo*)
{ return reinterpret_cast<Scene*>(object)->fov; }
int32_t Height(unity::Camera* object, const MethodInfo*)
{ return reinterpret_cast<Scene*>(object)->height; }
bool Near(Vector3 a, Vector3 b)
{ return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f && std::abs(a.z - b.z) < .001f; }
} // namespace orbit_fixture

namespace orbit_fixture_binding
{
using Vector3 = unity::Vector3;
using Poi     = navigation_orbit_binding::Poi;
bool Finite(Vector3 v)
{ return navigation_orbit_binding::Finite(v); }
template <il2cpp::Name N, class T, class B> il2cpp::CallResult<T> Field(B* object)
{
  if (!object || !orbit_fixture::field_available)
    return {};
  if constexpr (std::is_same_v<B, NavigationZoom>) {
    const auto& value = *reinterpret_cast<orbit_fixture::Zoom*>(object);
    if constexpr (N.View() == "_depth")
      return {true, value.depth};
    else if constexpr (N.View() == "_sceneCamera")
      return {true, value.camera};
    else if constexpr (N.View() == "_actualDistance")
      return {true, value.distance};
    else if constexpr (N.View() == "<NormalizedZoom>k__BackingField")
      return {true, value.normalized};
    else
      return {};
  } else if constexpr (std::is_same_v<B, NavigationPan>) {
    const auto& value = *reinterpret_cast<orbit_fixture::Pan*>(object);
    if constexpr (N.View() == "_nodeDepth")
      return {true, value.depth};
    else if constexpr (N.View() == "_sceneCamera")
      return {true, value.camera};
    else if constexpr (N.View() == "_trackingPOI")
      return {true, value.tracking};
    else if constexpr (N.View() == "_systemPosition")
      return {true, value.center};
    else if constexpr (N.View() == "_viewRadius")
      return {true, value.radius};
    else if constexpr (N.View() == "_dragDelta")
      return {true, value.drag};
    else if constexpr (N.View() == "_minPanSpeed")
      return {true, value.speed};
    else
      return {};
  } else
    return {};
}
bool InSystem()
{ return orbit_fixture::in_system; }
bool Available()
{ return orbit_fixture::binding_available; }
il2cpp::CallResult<bool> PointerAvailable(bool)
{
  ++orbit_fixture::pointer_reads;
  return {orbit_fixture::pointer_known, orbit_fixture::pointer_allowed};
}
il2cpp::CallResult<unity::Transform*> Transform(Il2CppObject*)
{
  ++orbit_fixture::transform_reads;
  return orbit_fixture::rig
             ? il2cpp::CallResult<unity::Transform*>{true, reinterpret_cast<unity::Transform*>(orbit_fixture::rig)}
             : il2cpp::CallResult<unity::Transform*>{};
}
template <il2cpp::Name N> const MethodInfo* VectorGetter()
{
  if constexpr (N.View() == "get_localEulerAngles")
    return &orbit_fixture::angles_tag;
  else if constexpr (N.View() == "get_position")
    return &orbit_fixture::position_tag;
  else
    return &orbit_fixture::forward_tag;
}
il2cpp::CallResult<Vector3> Vector(const MethodInfo* method, Il2CppObject*)
{
  ++orbit_fixture::vector_reads;
  if (!orbit_fixture::rig || !orbit_fixture::view_available)
    return {};
  if (method == &orbit_fixture::angles_tag)
    return {true, orbit_fixture::rig->angles};
  if (method == &orbit_fixture::position_tag) {
    ++orbit_fixture::position_reads;
    return {true, orbit_fixture::rig->position};
  }
  return {true, {0, 0, 1}};
}
bool SetView(unity::Transform*, Vector3 angles, float distance)
{
  if (!orbit_fixture::rig || !orbit_fixture::view_available)
    return false;
  ++orbit_fixture::view_sets;
  orbit_fixture::rig->angles   = angles;
  orbit_fixture::rig->position = {0, 0, -distance};
  return true;
}
bool Reanchor(NavigationZoom*)
{
  ++orbit_fixture::anchor_sets;
  return true;
}
} // namespace orbit_fixture_binding

namespace Shortcut
{
bool HasBinding(Action)
{ return orbit_fixture::binding_present; }
} // namespace Shortcut
#define navigation_orbit_binding orbit_fixture_binding
#include "../src/mod/features/navigation_orbit.cc"
#undef navigation_orbit_binding
namespace orbit_under_test = navigation_orbit;

namespace
{
int  orbit_checks = 0;
void OrbitCheck(bool value, const char* message)
{
  ++orbit_checks;
  Require(value, message);
}

void RunOrbitCases()
{
  using namespace orbit_fixture;
  auto& camera_class    = Define("UnityEngine", "Camera", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& transform_class = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& zoom_class      = Define("Digit.Prime.Navigation", "NavigationZoom", IL2CPP_TYPE_CLASS, &object_class);
  auto& pan_class       = Define("Digit.Prime.Navigation", "NavigationPan", IL2CPP_TYPE_CLASS, &object_class);
  auto& app_class       = Define("UnityEngine", "Application", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(app_class, "get_isFocused", reinterpret_cast<void*>(&Focus), T(bool_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(camera_class, "get_farClipPlane", reinterpret_cast<void*>(&Clip), T(float_class), {});
  AddMethod(camera_class, "set_farClipPlane", reinterpret_cast<void*>(&SetClip), T(void_class), {T(float_class)});
  AddMethod(camera_class, "get_fieldOfView", reinterpret_cast<void*>(&Fov), T(float_class), {});
  AddMethod(camera_class, "get_pixelHeight", reinterpret_cast<void*>(&Height), T(int_class), {});
  Rig   transform{{&transform_class.klass}};
  Scene camera{{&camera_class.klass}};
  Zoom  object{{&zoom_class.klass}};
  Pan   pan{{&pan_class.klass}};
  rig           = &transform;
  scene         = &camera;
  zoom          = &object;
  object.camera = pan.camera = reinterpret_cast<unity::Camera*>(&camera);
  auto* owner                = reinterpret_cast<NavigationZoom*>(&object);
  auto* panner               = reinterpret_cast<NavigationPan*>(&pan);
  auto& config               = Config::Get();
  OrbitCheck(config.graphics.system_camera_orbit && config.patches.navigationorbithooks,
             "system camera orbit and its hook group are available by default");
  OrbitCheck(config.patches.havenzoomhooks, "Haven keeps its existing enabled hook-group default");
  config.graphics.system_camera_orbit = false;
  config.control.hotkeys_enabled      = true;
  config.control.use_scopely_hotkeys  = false;
  selected                            = nullptr;
  has_event_system                    = true;
  visual_alive                        = true;
  const auto initial_roots            = warp_roots.size();
  NavigationOrbit().installed         = true;
  OrbitCheck(orbit_under_test::Available(), "runtime orbit capability reflects a successful installation");
  int        originals = 0, pans = 0, clamps = 0;
  float      native_yaw = 0;
  const auto frame      = [&] {
    ++input_frame;
    NavigationZoom_UpdateCameraPosition_OrbitHook(
        [&](NavigationZoom* actual) {
          OrbitCheck(actual == owner, "native zoom retains its receiver");
          ++originals;
          transform.angles   = {45, native_yaw, 0};
          transform.position = {0, 0, -object.distance};
        },
        owner);
  };
  const auto bind_pan = [&] {
    NavigationPan_MoveCamera_OrbitHook([&](NavigationPan*, unity::Vector2, bool) { ++pans; }, panner, {}, true);
  };
  const auto clamp = [&] {
    return NavigationPan_ClampPositionInside_OrbitHook(
        [&](NavigationPan*, unity::Vector3) {
          ++clamps;
          return unity::Vector3{1, 2, 3};
        },
        panner, {8, 9, 10});
  };
  fleet_native_keys = {Action::kNavigationOrbitDrag};
  camera_mouse      = {10, 20, 0};
  frame();
  bind_pan();
  OrbitCheck(originals == 1 && view_sets == 0 && !NavigationOrbit().zoom && camera.clip == 100,
             "disabled orbit preserves native update, orientation, clip and ownership");
  OrbitCheck(Near(clamp(), {1, 2, 3}) && clamps == 1, "disabled clamp remains native");

  config.graphics.system_camera_orbit = true;
  binding_present                     = false;
  OrbitCheck(!NavigationOrbitEnabled(), "an explicitly empty drag binding suppresses default-on system rotation");
  binding_present                     = true;
  config.patches.navigationorbithooks = false;
  OrbitCheck(NavigationOrbitEnabled(), "restart-only hook switch is not a live feature toggle after installation");
  config.patches.navigationorbithooks = true;
  fleet_native_keys.clear();
  const auto quiet_roots      = warp_next_root;
  const auto quiet_transforms = transform_reads, quiet_vectors = vector_reads;
  const auto quiet_clips     = clip_reads;
  const auto quiet_originals = originals;
  int        quiet_late_originals = 0;
  for (int i = 0; i < 240; ++i) {
    NavigationZoom_LateUpdate_OrbitHook(
        [&](NavigationZoom* actual) {
          OrbitCheck(actual == owner, "quiet LateUpdate retains its native receiver");
          ++quiet_late_originals;
          frame();
        },
        owner);
  }
  OrbitCheck(originals == quiet_originals + 240 && transform_reads == quiet_transforms && vector_reads == quiet_vectors
                 && clip_reads == quiet_clips && warp_next_root == quiet_roots && !NavigationOrbit().zoom
                 && quiet_late_originals == 240,
             "240 enabled idle callbacks preserve every original with zero rig/clip reads or handle allocations");
  fleet_native_keys = {Action::kNavigationOrbitDrag};
  frame();
  bind_pan();
  OrbitCheck(NavigationOrbit().zoom && NavigationOrbit().dragging && !NavigationOrbit().overridden,
             "first held frame acquires system rig but does not jump orientation");
  camera_mouse = {30, 50, 0};
  frame();
  OrbitCheck(NavigationOrbit().overridden && std::abs(transform.angles.y - 3) < .001f,
             "subsequent pointer movement applies production yaw");
  OrbitCheck(anchor_sets == 1, "rotation reanchors ordinary mouse zoom exactly once");
  OrbitCheck(camera.clip > 100 && clip_sets > 0, "rotated system extends owned camera clip");
  OrbitCheck(Near(clamp(), {8, 9, 10}) && clamps == 1, "only rotated system bypasses pan boundary");

  const auto same_frame_yaw     = transform.angles.y;
  const auto same_frame_anchors = anchor_sets;
  NavigationZoom_UpdateCameraPosition_OrbitHook(
      [&](NavigationZoom*) {
        ++originals;
        transform.angles = {45, 0, 0};
      },
      owner);
  OrbitCheck(transform.angles.y == same_frame_yaw && anchor_sets == same_frame_anchors,
             "a second original in one frame still reapplies orbit without advancing input twice");
  const auto before_late_views = view_sets, before_late_positions = position_reads;
  const auto before_late_reads = clip_reads, before_late_writes = clip_sets;
  int        late_originals = 0, late_position_originals = 0;
  NavigationZoom_LateUpdate_OrbitHook(
      [&](NavigationZoom* actual) {
        OrbitCheck(actual == owner, "LateUpdate retains its native receiver");
        ++late_originals;
        camera.clip = 100;
        for (int pass = 0; pass < 2; ++pass) {
          NavigationZoom_UpdateCameraPosition_OrbitHook(
              [&](NavigationZoom* position_owner) {
                OrbitCheck(position_owner == actual, "each LateUpdate pose write retains its receiver");
                ++late_position_originals;
                object.distance    = 40.0f + 10.0f * static_cast<float>(pass);
                transform.angles   = {45, 0, 0};
                transform.position = {0, 0, -object.distance};
              },
              actual);
          OrbitCheck(view_sets == before_late_views + pass + 1 && transform.angles.y == same_frame_yaw,
                     "each native LateUpdate pose write receives its immediate orbit correction");
          OrbitCheck(position_reads == before_late_positions && clip_reads == before_late_reads
                         && clip_sets == before_late_writes,
                     "LateUpdate defers both intermediate clip passes until its native work finishes");
        }
      },
      owner);
  OrbitCheck(late_originals == 1 && late_position_originals == 2 && view_sets == before_late_views + 2
                 && position_reads == before_late_positions + 1 && clip_reads == before_late_reads + 1
                 && clip_sets == before_late_writes + 1 && NavigationOrbit().distance == 50
                 && Near(transform.position, {0, 0, -50}) && camera.clip == 450 && !NavigationOrbitUpdates(),
             "one native LateUpdate preserves both pose corrections and completes exactly one final clip pass");

  const auto no_clip_observation = [&](const char* message) {
    const auto before_positions = position_reads, before_reads = clip_reads, before_writes = clip_sets;
    navigation_orbit::ApplyDrawDistance(owner);
    OrbitCheck(position_reads == before_positions && clip_reads == before_reads && clip_sets == before_writes,
               message);
  };
  NavigationOrbit().pan = {};
  no_clip_observation("a missing pan owner skips boxed camera position and clip observations");
  bind_pan();
  Scene mismatched_camera{{&camera_class.klass}};
  pan.camera = reinterpret_cast<unity::Camera*>(&mismatched_camera);
  no_clip_observation("a mismatched pan camera skips boxed camera position and clip observations");
  pan.camera = object.camera;
  pan.depth  = NodeDepth::Galaxy;
  no_clip_observation("a mismatched pan depth skips boxed camera position and clip observations");
  pan.depth  = NodeDepth::SolarSystem;
  pan.radius = 0;
  no_clip_observation("an unusable pan radius skips boxed camera position and clip observations");
  pan.radius   = 100;
  pan.center.x = std::numeric_limits<float>::quiet_NaN();
  no_clip_observation("a nonfinite pan center skips boxed camera position and clip observations");
  pan.center = {};

  const auto before_coalesced_reads = clip_reads, before_coalesced_writes = clip_sets;
  {
    navigation_orbit::ZoomUpdateScope outer(owner);
    navigation_orbit::ApplyDrawDistance(owner);
    NavigationZoom_UpdateCameraPosition_OrbitHook(
        [&](NavigationZoom*) {
          ++originals;
          object.distance    = 60;
          transform.angles   = {45, 0, 0};
          transform.position = {0, 0, -object.distance};
        },
        owner);
    camera.clip = 100; // The post-native zoom pass authors its baseline again.
    navigation_orbit::ApplyDrawDistance(owner);
    OrbitCheck(clip_reads == before_coalesced_reads && clip_sets == before_coalesced_writes,
               "zoom's pre/native/post span defers clip observations and writes until the final camera pose");
    outer.Complete();
  }
  OrbitCheck(clip_reads == before_coalesced_reads + 1 && clip_sets == before_coalesced_writes + 1 && camera.clip > 100
                 && transform.angles.y == same_frame_yaw && NavigationOrbit().distance == 60
                 && Near(transform.position, {0, 0, -60}),
             "one final clip pass observes the native zoom distance and preserves the final orbit pose");
  const auto before_unwind_reads = clip_reads, before_unwind_writes = clip_sets;
  try {
    navigation_orbit::ZoomUpdateScope failed(owner);
    navigation_orbit::ApplyDrawDistance(owner);
    throw std::runtime_error("synthetic native unwind");
  } catch (const std::runtime_error&) {
  }
  OrbitCheck(clip_reads == before_unwind_reads && clip_sets == before_unwind_writes && !NavigationOrbitUpdates(),
             "exception unwinding pops the authoring scope without fresh managed clip work");
  clip_callback = [] { navigation_orbit::ApplyDrawDistance(reinterpret_cast<NavigationZoom*>(orbit_fixture::zoom)); };
  navigation_orbit::ApplyDrawDistance(owner);
  OrbitCheck(clip_reads == before_unwind_reads + 1 && clip_sets == before_unwind_writes && !NavigationOrbitUpdates(),
             "managed getter reentry during a final pass cannot recursively read or write the clip");

  fleet_native_keys = {Action::kNavigationOrbitReset};
  frame();
  OrbitCheck(!NavigationOrbit().overridden && Near(transform.angles, {45, 0, 0}), "reset restores native orientation");
  OrbitCheck(camera.clip == 100, "reset restores the orbit-owned clip baseline");

  fleet_native_keys = {Action::kNavigationOrbitDrag};
  camera_mouse      = {10, 20, 0};
  frame();
  camera_mouse = {40, 20, 0};
  frame();
  OrbitCheck(NavigationOrbit().overridden && camera.clip > 100, "rotation can resume after reset");
  config.graphics.system_camera_orbit = false;
  frame();
  OrbitCheck(!NavigationOrbit().zoom && !NavigationOrbit().overridden && Near(transform.angles, {45, 0, 0})
                 && camera.clip == 100,
             "live disable releases rig and restores owned orientation and clip");
  OrbitCheck(warp_roots.size() == initial_roots, "disable releases all orbit roots");

  const auto rotate = [&] {
    config.graphics.system_camera_orbit = true;
    camera_mouse                        = {10, 20, 0};
    frame();
    bind_pan();
    camera_mouse = {40, 20, 0};
    frame();
    OrbitCheck(NavigationOrbit().overridden && camera.clip > 100, "cleanup retry begins from an owned clip");
  };
  rotate();
  clip_read_fails                     = true;
  config.graphics.system_camera_orbit = false;
  frame();
  OrbitCheck(!NavigationOrbit().zoom && NavigationOrbit().clip_camera && camera.clip > 100,
             "unavailable clip read retains cleanup ownership rather than inventing zero or abandoning it");
  clip_read_fails = false;
  NavigationOrbitTick();
  OrbitCheck(!NavigationOrbit().clip_camera && camera.clip == 100,
             "later frame retries clip cleanup even after its zoom rig was released");
  rotate();
  clip_write_fails                    = true;
  config.graphics.system_camera_orbit = false;
  frame();
  OrbitCheck(NavigationOrbit().clip_camera && camera.clip > 100,
             "failed native clip setter keeps the original baseline and its cleanup owner");
  clip_write_fails = false;
  NavigationOrbitTick();
  OrbitCheck(!NavigationOrbit().clip_camera && camera.clip == 100,
             "transient native setter failure can recover without re-enabling orbit");
  OrbitCheck(warp_roots.size() == initial_roots, "completed retry cleanup releases every retained camera root");

  config.graphics.system_camera_orbit = true;
  camera_mouse                        = {10, 20, 0};
  frame();
  bind_pan();
  camera_mouse = {40, 20, 0};
  frame();
  camera.clip                         = 222;
  config.graphics.system_camera_orbit = false;
  frame();
  OrbitCheck(camera.clip == 222, "disable cannot overwrite a newer native or other-feature clip value");

  config.graphics.system_camera_orbit = true;
  camera.clip                         = 100;
  camera_mouse                        = {10, 20, 0};
  frame();
  bind_pan();
  camera_mouse = {40, 20, 0};
  frame();
  OrbitCheck(NavigationOrbit().overridden, "native-rig replacement begins from an actual orbit override");
  const auto before_new_rig = view_sets;
  native_yaw                = 17;
  frame();
  OrbitCheck(!NavigationOrbit().zoom && !NavigationOrbit().overridden && transform.angles.y == 17
                 && view_sets == before_new_rig,
             "post-native rig rejection never overwrites newly authored native yaw with an old orientation");
  OrbitCheck(camera.clip == 100, "native rig replacement releases its orbit-owned clip extension");
  native_yaw = 0;
  rotate();
  transform.angles               = {30, 17, 0}; // A new section's camera owner has already authored its view.
  in_system                      = false;
  const auto before_section_exit = view_sets;
  NavigationOrbitTick();
  OrbitCheck(!NavigationOrbit().zoom && !NavigationOrbit().overridden && transform.angles.y == 17
                 && view_sets == before_section_exit,
             "direct frame cleanup on section exit never restores a stale system view over its new native owner");
  OrbitCheck(camera.clip == 100, "section exit still releases the orbit-owned clip extension");

  in_system = true;
  rotate();
  transform.angles                    = {32, 19, 0};
  in_system                           = false;
  config.graphics.system_camera_orbit = false;
  const auto before_off_exit          = view_sets;
  NavigationOrbitTick();
  OrbitCheck(!NavigationOrbit().zoom && transform.angles.y == 19 && view_sets == before_off_exit && camera.clip == 100,
             "simultaneous feature disable and section exit preserves the new section's native view");

  in_system = true;
  rotate();
  Zoom replacement{{&zoom_class.klass}};
  replacement.camera             = object.camera;
  auto*      replacement_owner   = reinterpret_cast<NavigationZoom*>(&replacement);
  const auto before_owner_change = view_sets;
  transform.angles               = {37, 23, 0};
  ++input_frame;
  NavigationZoom_UpdateCameraPosition_OrbitHook(
      [&](NavigationZoom* actual) {
        OrbitCheck(actual == replacement_owner && transform.angles.y == 23 && view_sets == before_owner_change,
                   "owner replacement does not stamp the old system transform before native camera processing");
        transform.angles = {40, 31, 0};
      },
      replacement_owner);
  OrbitCheck(!NavigationOrbit().zoom && !NavigationOrbit().overridden && transform.angles.y == 31
                 && view_sets == before_owner_change,
             "a replacement owner's native yaw survives both pre-native cleanup and the post-native rig gate");
  OrbitCheck(warp_roots.size() == initial_roots && camera.clip == 100,
             "owner replacement releases old rig roots and its owned clip without altering the new view");

  rotate();
  const auto before_nested_reads = clip_reads, before_nested_writes = clip_sets;
  {
    navigation_orbit::ZoomUpdateScope outer(owner);
    {
      navigation_orbit::ZoomUpdateScope inner(owner);
      navigation_orbit::ApplyDrawDistance(owner);
      inner.Complete();
    }
    {
      navigation_orbit::ZoomUpdateScope other(replacement_owner);
      navigation_orbit::ApplyDrawDistance(replacement_owner);
      other.Complete();
    }
    OrbitCheck(clip_reads == before_nested_reads && clip_sets == before_nested_writes,
               "nested matching scopes coalesce while unrelated receiver scopes cannot flush this owner");
    outer.Complete();
  }
  OrbitCheck(clip_reads == before_nested_reads + 1 && clip_sets == before_nested_writes,
             "nested same-receiver spans finish with one clip read and avoid an unchanged write");
  const auto before_takeover_reads = clip_reads, before_takeover_writes = clip_sets;
  {
    navigation_orbit::ZoomUpdateScope old_owner(owner);
    navigation_orbit::ApplyDrawDistance(owner);
    NavigationZoom_UpdateCameraPosition_OrbitHook([&](NavigationZoom*) { transform.angles = {45, 0, 0}; },
                                                  replacement_owner);
    old_owner.Complete();
  }
  OrbitCheck(NavigationOrbit().zoom.Get() == replacement_owner && !NavigationOrbit().overridden
                 && clip_reads == before_takeover_reads + 1 && clip_sets == before_takeover_writes + 1
                 && camera.clip == 100,
             "nested receiver takeover restores the old clip once and discards its pending completion");
  config.graphics.system_camera_orbit = false;
  NavigationOrbitTick();
  OrbitCheck(warp_roots.size() == initial_roots, "nested takeover cleanup releases all retained owners");
  camera.clip = 222;

  config.graphics.system_camera_orbit = true;
  const auto before_views             = view_sets;
  in_system                           = false;
  object.depth                        = NodeDepth::Galaxy;
  frame();
  bind_pan();
  OrbitCheck(!NavigationOrbit().zoom && view_sets == before_views && camera.clip == 222,
             "Galaxy remains native even with the system setting enabled");
  object.depth = NodeDepth::Starbase;
  frame();
  OrbitCheck(!NavigationOrbit().zoom && view_sets == before_views,
             "station or Haven sections never acquire the system rig");

  in_system       = true;
  object.depth    = NodeDepth::SolarSystem;
  pointer_allowed = false;
  camera_mouse    = {10, 20, 0};
  frame();
  camera_mouse = {50, 20, 0};
  frame();
  OrbitCheck(!NavigationOrbit().overridden && view_sets == before_views,
             "pointer-owned UI blocks starting system rotation");
  pointer_allowed = true;
  pointer_known   = false;
  frame();
  OrbitCheck(!orbit_under_test::Available() && !NavigationOrbit().installed && !NavigationOrbit().zoom,
             "unavailable pointer capability retains native behavior and disables this orbit owner");
  pointer_known     = true;
  binding_available = false;
  OrbitCheck(!InstallNavigationOrbit(), "unavailable installation capability refuses orbit hooks");
  OrbitCheck(warp_roots.size() == initial_roots, "failure paths release every retained rig");
  fleet_native_keys.clear();
  std::cout << "System orbit production callbacks: " << orbit_checks << " checks passed\n";
}
} // namespace

int main()
try {
  BuildRuntime();
  il2cpp::SetErrorHandler([](std::string_view message) { errors.emplace_back(message); });
  RunOrbitCases();
  return 0;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
