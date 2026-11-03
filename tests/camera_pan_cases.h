#pragma once

// The production camera.cc callbacks are already included by camera_bindings_cases.h. Native movement and
// tutorial methods, static storage and the input resolver belong to this isolated controlled-runtime process.
// Shortcut action/capture ownership uses the existing fixture boundary; this is not native device/UI testing.
namespace camera_pan_fixture
{
struct PanObject {
  Il2CppObject   header;
  unity::Vector2 last{4.0f, -2.0f};
  float          delta = 2.0f, keyboard = 3.0f;
  Il2CppObject*  tracking = nullptr;
  float          normal = 7.0f, extended = 19.0f;
};
struct TouchObject {
  Il2CppObject      header;
  unity::TouchPhase phase = unity::TouchPhase::Stationary;
};
using Resolver                         = Il2CppMethodPointer (*)(const char*, bool&);
bool                        mouse_held = false, missing_mouse = false, missing_touch = false, block = false;
int                         touch_count = 0, originals = 0, tutorial_calls = 0, shortcut_calls = 0;
std::vector<unity::Vector2> moves;
Il2CppObject                tutorial_object{};
bool                        tutorial_allowed = true, shortcuts_allowed = true;
enum class Replacement { None, DisableKeyboard, Falloff };
Replacement replacement          = Replacement::None;
bool        replace_in_shortcuts = false;
Config*     replacement_config   = nullptr;

void ReplacePolicy()
{
  if (replacement == Replacement::None)
    return;
  bindings_replace_config                   = true;
  auto& next                                = Config::Get();
  next.graphics.system_pan_momentum_falloff = 0.25f;
  if (replacement == Replacement::DisableKeyboard)
    next.control.hotkeys_enabled = false;
  replacement_config = &next;
  replacement        = Replacement::None;
}

bool MouseHeld(int32_t button)
{
  Require(button == 0, "pan acquires the primary drag button");
  return mouse_held;
}
int32_t TouchCount()
{ return touch_count; }
Il2CppMethodPointer Resolve(const char* name, bool& handled)
{
  if (std::string_view(name) == "UnityEngine.Input::GetMouseButton(System.Int32)") {
    handled = true;
    return missing_mouse ? nullptr : reinterpret_cast<Il2CppMethodPointer>(&MouseHeld);
  }
  if (std::string_view(name) == "UnityEngine.Input::get_touchCount()") {
    handled = true;
    return missing_touch ? nullptr : reinterpret_cast<Il2CppMethodPointer>(&TouchCount);
  }
  return nullptr;
}
struct InputResolver {
  Resolver previous = input_icall_override;
  InputResolver()
  { input_icall_override = Resolve; }
  ~InputResolver()
  { input_icall_override = previous; }
};
bool ReadStatic(FieldInfo* field, void* output)
{
  if (std::string_view(field->name) != "BlockPan")
    return false;
  std::memcpy(output, &block, sizeof(block));
  return true;
}
TutorialManager* Tutorial(const MethodInfo*)
{ return reinterpret_cast<TutorialManager*>(&tutorial_object); }
bool SceneAllowed(TutorialManager* actual, const MethodInfo*)
{
  Require(actual == reinterpret_cast<TutorialManager*>(&tutorial_object), "tutorial gate receives its native owner");
  ++tutorial_calls;
  if (!replace_in_shortcuts)
    ReplacePolicy();
  return tutorial_allowed;
}
bool ShortcutsAllowed(const MethodInfo*)
{
  ++shortcut_calls;
  if (replace_in_shortcuts)
    ReplacePolicy();
  return shortcuts_allowed;
}
void Move(NavigationPan*, unity::Vector2 delta, bool momentum, const MethodInfo*)
{
  Require(momentum, "pan retains the native momentum argument");
  moves.push_back(delta);
}
bool Near(unity::Vector2 actual, unity::Vector2 expected)
{ return std::abs(actual.x - expected.x) < 0.0001f && std::abs(actual.y - expected.y) < 0.0001f; }
bool NoMovement()
{
  return std::ranges::all_of(moves, [](auto delta) { return Near(delta, {}); });
}

void Run(std::string_view mode)
{
  InputResolver input;
  const auto    roots     = warp_roots.size();
  auto&         pan       = Define("Digit.Prime.Navigation", "NavigationPan", IL2CPP_TYPE_CLASS, &object_class);
  auto&         vector2   = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto&         poi       = Define("Digit.Prime.Navigation", "POI", IL2CPP_TYPE_CLASS, &object_class);
  auto&         tutorial  = Define("Digit.Prime.Tutorial", "TutorialManager", IL2CPP_TYPE_CLASS, &object_class);
  auto&         hub       = Define("Digit.Client.Core", "Hub", IL2CPP_TYPE_CLASS, &object_class);
  auto&         shortcuts = Define("Digit.Prime.GameInput", "ShortcutsManager", IL2CPP_TYPE_CLASS, &object_class);
  pan.klass.instance_size = sizeof(PanObject);
  if (mode != "camera-pan-missing-block")
    AddField(pan, "BlockPan", mode == "camera-pan-foreign-block" ? int_class : bool_class, 0, true);
  if (mode != "camera-pan-missing-last")
    AddField(pan, "_lastDelta", vector2, offsetof(PanObject, last));
  if (mode != "camera-pan-missing-delta")
    AddField(pan, "_deltaScaler", float_class, offsetof(PanObject, delta));
  if (mode != "camera-pan-missing-speed")
    AddField(pan, "_keyboardDeltaScaler", float_class, offsetof(PanObject, keyboard));
  if (mode != "camera-pan-missing-tracking")
    AddField(pan, "_trackingPOI", mode == "camera-pan-foreign-tracking" ? int_class : poi,
             offsetof(PanObject, tracking));
  AddField(pan, "_farMagRadiusRatioSystemNormal", float_class, offsetof(PanObject, normal));
  AddField(pan, "_farMagRadiusRatioSystemExtended", float_class, offsetof(PanObject, extended));
  AddMethod(pan, "MoveCamera", reinterpret_cast<void*>(&Move), T(void_class), {T(vector2), T(bool_class)});
  const auto& tutorial_getter = AddMethod(hub, "get_TutorialManager", reinterpret_cast<void*>(&Tutorial), T(tutorial),
                                          {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(hub, "TutorialManager", &tutorial_getter);
  AddMethod(tutorial, "IsSceneInputAllowed", reinterpret_cast<void*>(&SceneAllowed), T(bool_class), {});
  const auto& shortcuts_getter = AddMethod(shortcuts, "get_CanUseShortcuts", reinterpret_cast<void*>(&ShortcutsAllowed),
                                           T(bool_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(shortcuts, "CanUseShortcuts", &shortcuts_getter);
  tutorial_object.klass          = &tutorial.klass;
  tow_static_field_read_override = ReadStatic;

  PanObject  object{{&pan.klass}};
  auto*      receiver = reinterpret_cast<NavigationPan*>(&object);
  const auto original = [&](NavigationPan* actual) {
    Require(actual == receiver, "native pan receives its original receiver");
    ++originals;
    object.last = {8.0f, 6.0f};
    return true;
  };
  auto& config                                = Config::Get();
  config.control.hotkeys_enabled              = true;
  config.control.use_scopely_hotkeys          = false;
  config.patches.hotkeyhooks                  = true;
  config.ui.disable_move_keys                 = false;
  config.graphics.disable_target_camera_lock  = false;
  config.graphics.system_pan_momentum_falloff = 0.5f;
  auto* previous_selection                    = selected;
  selected                                    = nullptr;
  fleet_native_keys                           = {Action::kMoveRight};
  fleet_native_capture                        = false;
  const auto frame                            = [&] {
    ++input_frame;
    return NavigationPan_LateUpdate_Hook(original, receiver);
  };

  if (mode.starts_with("camera-pan-missing-") || mode.starts_with("camera-pan-foreign-")) {
    missing_mouse   = mode == "camera-pan-missing-mouse";
    missing_touch   = mode == "camera-pan-missing-touch";
    const auto last = object.last;
    Require(frame() && moves.empty() && Near(object.last, last) && object.extended == 19.0f,
            "unavailable required pan observations cannot move the camera or publish default-valued state");
  } else if (mode == "camera-pan-capture-native") {
    config.control.use_scopely_hotkeys = true;
    fleet_native_capture               = true; // A captured W/other non-arrow must still own the native route.
    object.last                        = {};
    frame();
    Require(originals == 0 && NoMovement(), "shortcut capture blocks native keyboard pan without a held arrow");
  } else if (mode == "camera-pan-policy-reentry" || mode == "camera-pan-falloff-reentry") {
    const auto last      = object.last;
    replace_in_shortcuts = mode == "camera-pan-falloff-reentry";
    replacement          = replace_in_shortcuts ? Replacement::Falloff : Replacement::DisableKeyboard;
    if (replace_in_shortcuts)
      fleet_native_keys.clear();
    frame();
    Require(replacement_config && moves.empty() && Near(object.last, last) && object.extended == 19.0f,
            "configuration replacement inside a managed input gate invalidates the pending pan command");
    if (replace_in_shortcuts) {
      frame();
      Require(moves.size() == 1 && Near(moves.back(), {1.0f, -0.5f}) && Near(object.last, {1.0f, -0.5f}),
              "the next stable frame glides with the current falloff rather than a retained old config");
    }
  } else if (mode == "camera-pan-native") {
    config.control.use_scopely_hotkeys = true;
    frame();
    Require(originals == 1 && moves.empty() && Near(object.last, {8.0f, 6.0f}) && object.extended == object.normal,
            "configured native keyboard mode runs native pan once without a second mod glide");
  } else if (mode == "camera-pan-diagonal") {
    fleet_native_keys = {Action::kMoveRight, Action::kMoveUp};
    frame();
    const float component = -3.0f / std::sqrt(2.0f);
    Require(moves.size() == 1 && Near(moves.back(), {2.0f * component, 2.0f * component})
                && Near(object.last, {component, component}),
            "diagonal keyboard pan is normalized and uses one native movement per frame");
  } else if (mode == "camera-pan-opposed") {
    object.last       = {};
    fleet_native_keys = {Action::kMoveRight, Action::kMoveLeft};
    frame();
    Require(moves.size() <= 1 && NoMovement() && Near(object.last, {}), "opposite directions cancel");
  } else if (mode == "camera-pan-coast") {
    fleet_native_keys.clear();
    frame();
    Require(moves.size() == 1 && Near(moves.back(), {2.0f, -1.0f}) && Near(object.last, {2.0f, -1.0f}),
            "released drag delta decays by the configured per-update falloff and moves once");
  } else if (mode == "camera-pan-pointer" || mode == "camera-pan-touch") {
    mouse_held      = mode == "camera-pan-pointer";
    touch_count     = mode == "camera-pan-touch" ? 1 : 0;
    const auto last = object.last;
    frame();
    Require(moves.empty() && Near(object.last, last), "active mouse drag or touch leaves native pointer motion intact");
  } else if (mode == "camera-pan-blocked" || mode == "camera-pan-tracking") {
    block = mode == "camera-pan-blocked";
    Il2CppObject tracked{&poi.klass};
    if (!block)
      object.tracking = &tracked;
    frame();
    Require(moves.empty() && Near(object.last, {}), "native pan block or target lock clears momentum without moving");
  } else if (mode == "camera-pan-unlock") {
    Il2CppObject tracked{&poi.klass};
    object.tracking                            = &tracked;
    config.graphics.disable_target_camera_lock = true;
    frame();
    Require(object.tracking == nullptr && moves.size() == 1 && Near(moves.back(), {-6.0f, 0.0f}),
            "the existing explicit target-unlock setting clears tracking before keyboard pan");
  } else if (mode == "camera-pan-modal" || mode == "camera-pan-tutorial") {
    object.last       = {};
    shortcuts_allowed = mode != "camera-pan-modal";
    tutorial_allowed  = mode != "camera-pan-tutorial";
    frame();
    Require(NoMovement() && originals == 0, "native tutorial/modal gates refuse keyboard movement");
  } else {
    Require(mode == "camera-pan-stationary-touch", "known pan fixture mode");
    auto& phase   = Define("UnityEngine", "TouchPhase", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::TouchPhase));
    phase.is_enum = true;
    auto& touch   = Define("", "TKTouch", IL2CPP_TYPE_CLASS, &object_class);
    AddField(touch, "phase", phase, offsetof(TouchObject, phase));
    TouchObject data{{&touch.klass}};
    auto*       touch_receiver = reinterpret_cast<TKTouch*>(&data);
    int         calls          = 0;
    const auto  populate       = [&](TKTouch* actual, uintptr_t position, unity::TouchPhase actual_phase) {
      Require(actual == touch_receiver && position == 0x12345 && actual_phase == unity::TouchPhase::Began,
              "touch population forwards the original receiver and arguments");
      ++calls;
      return touch_receiver;
    };
    Require(TKTouch_populateWithPosition_Hook(populate, touch_receiver, 0x12345, unity::TouchPhase::Began)
                    == touch_receiver
                && calls == 1 && data.phase == unity::TouchPhase::Moved,
            "a populated stationary touch becomes Moved without replacing the native result");
    data.phase = unity::TouchPhase::Ended;
    TKTouch_populateWithPosition_Hook(populate, touch_receiver, 0x12345, unity::TouchPhase::Began);
    Require(data.phase == unity::TouchPhase::Ended, "non-stationary touch phases remain native");
    Require(!TKTouch_populateWithPosition_Hook([](auto*, uintptr_t, unity::TouchPhase) -> TKTouch* { return nullptr; },
                                               touch_receiver, 1, unity::TouchPhase::Began),
            "failed native touch population remains null");
  }
  tow_static_field_read_override = nullptr;
  bindings_config_override       = nullptr;
  bindings_replacement_config.reset();
  replacement_config   = nullptr;
  selected             = previous_selection;
  fleet_native_capture = false;
  fleet_native_keys.clear();
  Require(warp_roots.size() == roots, "pan frame releases every temporary managed root");
  std::cout << "Pan production callback fixture passed: " << mode << '\n';
}
} // namespace camera_pan_fixture

namespace
{
void CameraPanCases(std::string_view mode)
{ camera_pan_fixture::Run(mode); }
} // namespace
