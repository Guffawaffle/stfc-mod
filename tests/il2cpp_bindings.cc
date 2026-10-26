// Typed bindings against a controlled managed runtime: lookup, signature checks, native calls, fields, dispatch,
// failure reporting, hook sets and strings, plus the Unity keyboard and focus snapshots built on them.

#include "il2cpp_test_api.h"

// The test API must suppress imported declarations before any binding header.
#include "game/chat.h"
#include "game/fleet.h"
#include "game/galaxy.h"
#include "game/navigation.h"
#include "game/settings.h"
#include "settings/native_boolean_callback.h"
#include <il2cpp/hook.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
void Require(bool condition, const char* what)
{
  if (!condition)
    throw std::runtime_error(what);
}

int  input_frame = 0, held_reads = 0, down_reads = 0, frame_reads = 0;
bool any_held = false, any_down = false;
std::array<bool, std::to_underlying(unity::KeyCode::Max)> held_keys{}, down_keys{};

void KeyboardSnapshot()
{
  using namespace unity;
  Key::Poll();
  Require(!Key::Pressed(KeyCode::A) && !Key::Down(KeyCode::A), "idle keyboard reads false");
  Require(held_reads == 0 && down_reads == 0, "idle keyboard avoids key queries");

  ++input_frame;
  any_held = any_down                       = true;
  held_keys[std::to_underlying(KeyCode::A)] = true;
  down_keys[std::to_underlying(KeyCode::A)] = true;
  Key::Poll();
  for (int i = 0; i < 100; ++i) {
    Key::Poll();
    Require(Key::Pressed(KeyCode::A) && Key::Down(KeyCode::A), "held and down snapshot");
    Require(!Key::Pressed(KeyCode::B) && !Key::Down(KeyCode::B), "negative key queries are cached too");
  }
  Require(held_reads == 2 && down_reads == 2, "each key is queried once per state per frame");
  ++input_frame;
  held_keys[std::to_underlying(KeyCode::A)] = false;
  down_keys[std::to_underlying(KeyCode::A)] = false;
  Key::Poll();
  Require(!Key::Pressed(KeyCode::A) && !Key::Down(KeyCode::A), "new frames refresh the snapshot");
  Require(held_reads == 3 && down_reads == 3, "new frame queries keys again");
  ++input_frame;
  any_held = any_down = false;
  Key::Poll();
  Require(!Key::Pressed(KeyCode::A) && !Key::Down(KeyCode::A), "return to idle clears held keys");
  Require(held_reads == 3 && down_reads == 3, "idle frame stays query-free");
}

struct FakeClass {
  // MSVC cannot embed Il2CppClass because its vtable is a zero-length trailing array.
  std::unique_ptr<Il2CppClass>   metadata = std::make_unique<Il2CppClass>();
  Il2CppClass&                   klass    = *metadata;
  Il2CppType                     byref{};
  std::vector<const MethodInfo*> methods;
  std::vector<FieldInfo>         fields;
  std::vector<PropertyInfo>      properties;
  std::vector<Il2CppClass*>      nested;
  int32_t                        value_size      = 0;
  uint32_t                       value_alignment = 4;
  bool                           is_enum         = false;
};

std::deque<FakeClass>                              classes;
std::deque<MethodInfo>                             methods;
std::deque<std::vector<const Il2CppType*>>         parameter_lists;
std::deque<Il2CppType>                             field_types;
std::unordered_map<const Il2CppClass*, FakeClass*> registry;
std::vector<std::string>                           errors;
std::vector<const Il2CppClass*>                    initialized;
// Method table walks started, one per class visited.
int method_lookups = 0;

FakeClass& Define(const char* namespaze, const char* name, Il2CppTypeEnum type, FakeClass* parent = nullptr,
                  int32_t value_size = 0)
{
  auto& fake                      = classes.emplace_back();
  fake.klass.name                 = name;
  fake.klass.namespaze            = namespaze;
  fake.klass.parent               = parent ? &parent->klass : nullptr;
  fake.klass.byval_arg.type       = type;
  fake.klass.byval_arg.data.dummy = &fake.klass;
  fake.klass.byval_arg.valuetype  = value_size > 0;
  fake.byref                      = fake.klass.byval_arg;
  fake.byref.byref                = 1;
  fake.value_size                 = value_size;
  registry[&fake.klass]           = &fake;
  return fake;
}

const Il2CppType* T(FakeClass& fake)
{ return &fake.klass.byval_arg; }
const Il2CppType* Ref(FakeClass& fake)
{ return &fake.byref; }

MethodInfo& AddMethod(FakeClass& owner, const char* name, void* native, const Il2CppType* result,
                      std::vector<const Il2CppType*> params, uint16_t flags = METHOD_ATTRIBUTE_PUBLIC)
{
  auto& list                  = parameter_lists.emplace_back(std::move(params));
  auto& method                = methods.emplace_back();
  method.name                 = name;
  method.methodPointer        = reinterpret_cast<Il2CppMethodPointer>(native);
  method.virtualMethodPointer = method.methodPointer;
  method.klass                = &owner.klass;
  method.return_type          = result;
  method.parameters           = list.data();
  method.parameters_count     = static_cast<uint8_t>(list.size());
  method.flags                = flags;
  owner.methods.push_back(&method);
  owner.klass.methods      = owner.methods.data();
  owner.klass.method_count = static_cast<uint16_t>(owner.methods.size());
  return method;
}

void AddField(FakeClass& owner, const char* name, FakeClass& type, int32_t offset, bool is_static = false)
{
  auto& field_type = field_types.emplace_back(type.klass.byval_arg);
  if (is_static)
    field_type.attrs |= FIELD_ATTRIBUTE_STATIC;
  owner.fields.push_back({name, &field_type, &owner.klass, offset, 0});
}

void AddProperty(FakeClass& owner, const char* name, const MethodInfo* get, const MethodInfo* set = nullptr)
{ owner.properties.push_back({&owner.klass, name, get, set, 0, 0}); }

FakeClass& Fake(const Il2CppClass* klass)
{ return *registry.at(klass); }

// Managed object storage for tests. Offsets mirror a real object: header, then fields.
struct TestObject {
  Il2CppObject  header;
  int32_t       count;
  Il2CppObject* target;
};

struct Vec3 {
  float x, y, z;
};

// Runtime classes.
FakeClass& void_class               = Define("System", "Void", IL2CPP_TYPE_VOID);
FakeClass& object_class             = Define("System", "Object", IL2CPP_TYPE_OBJECT);
FakeClass& int_class                = Define("System", "Int32", IL2CPP_TYPE_I4, nullptr, 4);
FakeClass& unsigned_int_class       = Define("System", "UInt32", IL2CPP_TYPE_U4, nullptr, 4);
FakeClass& unsigned_long_class      = Define("System", "UInt64", IL2CPP_TYPE_U8, nullptr, 8);
FakeClass& bool_class               = Define("System", "Boolean", IL2CPP_TYPE_BOOLEAN, nullptr, 1);
FakeClass& float_class              = Define("System", "Single", IL2CPP_TYPE_R4, nullptr, 4);
FakeClass& string_class             = Define("System", "String", IL2CPP_TYPE_STRING, &object_class);
FakeClass& type_class               = Define("System", "Type", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& array_class              = Define("System", "Array", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& object_array_class       = Define("System", "Object[]", IL2CPP_TYPE_SZARRAY, &array_class);
FakeClass& string_array_class       = Define("System", "String[]", IL2CPP_TYPE_SZARRAY, &array_class);
FakeClass& vec3_class               = Define("UnityEngine", "Vector3", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(Vec3));
FakeClass& state_class              = Define("Game", "State", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
FakeClass& base_class               = Define("Game", "Base", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& derived_class            = Define("Game", "Derived", IL2CPP_TYPE_CLASS, &base_class);
FakeClass& other_class              = Define("Game", "Other", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& outer_class              = Define("Game", "Outer", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& inner_class              = Define("", "Inner", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& list_a_class             = Define("Game", "ListA", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& list_b_class             = Define("Game", "ListB", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& delegate_base            = Define("System", "Delegate", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& action_class             = Define("System", "Action", IL2CPP_TYPE_CLASS, &delegate_base);
FakeClass& settings_singleton_base  = Define("Game", "MonoSingleton`1", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& settings_singleton_class = Define("Game", "SettingsSingleton", IL2CPP_TYPE_CLASS, &settings_singleton_base);
FakeClass& intptr_class             = Define("System", "IntPtr", IL2CPP_TYPE_I, nullptr, sizeof(void*));
FakeClass& uintptr_class            = Define("System", "UIntPtr", IL2CPP_TYPE_U, nullptr, sizeof(void*));
FakeClass& pointer_class            = Define("Game", "UnmanagedPointer", IL2CPP_TYPE_PTR);
FakeClass& nullable_definition      = Define("System", "Nullable`1", IL2CPP_TYPE_VALUETYPE, nullptr, 16);
FakeClass& foreign_nullable_definition = Define("System", "Nullable`1", IL2CPP_TYPE_VALUETYPE, nullptr, 16);
FakeClass& nullable_vector_class       = Define("System", "Nullable`1", IL2CPP_TYPE_GENERICINST, nullptr, 16);
FakeClass& foreign_vector_class        = Define("Other", "Vector3", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(Vec3));
FakeClass& warp_navigation_class =
    Define("Digit.Prime.Navigation", "NavigationManager", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_animator_class =
    Define("Digit.Prime.Navigation", "NavigationTransitionAnimator", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_helper_class =
    Define("Digit.Prime.Navigation", "FleetNavigationHelper", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_context_class =
    Define("Digit.Prime.Navigation", "NavigationInteractionUIContext", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_locale_class     = Define("Digit.Client.UI", "LocaleTextContext", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_deployment_class = Define("", "DeploymentManager", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_course_class     = Define("Digit.PrimeServer.Models", "CourseData", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_node_class       = Define("Digit.PrimeServer.Models", "CourseNode", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& warp_result_enum = Define("Digit.PrimeServer.Services", "PathFindResult", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
std::array<const Il2CppType*, 1> nullable_arguments{};
Il2CppGenericInst                nullable_inst{};
Il2CppGenericClass               nullable_generic{};
Il2CppObject warp_navigation{}, warp_animator{}, warp_helper{}, warp_locale{}, warp_deployment{}, warp_course{},
    warp_node{};
struct WarpContextObject {
  Il2CppObject   header;
  unity::Vector3 position;
} warp_context{};
struct WarpVectorBox {
  Il2CppObject   header;
  unity::Vector3 value;
} warp_vector_box{};
const MethodInfo* warp_move_method            = nullptr;
const MethodInfo* warp_world_method           = nullptr;
const MethodInfo* warp_system_method          = nullptr;
const MethodInfo* warp_galaxy_method          = nullptr;
bool              nullable_definition_missing = false, vector_class_missing = false;
bool    warp_validator_value = true, warp_move_failure = false, warp_vector_failure = false, warp_vector_null = false;
bool    warp_vector_wrong_type = false, warp_root_failure = false;
int     warp_move_calls = 0, warp_vector_calls = 0, warp_validator_calls = 0;
int64_t warp_seen_fleet = 0, warp_seen_node = 0;
unity::Vector3                                    warp_seen_position{};
std::unordered_map<Il2CppGCHandle, Il2CppObject*> warp_roots;
uintptr_t                                         warp_next_root = 1;
FakeClass&                                        long_class = Define("System", "Int64", IL2CPP_TYPE_I8, nullptr, 8);
FakeClass& ship_model_class    = Define("Digit.PrimeServer.Models", "IShipModel", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& base_ship_class     = Define("Digit.PrimeServer.Models", "BaseShip", IL2CPP_TYPE_CLASS, &ship_model_class);
FakeClass& concrete_ship_class = Define("Digit.PrimeServer.Models", "Ship", IL2CPP_TYPE_CLASS, &base_ship_class);
FakeClass& other_ship_class =
    Define("Digit.PrimeServer.Models", "OtherShipModel", IL2CPP_TYPE_CLASS, &ship_model_class);
FakeClass& fleet_data_class = Define("Digit.PrimeServer.Models", "FleetPlayerData", IL2CPP_TYPE_CLASS, &object_class);
struct ShipIdentityObject {
  Il2CppObject header;
  int64_t      id;
} identity_ship{};
struct FleetIdentityObject {
  Il2CppObject  header;
  Il2CppObject* ship;
} identity_fleet{};
bool ship_read_throws = false, ship_id_throws = false;
int  fleet_identity_reads = 0;

FakeClass& event_system_class    = Define("UnityEngine.EventSystems", "EventSystem", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& unity_object_class    = Define("UnityEngine", "Object", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& game_object_class     = Define("UnityEngine", "GameObject", IL2CPP_TYPE_CLASS, &unity_object_class);
FakeClass& component_class       = Define("UnityEngine", "Component", IL2CPP_TYPE_CLASS, &unity_object_class);
FakeClass& renderer_class        = Define("UnityEngine", "Renderer", IL2CPP_TYPE_CLASS, &component_class);
FakeClass& material_class        = Define("UnityEngine", "Material", IL2CPP_TYPE_CLASS, &unity_object_class);
FakeClass& shader_class          = Define("UnityEngine", "Shader", IL2CPP_TYPE_CLASS, &unity_object_class);
FakeClass& component_array_class = Define("UnityEngine", "Component[]", IL2CPP_TYPE_SZARRAY, &array_class);
FakeClass& material_array_class  = Define("UnityEngine", "Material[]", IL2CPP_TYPE_SZARRAY, &array_class);
FakeClass& input_field_class     = Define("TMPro", "TMP_InputField", IL2CPP_TYPE_CLASS, &object_class);
FakeClass& chat_avatar_class =
    Define("Digit.Prime.PlayerAvatars", "FrameAndAvatarWidget", IL2CPP_TYPE_CLASS, &component_class);
FakeClass& chat_profile_class =
    Define("Digit.Prime.PlayerProfile", "UserProfileWidget", IL2CPP_TYPE_CLASS, &component_class);
FakeClass& chat_message_class = Define("Digit.Prime.Chat", "ChatMessageWidget", IL2CPP_TYPE_CLASS, &component_class);

struct ChatAvatarObject {
  Il2CppObject                 header;
  bool                         keep_in_cache;
  std::array<unsigned char, 7> adjacent;
} chat_profile_avatar{}, chat_admin_avatar{};
struct ChatProfileObject {
  Il2CppObject  header;
  Il2CppObject* avatar;
} chat_profile{};
struct ChatMessageObject {
  Il2CppObject  header;
  Il2CppObject* profile;
  Il2CppObject* admin;
} chat_message{};
std::vector<Il2CppObject*> chat_retention_writes;
int                        chat_set_data_calls  = 0;
const MethodInfo*          chat_set_data_method = nullptr;

Il2CppObject visual_renderer{}, visual_material{}, visual_shader{}, visual_components{}, visual_materials{};
bool         visual_alive = true, visual_active = true, visual_enabled = true, visual_throws = false;
bool         visual_property = true, visual_set_throws = false;
int32_t      visual_id    = -17;
float        visual_depth = 0.0f;
int          visual_calls = 0;
bool         VisualAlive(Il2CppObject*, const MethodInfo*)
{
  ++visual_calls;
  if (visual_throws)
    throw std::runtime_error("unavailable Unity observation");
  return visual_alive;
}
int32_t VisualId(Il2CppObject*, const MethodInfo*)
{
  if (visual_throws)
    throw std::runtime_error("unavailable Unity identity");
  return visual_id;
}
Il2CppString* VisualName(Il2CppObject*, const MethodInfo*);
bool          VisualActive(Il2CppObject*, const MethodInfo*)
{ return visual_active; }
Il2CppObject* VisualComponents(Il2CppObject*, Il2CppReflectionType*, bool, const MethodInfo*)
{ return &visual_components; }
Il2CppObject* VisualMaterials(Il2CppObject*, const MethodInfo*)
{ return &visual_materials; }
Il2CppObject* VisualShader(Il2CppObject*, const MethodInfo*)
{ return &visual_shader; }
bool VisualEnabled(Il2CppObject*, const MethodInfo*)
{ return visual_enabled; }
void VisualSetEnabled(Il2CppObject*, bool value, const MethodInfo*)
{
  visual_enabled = value;
  if (visual_set_throws)
    throw std::runtime_error("setter mutated before failing");
}
bool VisualHasProperty(Il2CppObject*, Il2CppString*, const MethodInfo*)
{ return visual_property; }
float VisualGetDepth(Il2CppObject*, Il2CppString*, const MethodInfo*)
{ return visual_depth; }
void VisualSetDepth(Il2CppObject*, Il2CppString*, float value, const MethodInfo*)
{ visual_depth = value; }

unity::EventSystem    event_system{};
unity::GameObject     selected_object{};
unity::TMP_InputField input_field{};
unity::GameObject*    selected         = &selected_object;
bool                  has_event_system = true, has_input_field = true, redirect_selection = false;
int                   event_system_reads = 0;

unity::EventSystem* EventSystem_get_current(const MethodInfo*)
{
  ++event_system_reads;
  return has_event_system ? &event_system : nullptr;
}
unity::GameObject* EventSystem_get_selected(unity::EventSystem*, const MethodInfo*)
{ return selected; }
void EventSystem_set_selected(unity::EventSystem*, unity::GameObject* value, const MethodInfo*)
{ selected = redirect_selection ? &selected_object : value; }
unity::TMP_InputField* GameObject_get_component(unity::GameObject*, Il2CppObject*, const MethodInfo*)
{ return has_input_field ? &input_field : nullptr; }
bool InputField_get_focused(unity::TMP_InputField*, const MethodInfo*)
{ return true; }

void InputFocus()
{
  using namespace unity;
  ++input_frame;
  Key::Poll();
  Require(Key::IsInputFocused(), "selected text field has focus");
  const int reads = event_system_reads;
  Require(Key::IsInputFocused() && event_system_reads == reads, "focus is cached within a frame");
  Key::ClearInputFocus();
  Require(event_system_reads == reads + 1, "clearing focus reads the event system once");
  Require(selected == nullptr && !Key::IsInputFocused(), "clearing selection invalidates focus within the frame");

  selected           = &selected_object;
  redirect_selection = true;
  Key::ClearInputFocus();
  Require(Key::IsInputFocused(), "selection callbacks can focus another field when focus is cleared");
  redirect_selection = false;

  has_input_field = false;
  ++input_frame;
  Key::Poll();
  Require(!Key::IsInputFocused(), "a selected object without a text field is not text focus");
  Key::ClearInputFocus();
  Require(selected == &selected_object, "clearing text focus preserves a selected non-text object");

  has_event_system = false;
  ++input_frame;
  Key::Poll();
  Require(!Key::IsInputFocused(), "no event system has no text focus");
  Key::ClearInputFocus();
}

TestObject    base_object{}, derived_object{}, other_object{}, list_a{}, list_b{}, allocated{};
Il2CppObject  string_object{}, type_object{};
Il2CppString* VisualName(Il2CppObject*, const MethodInfo*)
{ return reinterpret_cast<Il2CppString*>(&string_object); }
Il2CppDelegate delegate_object{};
bool           settings_singleton_initialized = false;
Il2CppObject*  settings_singleton_instance    = nullptr;

// Generated code stand-ins. Each records what the binding passed.
const Il2CppObject* seen_self   = nullptr;
const MethodInfo*   seen_method = nullptr;
int32_t             seen_int    = 0;
bool                seen_bool   = false;
std::string         seen_overload;
int                 native_calls = 0;
int32_t             static_flag  = 0;

int32_t Base_get_Count(Il2CppObject* self, const MethodInfo* method)
{
  seen_self   = self;
  seen_method = method;
  ++native_calls;
  return reinterpret_cast<TestObject*>(self)->count;
}
void Base_set_Count(Il2CppObject* self, int32_t value, const MethodInfo*)
{ reinterpret_cast<TestObject*>(self)->count = value; }
void Base_Select_Int(Il2CppObject*, int32_t value, const MethodInfo*)
{
  seen_overload = "int";
  seen_int      = value;
}
void Base_Select_Bool(Il2CppObject*, bool value, const MethodInfo*)
{
  seen_overload = "bool";
  seen_bool     = value;
}
bool Base_Find_String(Il2CppObject*, Il2CppString*, const MethodInfo*)
{
  seen_overload = "string";
  return true;
}
bool Base_Find_Type(Il2CppObject*, Il2CppReflectionType*, const MethodInfo*)
{
  seen_overload = "type";
  return true;
}
bool Base_TryGet(Il2CppObject*, int32_t key, Il2CppObject** result, const MethodInfo*)
{
  *result = key == 1 ? &other_object.header : nullptr;
  return *result != nullptr;
}
Vec3 Base_get_Position(Il2CppObject*, const MethodInfo*)
{ return {1.f, 2.f, 3.f}; }
int32_t Base_get_State(Il2CppObject*, const MethodInfo*)
{ return 2; }
const char* Base_Describe(Il2CppObject*, const MethodInfo*)
{ return "base"; }
const char* Derived_Describe(Il2CppObject*, const MethodInfo*)
{ return "derived"; }
int32_t Base_Twice(int32_t value, const MethodInfo*)
{ return value * 2; }
Il2CppObject* Base_get_Instance(const MethodInfo*)
{ return &base_object.header; }
void Base_ctor(Il2CppObject* self, int32_t count, const MethodInfo*)
{ reinterpret_cast<TestObject*>(self)->count = count; }
int32_t ListA_get_Count(Il2CppObject*, const MethodInfo*)
{ return 2; }
int32_t ListB_get_Count(Il2CppObject*, const MethodInfo*)
{ return 3; }
Il2CppObject* List_get_Item(Il2CppObject* self, int32_t index, const MethodInfo*)
{ return index == 0 ? self : &other_object.header; }
void List_set_Item(Il2CppObject* self, int32_t index, Il2CppObject* value, const MethodInfo* method)
{
  seen_self                                   = self;
  seen_int                                    = index;
  seen_method                                 = method;
  reinterpret_cast<TestObject*>(self)->target = value;
}
Il2CppException   thrown{};
int               warp_course_calls = 0, warp_scalar_calls = 0;
const MethodInfo* warp_validator_method = nullptr;
const MethodInfo* warp_course_method    = nullptr;
const MethodInfo* warp_action_method    = nullptr;
const MethodInfo* warp_reference_method = nullptr;
int               warp_reference_calls  = 0;
bool              WarpValidate(Il2CppObject*, int64_t fleet, LocaleTextContext** message, const MethodInfo*)
{
  ++warp_validator_calls;
  warp_seen_fleet = fleet;
  *message        = reinterpret_cast<LocaleTextContext*>(&warp_locale);
  return warp_validator_value;
}
Il2CppObject* WarpGetCourse(Il2CppObject*, int64_t fleet, const MethodInfo*)
{
  ++warp_course_calls;
  warp_seen_fleet = fleet;
  return &warp_course;
}
int32_t WarpTargetAction(Il2CppObject*, const MethodInfo*)
{
  ++warp_scalar_calls;
  return -1;
}
int64_t WarpFleetId(Il2CppObject*, const MethodInfo*)
{ return 9007199254740993LL; }
Il2CppObject* WarpGetTarget(Il2CppObject*, const MethodInfo*)
{
  ++warp_reference_calls;
  return &warp_node;
}
Il2CppObject* WarpGetInstance(const MethodInfo*)
{ return &warp_deployment; }
Il2CppObject* FleetIdentity_get_Ship(Il2CppObject* self, const MethodInfo*)
{
  if (ship_read_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<FleetIdentityObject*>(self)->ship;
}
int64_t ShipIdentity_get_Id(Il2CppObject* self, const MethodInfo*)
{
  if (ship_id_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<ShipIdentityObject*>(self)->id;
}
int64_t FleetIdentity_get_Id(Il2CppObject*, const MethodInfo*)
{
  ++fleet_identity_reads;
  return 999;
}
int32_t Base_Explode(Il2CppObject*, const MethodInfo*)
{ throw Il2CppExceptionWrapper(&thrown); }
void Base_ExplodeVoid(Il2CppObject*, const MethodInfo*)
{ throw Il2CppExceptionWrapper(&thrown); }
void Never(Il2CppObject*, const MethodInfo*)
{ throw std::runtime_error("shared generic method was called natively"); }

const MethodInfo* delegate_target      = nullptr;
bool              delegate_ctor_throws = false;
void              DelegateInvoker(Il2CppMethodPointer, const MethodInfo*, void* self, void** args, void*)
{
  if (delegate_ctor_throws)
    throw Il2CppExceptionWrapper(&thrown);
  // Delegate(object target, IntPtr method): the second argument points at the MethodInfo address.
  auto* delegate             = static_cast<Il2CppDelegate*>(self);
  delegate_target            = *static_cast<const MethodInfo**>(args[1]);
  delegate->method           = delegate_target;
  delegate->target           = static_cast<Il2CppObject*>(args[0]);
  delegate->invoke_impl_this = delegate->target;
}
void     NativeTarget() {}
unsigned closed_delegate_calls = 0;
unsigned closed_bool_get_calls = 0, closed_bool_set_calls = 0, closed_query_calls = 0;
bool     ClosedBoolGetter(Il2CppObject* self, const MethodInfo*)
{
  Require(self == &base_object.header, "closed bool getter receiver");
  ++closed_bool_get_calls;
  return true;
}
void ClosedBoolSetter(Il2CppObject* self, bool value, const MethodInfo*)
{
  Require(self == &base_object.header && value, "closed bool setter receiver and value");
  ++closed_bool_set_calls;
}
int ClosedQuery(Il2CppObject* self, const MethodInfo*)
{
  Require(self == &base_object.header, "closed enum query receiver");
  ++closed_query_calls;
  return 2;
}
void ClosedTarget(Il2CppObject* self, const MethodInfo*)
{
  Require(self == &base_object.header, "closed callback retains its entered receiver");
  ++closed_delegate_calls;
}
void ClosedInvoker(Il2CppMethodPointer pointer, const MethodInfo* method, void* self, void**, void*)
{ reinterpret_cast<void (*)(Il2CppObject*, const MethodInfo*)>(pointer)(static_cast<Il2CppObject*>(self), method); }
void WarpInvokerSentinel(Il2CppMethodPointer, const MethodInfo*, void*, void**, void*)
{ Require(false, "the fake runtime must own invocation instead of calling a metadata sentinel directly"); }

MethodInfo*   base_describe     = nullptr;
MethodInfo*   shared_method     = nullptr;
int32_t       shared_result     = 41;
int           boxed_invocations = 0;
bool          boxed_failure = false, boxed_null = false;
bool          array_class_missing  = false;
bool          string_class_missing = false;
int           array_getter_calls   = 0;
Il2CppObject* array_getter_value   = nullptr;
Il2CppObject* ArrayGetter(Il2CppObject*, const MethodInfo*)
{
  ++array_getter_calls;
  return array_getter_value;
}
void ChatSetWidgetData(Il2CppObject*, const MethodInfo*)
{ ++chat_set_data_calls; }

void BuildRuntime()
{
  chat_avatar_class.klass.instance_size  = sizeof(ChatAvatarObject);
  chat_profile_class.klass.instance_size = sizeof(ChatProfileObject);
  chat_message_class.klass.instance_size = sizeof(ChatMessageObject);
  chat_profile_avatar.header.klass       = &chat_avatar_class.klass;
  chat_admin_avatar.header.klass         = &chat_avatar_class.klass;
  chat_profile_avatar.adjacent.fill(0xa5);
  chat_admin_avatar.adjacent.fill(0x5a);
  chat_profile.header.klass = &chat_profile_class.klass;
  chat_profile.avatar       = &chat_profile_avatar.header;
  chat_message.header.klass = &chat_message_class.klass;
  chat_message.profile      = &chat_profile.header;
  chat_message.admin        = &chat_admin_avatar.header;
  AddField(chat_avatar_class, "_keepInCache", bool_class, offsetof(ChatAvatarObject, keep_in_cache));
  AddField(chat_profile_class, "_profileFrameAndAvatar", chat_avatar_class, offsetof(ChatProfileObject, avatar));
  AddField(chat_message_class, "_userProfileWidget", chat_profile_class, offsetof(ChatMessageObject, profile));
  AddField(chat_message_class, "_adminAvatarWidget", chat_avatar_class, offsetof(ChatMessageObject, admin));
  chat_set_data_method =
      &AddMethod(chat_message_class, "SetWidgetData", reinterpret_cast<void*>(&ChatSetWidgetData), T(void_class), {});
  nullable_arguments[0]                                    = T(vec3_class);
  nullable_inst.type_argc                                  = 1;
  nullable_inst.type_argv                                  = nullable_arguments.data();
  nullable_generic.type                                    = T(nullable_definition);
  nullable_generic.context.class_inst                      = &nullable_inst;
  nullable_vector_class.klass.byval_arg.data.generic_class = &nullable_generic;
  nullable_vector_class.klass.generic_class                = &nullable_generic;
  warp_result_enum.is_enum                                 = true;
  AddField(warp_result_enum, "value__", int_class, sizeof(Il2CppObject));
  AddField(nullable_vector_class, "hasValue", bool_class, sizeof(Il2CppObject));
  AddField(nullable_vector_class, "value", vec3_class, sizeof(Il2CppObject) + offsetof(il2cpp::Nullable<Vec3>, value));
  warp_navigation.klass        = &warp_navigation_class.klass;
  warp_animator.klass          = &warp_animator_class.klass;
  warp_helper.klass            = &warp_helper_class.klass;
  warp_locale.klass            = &warp_locale_class.klass;
  warp_deployment.klass        = &warp_deployment_class.klass;
  warp_course.klass            = &warp_course_class.klass;
  warp_node.klass              = &warp_node_class.klass;
  warp_context.header.klass    = &warp_context_class.klass;
  warp_context.position        = {};
  warp_vector_box.header.klass = &vec3_class.klass;
  warp_move_method = &AddMethod(warp_navigation_class, "OnMoveFleetAction", reinterpret_cast<void*>(&Never),
                                T(void_class), {T(nullable_vector_class), T(long_class), T(long_class), T(bool_class)});
  warp_world_method = &AddMethod(warp_animator_class, "WorldToDepth", reinterpret_cast<void*>(&Never), T(vec3_class),
                                 {T(vec3_class), T(int_class)});
  warp_system_method =
      &AddMethod(warp_node_class, "get_SystemPosition", reinterpret_cast<void*>(&Never), T(vec3_class), {});
  warp_galaxy_method =
      &AddMethod(warp_node_class, "get_GalaxyPosition", reinterpret_cast<void*>(&Never), T(vec3_class), {});
  for (const auto* method : {warp_move_method, warp_world_method, warp_system_method, warp_galaxy_method})
    const_cast<MethodInfo*>(method)->invoker_method = &WarpInvokerSentinel;
  warp_validator_method =
      &AddMethod(warp_helper_class, "NavigationValidateInput", reinterpret_cast<void*>(&WarpValidate), T(bool_class),
                 {T(long_class), Ref(warp_locale_class)});
  warp_course_method = &AddMethod(warp_deployment_class, "GetPlannedCourse", reinterpret_cast<void*>(&WarpGetCourse),
                                  T(warp_course_class), {T(long_class)});
  warp_action_method = &AddMethod(warp_course_class, "get_TargetAction", reinterpret_cast<void*>(&WarpTargetAction),
                                  T(warp_result_enum), {});
  AddMethod(warp_course_class, "get_FleetID", reinterpret_cast<void*>(&WarpFleetId), T(long_class), {});
  warp_reference_method =
      &AddMethod(warp_course_class, "get_TargetNode", reinterpret_cast<void*>(&WarpGetTarget), T(warp_node_class), {});
  AddMethod(warp_deployment_class, "get_Instance", reinterpret_cast<void*>(&WarpGetInstance), T(warp_deployment_class),
            {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(base_class, "get_CanonicalSigned", reinterpret_cast<void*>(&WarpTargetAction), T(int_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  AddMethod(derived_class, "get_CanonicalSigned", reinterpret_cast<void*>(&WarpTargetAction), T(unsigned_int_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  AddField(warp_context_class, "Position", vec3_class, offsetof(WarpContextObject, position));
  component_array_class.klass.byval_arg.data.type = T(component_class);
  material_array_class.klass.byval_arg.data.type  = T(material_class);
  visual_renderer.klass                           = &renderer_class.klass;
  visual_material.klass                           = &material_class.klass;
  visual_shader.klass                             = &shader_class.klass;
  visual_components.klass                         = &component_array_class.klass;
  visual_materials.klass                          = &material_array_class.klass;
  AddMethod(unity_object_class, "op_Implicit", reinterpret_cast<void*>(&VisualAlive), T(bool_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(unity_object_class, "GetInstanceID", reinterpret_cast<void*>(&VisualId), T(int_class), {});
  AddMethod(unity_object_class, "get_name", reinterpret_cast<void*>(&VisualName), T(string_class), {});
  AddMethod(game_object_class, "get_activeInHierarchy", reinterpret_cast<void*>(&VisualActive), T(bool_class), {});
  AddMethod(game_object_class, "GetComponentsInChildren", reinterpret_cast<void*>(&VisualComponents),
            T(component_array_class), {T(type_class), T(bool_class)});
  AddMethod(renderer_class, "get_sharedMaterials", reinterpret_cast<void*>(&VisualMaterials), T(material_array_class),
            {});
  AddMethod(renderer_class, "get_enabled", reinterpret_cast<void*>(&VisualEnabled), T(bool_class), {});
  AddMethod(renderer_class, "set_enabled", reinterpret_cast<void*>(&VisualSetEnabled), T(void_class), {T(bool_class)});
  AddMethod(material_class, "get_shader", reinterpret_cast<void*>(&VisualShader), T(shader_class), {});
  AddMethod(material_class, "HasProperty", reinterpret_cast<void*>(&VisualHasProperty), T(bool_class),
            {T(string_class)});
  AddMethod(material_class, "GetFloat", reinterpret_cast<void*>(&VisualGetDepth), T(float_class), {T(string_class)});
  AddMethod(material_class, "SetFloat", reinterpret_cast<void*>(&VisualSetDepth), T(void_class),
            {T(string_class), T(float_class)});
  object_array_class.klass.byval_arg.data.type = T(object_class);
  string_array_class.klass.byval_arg.data.type = T(string_class);
  AddMethod(base_class, "get_ReferenceParameters", reinterpret_cast<void*>(&ArrayGetter), T(object_array_class), {});
  AddMethod(base_class, "get_StringParameters", reinterpret_cast<void*>(&ArrayGetter), T(string_array_class), {});
  AddMethod(base_class, "get_PointerParameters", reinterpret_cast<void*>(&ArrayGetter), T(pointer_class), {});
  AddMethod(base_class, "get_IntPtrParameters", reinterpret_cast<void*>(&ArrayGetter), T(intptr_class), {});
  AddMethod(base_class, "get_UIntPtrParameters", reinterpret_cast<void*>(&ArrayGetter), T(uintptr_class), {});
  AddMethod(base_class, "get_ValueParameters", reinterpret_cast<void*>(&ArrayGetter), T(int_class), {});
  const auto& ship_getter = AddMethod(fleet_data_class, "get_Ship", reinterpret_cast<void*>(&FleetIdentity_get_Ship),
                                      T(ship_model_class), {});
  AddProperty(fleet_data_class, "Ship", &ship_getter);
  const auto& ship_id =
      AddMethod(base_ship_class, "get_Id", reinterpret_cast<void*>(&ShipIdentity_get_Id), T(long_class), {});
  AddProperty(base_ship_class, "Id", &ship_id);
  const auto& fleet_id =
      AddMethod(fleet_data_class, "get_Id", reinterpret_cast<void*>(&FleetIdentity_get_Id), T(long_class), {});
  AddProperty(fleet_data_class, "Id", &fleet_id);
  identity_ship.header.klass  = &concrete_ship_class.klass;
  identity_ship.id            = 9007199254740993LL;
  identity_fleet.header.klass = &fleet_data_class.klass;
  identity_fleet.ship         = &identity_ship.header;
  auto& get_count = AddMethod(base_class, "get_Count", reinterpret_cast<void*>(&Base_get_Count), T(int_class), {});
  auto& set_count =
      AddMethod(base_class, "set_Count", reinterpret_cast<void*>(&Base_set_Count), T(void_class), {T(int_class)});
  AddProperty(base_class, "Count", &get_count, &set_count);
  AddMethod(base_class, "Select", reinterpret_cast<void*>(&Base_Select_Int), T(void_class), {T(int_class)});
  AddMethod(base_class, "Select", reinterpret_cast<void*>(&Base_Select_Bool), T(void_class), {T(bool_class)});
  AddMethod(base_class, "Find", reinterpret_cast<void*>(&Base_Find_String), T(bool_class), {T(string_class)});
  AddMethod(base_class, "Find", reinterpret_cast<void*>(&Base_Find_Type), T(bool_class), {T(type_class)});
  AddMethod(base_class, "TryGet", reinterpret_cast<void*>(&Base_TryGet), T(bool_class),
            {T(int_class), Ref(object_class)});
  auto& get_position =
      AddMethod(base_class, "get_Position", reinterpret_cast<void*>(&Base_get_Position), T(vec3_class), {});
  AddProperty(base_class, "Position", &get_position);
  auto& get_state = AddMethod(base_class, "get_State", reinterpret_cast<void*>(&Base_get_State), T(state_class), {});
  AddProperty(base_class, "State", &get_state);
  base_describe = &AddMethod(base_class, "Describe", reinterpret_cast<void*>(&Base_Describe), T(object_class), {},
                             METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  AddMethod(derived_class, "Describe", reinterpret_cast<void*>(&Derived_Describe), T(object_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  AddMethod(base_class, "Twice", reinterpret_cast<void*>(&Base_Twice), T(int_class), {T(int_class)},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto& get_instance = AddMethod(base_class, "get_Instance", reinterpret_cast<void*>(&Base_get_Instance), T(base_class),
                                 {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(base_class, "Instance", &get_instance);
  AddMethod(base_class, ".ctor", reinterpret_cast<void*>(&Base_ctor), T(void_class), {T(int_class)});
  AddMethod(base_class, "Explode", reinterpret_cast<void*>(&Base_Explode), T(int_class), {});
  AddMethod(base_class, "ExplodeVoid", reinterpret_cast<void*>(&Base_ExplodeVoid), T(void_class), {});
  shared_method = &AddMethod(base_class, "Shared", reinterpret_cast<void*>(&Never), T(int_class), {});
  shared_method->has_full_generic_sharing_signature = 1;
  AddField(base_class, "_count", int_class, offsetof(TestObject, count));
  AddField(base_class, "_target", base_class, offsetof(TestObject, target));
  AddField(base_class, "s_flag", int_class, 0, true);
  AddField(settings_singleton_base, "s_initialized", bool_class, 0, true);
  AddField(settings_singleton_base, "s_instance", settings_singleton_class, 0, true);

  AddMethod(list_a_class, "get_Count", reinterpret_cast<void*>(&ListA_get_Count), T(int_class), {});
  AddMethod(list_a_class, "get_Item", reinterpret_cast<void*>(&List_get_Item), T(object_class), {T(int_class)});
  AddMethod(list_a_class, "set_Item", reinterpret_cast<void*>(&List_set_Item), T(void_class),
            {T(int_class), T(object_class)});
  AddMethod(list_b_class, "get_Count", reinterpret_cast<void*>(&ListB_get_Count), T(int_class), {});
  AddMethod(list_b_class, "get_Item", reinterpret_cast<void*>(&List_get_Item), T(object_class), {T(int_class)});
  AddMethod(list_b_class, "set_Item", reinterpret_cast<void*>(&List_set_Item), T(void_class),
            {T(int_class), T(object_class)});

  auto& ctor          = AddMethod(action_class, ".ctor", nullptr, T(void_class), {T(object_class), T(intptr_class)});
  ctor.invoker_method = &DelegateInvoker;
  AddMethod(action_class, "Invoke", reinterpret_cast<void*>(&Never), T(void_class), {});
  AddMethod(base_class, "OnEvent", reinterpret_cast<void*>(&NativeTarget), T(void_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);

  auto& current = AddMethod(event_system_class, "get_current", reinterpret_cast<void*>(&EventSystem_get_current),
                            T(event_system_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(event_system_class, "current", &current);
  auto& selected_getter = AddMethod(event_system_class, "get_currentSelectedGameObject",
                                    reinterpret_cast<void*>(&EventSystem_get_selected), T(game_object_class), {});
  AddProperty(event_system_class, "currentSelectedGameObject", &selected_getter);
  AddMethod(event_system_class, "SetSelectedGameObject", reinterpret_cast<void*>(&EventSystem_set_selected),
            T(void_class), {T(game_object_class)});
  AddMethod(game_object_class, "GetComponent", reinterpret_cast<void*>(&GameObject_get_component), T(input_field_class),
            {T(type_class)});
  auto& focused = AddMethod(input_field_class, "get_isFocused", reinterpret_cast<void*>(&InputField_get_focused),
                            T(bool_class), {});
  AddProperty(input_field_class, "isFocused", &focused);
  event_system.klass    = &event_system_class.klass;
  selected_object.klass = &game_object_class.klass;
  input_field.klass     = &input_field_class.klass;

  inner_class.klass.declaringType = &outer_class.klass;
  outer_class.nested.push_back(&inner_class.klass);

  base_object.header.klass    = &base_class.klass;
  base_object.count           = 7;
  derived_object.header.klass = &derived_class.klass;
  other_object.header.klass   = &other_class.klass;
  list_a.header.klass         = &list_a_class.klass;
  list_b.header.klass         = &list_b_class.klass;
  string_object.klass         = &string_class.klass;
  type_object.klass           = &type_class.klass;
}
} // namespace

API(Il2CppDomain*, il2cpp_domain_get, ())
{ return reinterpret_cast<Il2CppDomain*>(1); }
END_API
API(const Il2CppAssembly*, il2cpp_domain_assembly_open, (Il2CppDomain*, const char*))
{ return reinterpret_cast<Il2CppAssembly*>(1); }
END_API
API(const Il2CppImage*, il2cpp_assembly_get_image, (const Il2CppAssembly*))
{ return reinterpret_cast<Il2CppImage*>(1); }
END_API
API(const Il2CppImage*, il2cpp_get_corlib, ())
{ return reinterpret_cast<Il2CppImage*>(1); }
END_API
API(Il2CppClass*, il2cpp_class_from_name, (const Il2CppImage*, const char* namespaze, const char* name))
{
  if (array_class_missing && std::string_view(namespaze) == "System" && std::string_view(name) == "Array")
    return nullptr;
  if (string_class_missing && std::string_view(namespaze) == "System" && std::string_view(name) == "String")
    return nullptr;
  if (nullable_definition_missing && std::string_view(namespaze) == "System" && std::string_view(name) == "Nullable`1")
    return nullptr;
  if (vector_class_missing && std::string_view(namespaze) == "UnityEngine" && std::string_view(name) == "Vector3")
    return nullptr;
  const auto fake = std::ranges::find_if(classes, [&](const FakeClass& fake) {
    return std::string_view(fake.klass.namespaze) == namespaze && std::string_view(fake.klass.name) == name;
  });
  return fake == classes.end() ? nullptr : &fake->klass;
}
END_API
API(const char*, il2cpp_class_get_name, (Il2CppClass * klass))
{ return klass->name; }
END_API
API(const char*, il2cpp_class_get_namespace, (Il2CppClass * klass))
{ return klass->namespaze; }
END_API
API(Il2CppClass*, il2cpp_class_get_declaring_type, (Il2CppClass * klass))
{ return klass->declaringType; }
END_API
API(Il2CppClass*, il2cpp_class_get_parent, (Il2CppClass * klass))
{ return klass->parent; }
END_API
API(Il2CppClass*, il2cpp_class_get_nested_types, (Il2CppClass * klass, void** iter))
{
  auto& nested = Fake(klass).nested;
  auto  index  = reinterpret_cast<size_t>(*iter);
  if (index >= nested.size())
    return nullptr;
  *iter = reinterpret_cast<void*>(index + 1);
  return nested[index];
}
END_API
API(const MethodInfo*, il2cpp_class_get_methods, (Il2CppClass * klass, void** iter))
{
  auto index = reinterpret_cast<size_t>(*iter);
  method_lookups += index == 0;
  if (index >= klass->method_count)
    return nullptr;
  *iter = reinterpret_cast<void*>(index + 1);
  return klass->methods[index];
}
END_API
API(const PropertyInfo*, il2cpp_class_get_property_from_name, (Il2CppClass * klass, const char* name))
{
  for (; klass; klass = klass->parent)
    for (auto& property : Fake(klass).properties)
      if (std::string_view(property.name) == name)
        return &property;
  return nullptr;
}
END_API
API(const MethodInfo*, il2cpp_property_get_get_method, (PropertyInfo * property))
{ return property->get; }
END_API
API(const MethodInfo*, il2cpp_property_get_set_method, (PropertyInfo * property))
{ return property->set; }
END_API
API(FieldInfo*, il2cpp_class_get_field_from_name, (Il2CppClass * klass, const char* name))
{
  for (; klass; klass = klass->parent)
    for (auto& field : Fake(klass).fields)
      if (std::string_view(field.name) == name)
        return &field;
  return nullptr;
}
END_API
API(Il2CppClass*, il2cpp_class_get_element_class, (Il2CppClass * klass))
{
  return klass && klass->byval_arg.type == IL2CPP_TYPE_SZARRAY ? il2cpp_class_from_type(klass->byval_arg.data.type)
                                                               : nullptr;
}
END_API
API(Il2CppClass*, il2cpp_class_from_type, (const Il2CppType* type))
{
  // A vector's type union points to its element type rather than its own class.
  // Preserve that actual SDK relationship in the controlled runtime too.
  for (auto& owner : classes)
    if (type == &owner.klass.byval_arg || type == &owner.byref)
      return &owner.klass;
  return static_cast<Il2CppClass*>(type->data.dummy);
}
END_API
API(const Il2CppType*, il2cpp_class_get_type, (Il2CppClass * klass))
{ return &klass->byval_arg; }
END_API
API(Il2CppMethodPointer, il2cpp_resolve_icall, (const char* name))
{
  static constexpr auto answer = +[]() -> int32_t { return 42; };
  static constexpr auto frame  = +[]() -> int32_t {
    ++frame_reads;
    return input_frame;
  };
  static constexpr auto held = +[]() { return any_held; };
  static constexpr auto down = +[]() { return any_down; };
  static constexpr auto key  = +[](unity::KeyCode key) {
    ++held_reads;
    return held_keys[std::to_underlying(key)];
  };
  static constexpr auto key_down = +[](unity::KeyCode key) {
    ++down_reads;
    return down_keys[std::to_underlying(key)];
  };
  const std::pair<std::string_view, Il2CppMethodPointer> calls[] = {
      {"Test::Answer()", reinterpret_cast<Il2CppMethodPointer>(answer)},
      {"UnityEngine.Time::get_frameCount()", reinterpret_cast<Il2CppMethodPointer>(frame)},
      {"UnityEngine.Input::get_anyKey()", reinterpret_cast<Il2CppMethodPointer>(held)},
      {"UnityEngine.Input::get_anyKeyDown()", reinterpret_cast<Il2CppMethodPointer>(down)},
      {"UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)", reinterpret_cast<Il2CppMethodPointer>(key)},
      {"UnityEngine.Input::GetKeyDownInt(UnityEngine.KeyCode)", reinterpret_cast<Il2CppMethodPointer>(key_down)},
  };
  const auto* call = std::ranges::find(calls, name, &std::pair<std::string_view, Il2CppMethodPointer>::first);
  return call == std::end(calls) ? nullptr : call->second;
}
END_API
API(Il2CppObject*, il2cpp_type_get_object, (const Il2CppType*))
{ return &type_object; }
END_API
API(char*, il2cpp_type_get_name, (const Il2CppType* type))
{
  const auto* klass = type ? il2cpp_class_from_type(type) : nullptr;
  return strdup(klass && klass->name ? klass->name : "?");
}
END_API
API(void, il2cpp_free, (void* pointer))
{ std::free(pointer); }
END_API
API(bool, il2cpp_class_is_valuetype, (const Il2CppClass* klass))
{ return klass->byval_arg.valuetype; }
END_API
API(bool, il2cpp_class_is_enum, (const Il2CppClass* klass))
{ return klass == &state_class.klass || Fake(klass).is_enum; }
END_API
API(int32_t, il2cpp_class_value_size, (Il2CppClass * klass, uint32_t* alignment))
{
  if (alignment)
    *alignment = Fake(klass).value_alignment;
  return Fake(klass).value_size;
}
END_API
API(int32_t, il2cpp_class_instance_size, (Il2CppClass * klass))
{ return static_cast<int32_t>(klass->instance_size); }
END_API
API(bool, il2cpp_class_is_assignable_from, (Il2CppClass * klass, Il2CppClass* other))
{
  // The real runtime dereferences both classes.
  if (!klass || !other) {
    std::fprintf(stderr, "il2cpp_class_is_assignable_from called with a null class\n");
    std::abort();
  }
  for (; other; other = other->parent)
    if (other == klass)
      return true;
  return false;
}
END_API
API(void, il2cpp_runtime_class_init, (Il2CppClass * klass))
{ initialized.push_back(klass); }
END_API
API(const MethodInfo*, il2cpp_object_get_virtual_method, (Il2CppObject * object, const MethodInfo* method))
{
  for (auto* klass = object->klass; klass; klass = klass->parent)
    for (const auto* candidate : std::span(klass->methods, klass->method_count))
      if (std::string_view(candidate->name) == method->name)
        return candidate;
  return nullptr;
}
END_API
API(void*, il2cpp_object_unbox, (Il2CppObject * object))
{ return object == &warp_vector_box.header ? static_cast<void*>(&warp_vector_box.value) : &shared_result; }
END_API
API(Il2CppObject*, il2cpp_runtime_invoke, (const MethodInfo* method, void*, void** args, Il2CppException** exception))
{
  if (method == warp_move_method) {
    ++warp_move_calls;
    const auto* position = static_cast<const il2cpp::Nullable<unity::Vector3>*>(args[0]);
    Require(position->has_value && !*static_cast<const bool*>(args[3]),
            "runtime receives present destination and force=false");
    warp_seen_position = position->value;
    warp_seen_node     = *static_cast<const int64_t*>(args[1]);
    warp_seen_fleet    = *static_cast<const int64_t*>(args[2]);
    if (warp_move_failure)
      *exception = &thrown;
    return nullptr;
  }
  if (method == warp_world_method || method == warp_system_method || method == warp_galaxy_method) {
    ++warp_vector_calls;
    if (method == warp_world_method) {
      Require(*static_cast<const int32_t*>(args[1]) == 1, "world projection receives galaxy depth");
      warp_vector_box.value = *static_cast<const unity::Vector3*>(args[0]);
    }
    if (warp_vector_failure)
      *exception = &thrown;
    return warp_vector_failure || warp_vector_null ? nullptr
           : warp_vector_wrong_type                ? &string_object
                                                   : &warp_vector_box.header;
  }
  if (method != shared_method || !exception) {
    std::fputs("unexpected boxed invocation\n", stderr);
    std::abort();
  }
  ++boxed_invocations;
  if (boxed_failure)
    *exception = &thrown;
  return boxed_failure || boxed_null ? nullptr : &string_object;
}
END_API
API(Il2CppGCHandle, il2cpp_gchandle_new, (Il2CppObject * object, bool))
{
  if (warp_root_failure)
    return nullptr;
  auto handle = reinterpret_cast<Il2CppGCHandle>(warp_next_root++);
  warp_roots.emplace(handle, object);
  return handle;
}
END_API
API(Il2CppObject*, il2cpp_gchandle_get_target, (Il2CppGCHandle handle))
{ return warp_roots.contains(handle) ? warp_roots.at(handle) : nullptr; }
END_API
API(void, il2cpp_gchandle_free, (Il2CppGCHandle handle))
{ warp_roots.erase(handle); }
END_API
API(void, il2cpp_format_exception, (const Il2CppException*, char* message, int size))
{ std::snprintf(message, size, "NullReferenceException"); }
END_API
API(Il2CppObject*, il2cpp_object_new, (const Il2CppClass* klass))
{
  if (klass == &action_class.klass) {
    delegate_object.object.klass = const_cast<Il2CppClass*>(klass);
    return &delegate_object.object;
  }
  allocated              = {};
  allocated.header.klass = const_cast<Il2CppClass*>(klass);
  return &allocated.header;
}
END_API
API(void, il2cpp_field_set_value, (Il2CppObject * object, FieldInfo* field, void* value))
{
  if (object == &chat_profile_avatar.header || object == &chat_admin_avatar.header) {
    Require(std::string_view(field->name) == "_keepInCache" && field->type->type == IL2CPP_TYPE_BOOLEAN,
            "chat retention must write the qualified Boolean field");
    Require(value && *static_cast<bool*>(value), "chat retention writes true rather than a transient false");
    chat_retention_writes.push_back(object);
  }
  // Like Field::SetValueRaw: value types are copied from the pointer, references are the pointer.
  auto  size = Fake(static_cast<Il2CppClass*>(field->type->data.dummy)).value_size;
  auto* dest = reinterpret_cast<char*>(object) + field->offset;
  if (size)
    std::memcpy(dest, value, size);
  else
    std::memcpy(dest, &value, sizeof(void*));
}
END_API
API(void, il2cpp_field_get_value, (Il2CppObject * object, FieldInfo* field, void* value))
{
  const auto size = Fake(il2cpp_class_from_type(field->type)).value_size;
  std::memcpy(value, reinterpret_cast<const char*>(object) + field->offset, size ? size : sizeof(void*));
}
END_API
API(int, il2cpp_field_get_flags, (FieldInfo * field))
{ return field->type->attrs; }
END_API
API(Il2CppClass*, il2cpp_object_get_class, (Il2CppObject * object))
{ return object ? object->klass : nullptr; }
END_API
API(void, il2cpp_field_static_get_value, (FieldInfo * field, void* value))
{
  if (std::string_view(field->name) == "s_initialized")
    std::memcpy(value, &settings_singleton_initialized, sizeof(settings_singleton_initialized));
  else if (std::string_view(field->name) == "s_instance")
    std::memcpy(value, &settings_singleton_instance, sizeof(settings_singleton_instance));
  else
    std::memcpy(value, &static_flag, sizeof(static_flag));
}
END_API
API(void, il2cpp_field_static_set_value, (FieldInfo*, void* value))
{ std::memcpy(&static_flag, value, sizeof(static_flag)); }
END_API

namespace
{
struct ExistingSettingsSingleton : il2cpp::Object {
  struct Class {
    static Il2CppClass* Raw()
    { return &settings_singleton_class.klass; }
  };
};
struct Base : il2cpp::Object {
  using Class = il2cpp::Class<"Game", "Game", "Base">;

  int32_t Count()
  { return Get<"Count", int32_t>(); }
  void SetCount(int32_t value)
  { Set<"Count">(value); }
  int32_t count()
  { return Field<"_count", int32_t>(); }
  void SetCount_(int32_t value)
  { SetField<"_count">(value); }
  Base* target()
  { return Field<"_target", Base*>(); }
  void SetTarget(Base* value)
  { SetField<"_target">(value); }
  Vec3 Position()
  { return Get<"Position", Vec3>(); }
  const char* Describe()
  { return Call<"Describe", const char*>(); }
};
struct Derived : Base {
  using Class = il2cpp::Class<"Game", "Game", "Derived">;
};
struct Other : il2cpp::Object {
  using Class = il2cpp::Class<"Game", "Game", "Other">;
};
// A binding whose class is not in the game, as after an update renames it.
struct Gone : il2cpp::Object {
  using Class = il2cpp::Class<"Game", "Game", "Gone">;
};
template <typename E>
concept SupportsReferenceArray = requires { il2cpp::TryReferenceArray<E>(nullptr); };
static_assert(SupportsReferenceArray<Base*> && SupportsReferenceArray<Il2CppObject*>
              && SupportsReferenceArray<Il2CppString*> && SupportsReferenceArray<Il2CppReflectionType*>);
static_assert(!SupportsReferenceArray<int*> && !SupportsReferenceArray<void*> && !SupportsReferenceArray<void (*)()>
              && !SupportsReferenceArray<int>);
enum class State : int32_t { kIdle = 0, kBusy = 2 };

// Detour targets for the hook set test, long enough for SPUD's patch.
[[gnu::noinline]] int Scaled(int value)
{
  volatile int result = value;
  for (int i = 0; i < 2; ++i)
    result = result * 3 + 1;
  return result - value;
}
[[gnu::noinline]] int Offset(int value)
{
  volatile int result = value;
  for (int i = 0; i < 2; ++i)
    result = result + 5;
  return result * 2;
}
int Scaled_Hook(auto original, int value)
{ return original(value) + 100; }
int Offset_Hook(auto original, int value)
{ return original(value) + 1000; }

size_t ErrorsMentioning(std::string_view text)
{
  return std::ranges::count_if(errors, [text](const std::string& error) { return error.contains(text); });
}

void ChatAvatarRetentionObservations()
{
  auto* message = reinterpret_cast<ChatMessageWidget*>(&chat_message);
  Require(ChatMessageWidget::AvatarRetentionStatus() == ChatMessageWidget::RetentionStatus::Ready,
          "chat retention qualifies the exact reference and Boolean field schema");
  Require(ChatMessageWidget::AvatarRetentionTarget().Info() == chat_set_data_method && chat_set_data_calls == 0,
          "retention target lookup finds only the widget's own void binding without invoking it");
  const auto reset = [] {
    chat_profile_avatar.keep_in_cache = chat_admin_avatar.keep_in_cache = false;
    chat_profile.header.klass                                           = &chat_profile_class.klass;
    chat_profile.avatar                                                 = &chat_profile_avatar.header;
    chat_message.profile                                                = &chat_profile.header;
    chat_message.admin                                                  = &chat_admin_avatar.header;
    chat_retention_writes.clear();
  };
  reset();
  Require(message->RetainAvatars() && chat_profile_avatar.keep_in_cache && chat_admin_avatar.keep_in_cache,
          "actual typed chat binding retains both profile and admin avatar assets");
  Require(chat_retention_writes == std::vector<Il2CppObject*>{&chat_profile_avatar.header, &chat_admin_avatar.header},
          "retention writes each distinct managed avatar once");
  Require(std::ranges::all_of(chat_profile_avatar.adjacent, [](unsigned char value) { return value == 0xa5; })
              && std::ranges::all_of(chat_admin_avatar.adjacent, [](unsigned char value) { return value == 0x5a; }),
          "the exact Boolean write leaves adjacent managed storage unchanged");
  Require(warp_roots.empty(), "retention releases temporary strong roots after the writes");

  reset();
  chat_message.admin = &chat_profile_avatar.header;
  Require(message->RetainAvatars() && chat_profile_avatar.keep_in_cache && !chat_admin_avatar.keep_in_cache
              && chat_retention_writes == std::vector<Il2CppObject*>{&chat_profile_avatar.header},
          "profile and admin aliases produce one write to their shared avatar");

  reset();
  chat_message.profile = nullptr;
  Require(message->RetainAvatars() && chat_admin_avatar.keep_in_cache
              && chat_retention_writes == std::vector<Il2CppObject*>{&chat_admin_avatar.header},
          "a null profile leaves a present admin avatar available for retention");
  reset();
  chat_message.admin = nullptr;
  Require(message->RetainAvatars() && chat_profile_avatar.keep_in_cache
              && chat_retention_writes == std::vector<Il2CppObject*>{&chat_profile_avatar.header},
          "a null admin leaves a present profile avatar available for retention");
  reset();
  chat_profile.avatar = nullptr;
  chat_message.admin  = nullptr;
  Require(message->RetainAvatars() && chat_retention_writes.empty(), "null avatar references are benign");
  chat_message.profile = nullptr;
  Require(message->RetainAvatars() && chat_retention_writes.empty(), "an unbound chat row is benign");

  reset();
  chat_profile.header.klass = &other_class.klass;
  chat_message.admin        = nullptr;
  Require(!message->RetainAvatars() && chat_retention_writes.empty(),
          "a runtime reference of the wrong profile class is refused before field storage is read");
  reset();
  chat_profile.avatar = &other_object.header;
  chat_message.admin  = nullptr;
  Require(!message->RetainAvatars() && chat_retention_writes.empty(),
          "an unexpected profile avatar class cannot receive the retention write");
  reset();
  chat_message.profile = nullptr;
  chat_message.admin   = &other_object.header;
  Require(!message->RetainAvatars() && chat_retention_writes.empty(),
          "an unexpected admin avatar class cannot receive the retention write");
  reset();
  warp_root_failure = true;
  Require(!message->RetainAvatars() && chat_retention_writes.empty(),
          "unavailable managed rooting refuses retention without writing unprotected objects");
  warp_root_failure = false;
  Require(warp_roots.empty() && chat_set_data_calls == 0,
          "all retention paths release their roots and leave SetWidgetData invocation to the real hook");
  reset();
  std::cout << "IL2CPP chat avatar typed retention, null references and alias deduplication passed\n";
}

void ChatAvatarMetadataRejection(std::string_view mode)
{
  auto& keep    = chat_avatar_class.fields[0];
  auto& avatar  = chat_profile_class.fields[0];
  auto& profile = chat_message_class.fields[0];
  auto& admin   = chat_message_class.fields[1];
  if (mode == "chat-avatar-native")
    keep.name = "LegacyNativeRetention";
  else if (mode == "chat-avatar-reject-profile-type")
    profile.type = T(int_class);
  else if (mode == "chat-avatar-reject-avatar-type")
    avatar.type = T(other_class);
  else if (mode == "chat-avatar-reject-admin-type")
    admin.type = T(other_class);
  else if (mode == "chat-avatar-reject-keep-type")
    const_cast<Il2CppType*>(keep.type)->type = IL2CPP_TYPE_I1;
  else if (mode == "chat-avatar-reject-static")
    const_cast<Il2CppType*>(keep.type)->attrs |= FIELD_ATTRIBUTE_STATIC;
  else if (mode == "chat-avatar-reject-byref")
    const_cast<Il2CppType*>(keep.type)->byref = 1;
  else if (mode == "chat-avatar-reject-header-offset")
    keep.offset = sizeof(Il2CppObject) - 1;
  else if (mode == "chat-avatar-reject-negative-offset")
    keep.offset = -1;
  else if (mode == "chat-avatar-reject-overflow-offset")
    keep.offset = std::numeric_limits<int32_t>::max();
  else if (mode == "chat-avatar-reject-end-offset")
    keep.offset = chat_avatar_class.klass.instance_size;
  else if (mode == "chat-avatar-reject-undersized-owner")
    chat_message_class.klass.instance_size = sizeof(Il2CppObject);
  else
    Require(false, "unknown chat avatar schema fixture mode");
  const auto expected = mode == "chat-avatar-native" ? ChatMessageWidget::RetentionStatus::NativeBehavior
                                                     : ChatMessageWidget::RetentionStatus::Unavailable;
  Require(ChatMessageWidget::AvatarRetentionStatus() == expected,
          "unqualified chat avatar field metadata must not enable the optional feature");
  Require(chat_retention_writes.empty() && chat_set_data_calls == 0 && warp_roots.empty(),
          "schema refusal neither writes managed fields nor invokes the original binding");
  std::cout << "IL2CPP chat avatar schema refusal passed: " << mode << '\n';
}

void ChatAvatarMethodRejection(std::string_view mode)
{
  auto* method = const_cast<MethodInfo*>(chat_set_data_method);
  if (mode == "chat-avatar-method-static")
    method->flags |= METHOD_ATTRIBUTE_STATIC;
  else if (mode == "chat-avatar-method-generic")
    method->is_generic = 1;
  else if (mode == "chat-avatar-method-inflated")
    method->is_inflated = 1;
  else if (mode == "chat-avatar-method-return")
    method->return_type = T(int_class);
  else if (mode == "chat-avatar-method-parameters") {
    // A changed managed overload has a real parameter table. Keep that metadata
    // consistent so the production resolver can describe the rejected signature.
    auto& parameters         = parameter_lists.emplace_back(std::vector<const Il2CppType*>{T(int_class)});
    method->parameters       = parameters.data();
    method->parameters_count = 1;
  } else if (mode == "chat-avatar-method-inherited")
    method->klass = &component_class.klass;
  else if (mode == "chat-avatar-method-null-code")
    method->methodPointer = nullptr;
  else if (mode == "chat-avatar-method-missing")
    method->name = "LegacySetWidgetData";
  else
    Require(false, "unknown chat avatar target fixture mode");
  Require(!ChatMessageWidget::AvatarRetentionTarget() && chat_set_data_calls == 0,
          "unqualified SetWidgetData metadata must refuse before native invocation");
  std::cout << "IL2CPP chat avatar target refusal passed: " << mode << '\n';
}

void ShipIdentityObservations()
{
  auto* fleet = reinterpret_cast<FleetPlayerData*>(&identity_fleet);
  Require(FleetPlayerData::ShipIdentityAvailable(), "typed ship identity capability");
  Require(fleet->ShipInstanceId() == 9007199254740993LL, "ship instance identity preserves exact large int64");
  identity_ship.id = 0;
  Require(!fleet->ShipInstanceId(), "zero ship ID is unavailable rather than fleet identity");
  identity_ship.id = -1;
  Require(!fleet->ShipInstanceId(), "negative ship ID is unavailable");
  identity_ship.id           = 42;
  identity_ship.header.klass = &other_ship_class.klass;
  Require(!fleet->ShipInstanceId(), "another IShipModel implementation is not assumed to be BaseShip");
  identity_ship.header.klass = &concrete_ship_class.klass;
  ship_id_throws             = true;
  Require(!fleet->ShipInstanceId(), "failed ship-ID getter does not become a usable zero");
  ship_id_throws   = false;
  ship_read_throws = true;
  Require(!fleet->ShipInstanceId(), "failed Ship getter remains unavailable");
  ship_read_throws    = false;
  identity_fleet.ship = nullptr;
  Require(!fleet->ShipInstanceId(), "null Ship has no fabricated identity");
  identity_fleet.ship = &identity_ship.header;
  Require(fleet->ShipInstanceId() == 42 && fleet_identity_reads == 0,
          "ship identity never substitutes FleetPlayerData.Id, hull or display name");
}

struct GalaxyTestDictionary {
  Il2CppObject                     header{};
  std::map<int32_t, Il2CppObject*> entries;
  bool                             contains_fails = false, count_changes = false;
  int32_t                          count_reads = 0;
};
int32_t GalaxyDictionaryCount(Il2CppObject* object, const MethodInfo*)
{
  auto& dictionary = *reinterpret_cast<GalaxyTestDictionary*>(object);
  return static_cast<int32_t>(dictionary.entries.size())
         + (dictionary.count_changes && ++dictionary.count_reads > 1 ? 1 : 0);
}
bool GalaxyDictionaryContains(Il2CppObject* object, int32_t key, const MethodInfo*)
{
  auto& dictionary = *reinterpret_cast<GalaxyTestDictionary*>(object);
  if (dictionary.contains_fails)
    throw std::runtime_error("controlled dictionary observation failure");
  return dictionary.entries.contains(key);
}
Il2CppObject* GalaxyDictionaryItem(Il2CppObject* object, int32_t key, const MethodInfo*)
{ return reinterpret_cast<GalaxyTestDictionary*>(object)->entries.at(key); }

void GalaxyObservations()
{
  auto& galaxy_class = Define("Digit.PrimeServer.Models", "OptimisedGalaxy", IL2CPP_TYPE_CLASS, &object_class);
  auto& node_class =
      Define("Digit.PrimeServer.Models", "GalaxyNode", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(galaxy_binding::Node));
  AddField(node_class, "_galaxy", galaxy_class, 0);
  AddField(node_class, "_index", int_class, offsetof(galaxy_binding::Node, index));
  Il2CppObject galaxy{};
  galaxy.klass = &galaxy_class.klass;
  Require(galaxy_binding::NodeLayout(), "retained GalaxyNode metadata qualifies its inline reference/index layout");
  Require(!galaxy_binding::NodeId({reinterpret_cast<galaxy_binding::OptimisedGalaxy*>(&galaxy), 0, 0}),
          "missing GalaxyNode.ID stays unavailable without invoking a null MethodInfo");
  auto& dictionary_class = Define("Game", "GalaxyInteractionDictionary", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(dictionary_class, "get_Count", reinterpret_cast<void*>(&GalaxyDictionaryCount), T(int_class), {});
  AddMethod(dictionary_class, "ContainsKey", reinterpret_cast<void*>(&GalaxyDictionaryContains), T(bool_class),
            {T(int_class)});
  AddMethod(dictionary_class, "get_Item", reinterpret_cast<void*>(&GalaxyDictionaryItem), T(object_class),
            {T(int_class)});
  GalaxyTestDictionary dictionary;
  dictionary.header.klass = &dictionary_class.klass;
  dictionary.entries      = {{2, &galaxy}, {17, nullptr}};
  auto observe_dictionary = [&] {
    galaxy_binding::PinReader  reader;
    std::vector<Il2CppObject*> values;
    return reader.Dictionary(&dictionary.header, values) && values.size() == 1 && values[0] == &galaxy;
  };
  const auto dictionary_roots = warp_roots.size();
  Require(observe_dictionary(), "production dictionary scan covers retained keys and known null values");
  dictionary.entries[18] = &galaxy;
  Require(!observe_dictionary(), "future enum keys deny retirement rather than claiming complete owners");
  dictionary.entries.erase(18);
  dictionary.contains_fails = true;
  Require(!observe_dictionary(), "failed native dictionary lookup keeps retirement unknown");
  dictionary.contains_fails = false;
  dictionary.count_changes  = true;
  Require(!observe_dictionary(), "dictionary count mutation during observation denies retirement");
  dictionary.count_changes = false;
  warp_root_failure        = true;
  Require(!observe_dictionary(), "failed dictionary value custody denies retirement");
  warp_root_failure = false;
  Require(observe_dictionary() && warp_roots.size() == dictionary_roots,
          "dictionary owner roots release on successful and failed observations");
  auto& vector_class                     = Define("System", "Int32[]", IL2CPP_TYPE_SZARRAY, &array_class);
  vector_class.klass.byval_arg.data.type = T(int_class);
  struct Vector {
    Il2CppObject        header{};
    Il2CppArrayBounds*  bounds = nullptr;
    il2cpp_array_size_t length = 3;
    int32_t             values[3]{7, 8, 9};
  } vector;
  vector.header.klass = &vector_class.klass;
  struct Repeated {
    Il2CppObject header{};
    Il2CppArray* array = nullptr;
    int32_t      count = 3;
  } repeated;
  auto& repeated_class = Define("Google.Protobuf.Collections", "RepeatedField`1", IL2CPP_TYPE_CLASS, &object_class);
  AddField(repeated_class, "array", vector_class, offsetof(Repeated, array));
  AddField(repeated_class, "count", int_class, offsetof(Repeated, count));
  repeated.header.klass = &repeated_class.klass;
  repeated.array        = reinterpret_cast<Il2CppArray*>(&vector);
  const auto before     = warp_roots.size();
  {
    auto values = galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&repeated), 3);
    Require(values && values->data.size() == 3 && values->data[0] == 7 && values->data[2] == 9
                && warp_roots.size() == before + 2,
            "Galaxy backing vector retains container and array without per-node boxing");
  }
  Require(warp_roots.size() == before, "Galaxy vector roots release after owned scan");
  repeated.count = 2;
  Require(!galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&repeated), 3),
          "mismatched source count is unavailable");
  repeated.count = 3;
  vector.length  = 2;
  Require(!galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&repeated), 3),
          "backing capacity cannot be shorter than source count");
  vector.length                          = 3;
  vector_class.klass.byval_arg.data.type = T(unsigned_int_class);
  Require(!galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&repeated), 3),
          "same-width unsigned backing values cannot become signed coordinates");
  vector_class.klass.byval_arg.data.type = T(int_class);
  warp_root_failure                      = true;
  Require(!galaxy_binding::ReadValues<int32_t>(reinterpret_cast<galaxy_binding::Reference*>(&repeated), 3),
          "failed scan custody rejects backing-vector access");
  warp_root_failure = false;
  Require(warp_roots.size() == before, "rejected Galaxy vectors leak no GC handles");
}

void ReferenceArrayObservations()
{
  struct Vector {
    Il2CppObject                 header{};
    Il2CppArrayBounds*           bounds = nullptr;
    il2cpp_array_size_t          size   = 1;
    std::array<Il2CppObject*, 1> elements{};
  } vector;
  auto& array         = Define("Game", "ReferenceVector", IL2CPP_TYPE_SZARRAY, &object_class);
  vector.header.klass = &array.klass;
  const auto input    = [&] { return &vector.header; };
  const auto element  = [&](FakeClass& type) { array.klass.byval_arg.data.type = T(type); };

  const auto null = il2cpp::TryReferenceArray<Il2CppObject*>(nullptr);
  Require(null && null.value == nullptr, "a null array input is distinguishable from invalid storage");
  element(object_class);
  Require(!il2cpp::detail::ReferenceArrayMatches(input(), il2cpp::detail::SlotOf<int32_t>()),
          "the internal predicate also refuses a non-reference request");
  vector.elements[0] = &base_object.header;
  auto objects       = il2cpp::TryReferenceArray<Il2CppObject*>(input());
  Require(objects && objects.value->size() == 1 && (*objects.value)[0] == &base_object.header,
          "object vector exposes borrowed reference elements after shape validation");
  Il2CppObject boxed_number{};
  boxed_number.klass = &int_class.klass;
  vector.elements[0] = &boxed_number;
  Require(il2cpp::TryReferenceArray<Il2CppObject*>(input()).succeeded,
          "boxed numerical objects are valid reference-vector contents");
  element(string_class);
  Require(il2cpp::TryReferenceArray<Il2CppString*>(input()).succeeded
              && il2cpp::TryReferenceArray<Il2CppObject*>(input()).succeeded,
          "string vectors support typed and untyped reference consumers");
  element(derived_class);
  Require(il2cpp::TryReferenceArray<Base*>(input()).succeeded && il2cpp::TryReferenceArray<Derived*>(input()).succeeded,
          "a derived vector is assignable toward its requested base element");
  element(base_class);
  Require(!il2cpp::TryReferenceArray<Derived*>(input()), "a base vector is not safely a derived vector");
  Require(!il2cpp::TryReferenceArray<Other*>(input()), "unrelated requested element classes are rejected");
  Require(!il2cpp::TryReferenceArray<Gone*>(input()), "missing requested element metadata is not untyped success");

  for (auto* value : {&int_class, &vec3_class, &intptr_class}) {
    element(*value);
    Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "inline value storage is not reference storage");
  }
  for (const auto kind : {IL2CPP_TYPE_PTR, IL2CPP_TYPE_FNPTR, IL2CPP_TYPE_VAR, IL2CPP_TYPE_MVAR, IL2CPP_TYPE_BYREF,
                          IL2CPP_TYPE_I, IL2CPP_TYPE_U}) {
    auto& unsupported = Define("Game", "NonReferenceElement", kind, &object_class);
    element(unsupported);
    Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "unmanaged and unresolved element kinds are rejected");
  }
  auto& generic_value = Define("Game", "GenericValue", IL2CPP_TYPE_GENERICINST, &object_class, 16);
  element(generic_value);
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "generic value arrays remain inline storage");
  auto& generic_reference = Define("Game", "GenericReference", IL2CPP_TYPE_GENERICINST, &object_class);
  element(generic_reference);
  Require(il2cpp::TryReferenceArray<Il2CppObject*>(input()).succeeded, "closed generic reference vectors are accepted");

  element(object_class);
  array.klass.byval_arg.type = IL2CPP_TYPE_ARRAY;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "bounded/multidimensional array kinds are rejected");
  array.klass.byval_arg.type = IL2CPP_TYPE_CLASS;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "ordinary reference objects are not vectors");
  array.klass.byval_arg.type  = IL2CPP_TYPE_SZARRAY;
  array.klass.byval_arg.byref = 1;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "a by-reference vector descriptor is rejected");
  array.klass.byval_arg.byref     = 0;
  array.klass.byval_arg.data.type = Ref(object_class);
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "by-reference elements are rejected");
  array.klass.byval_arg.data.type = nullptr;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "absent element metadata is unavailable");
  Il2CppType unresolved{};
  unresolved.type                 = IL2CPP_TYPE_CLASS;
  array.klass.byval_arg.data.type = &unresolved;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "unresolved element class is unavailable");
  vector.header.klass = nullptr;
  Require(!il2cpp::TryReferenceArray<Il2CppObject*>(input()), "absent runtime class is unavailable");
}

void ReferenceArrayGetterObservations()
{
  auto*        source = reinterpret_cast<Base*>(&base_object);
  Il2CppObject observed_array{};
  observed_array.klass = &object_array_class.klass;
  array_getter_value   = &observed_array;
  const auto objects   = source->TryCall<"get_ReferenceParameters", il2cpp::ArrayObject*>();
  Require(objects && objects.value == reinterpret_cast<il2cpp::ArrayObject*>(&observed_array)
              && il2cpp::TryReferenceArray<Il2CppObject*>(objects.value).succeeded,
          "a bound array result qualifies acquisition before vector storage is inspected");
  observed_array.klass = &string_array_class.klass;
  Require(source->TryCall<"get_StringParameters", il2cpp::ArrayObject*>().succeeded,
          "covariant declared string-array metadata remains a managed array result");
  const auto before = array_getter_calls;
  Require(!source->TryCall<"get_PointerParameters", il2cpp::ArrayObject*>()
              && !source->TryCall<"get_IntPtrParameters", il2cpp::ArrayObject*>()
              && !source->TryCall<"get_UIntPtrParameters", il2cpp::ArrayObject*>()
              && !source->TryCall<"get_ValueParameters", il2cpp::ArrayObject*>(),
          "pointer and numerical return metadata cannot provide candidate object bits");
  Require(array_getter_calls == before, "invalid declared return kinds are rejected before native invocation");
  array_getter_value = nullptr;
  const auto null    = source->TryCall<"get_ReferenceParameters", il2cpp::ArrayObject*>();
  Require(null && null.value == nullptr && array_getter_calls == before + 1,
          "a successful null array getter is distinct from an unavailable getter");
}

void CheckedUnityVisualObservations()
{
  Require(reinterpret_cast<il2cpp::Object*>(&string_object)->Is<Il2CppString>()
              && !reinterpret_cast<il2cpp::Object*>(&visual_renderer)->Is<Il2CppString>()
              && !reinterpret_cast<il2cpp::Object*>(&string_object)->Is<Gone>(),
          "runtime string qualification accepts strings and rejects unrelated or missing expected classes");
  const auto before = visual_calls;
  Require(unity::TryAlive(nullptr).succeeded && !unity::TryAlive(nullptr).value && visual_calls == before,
          "null Unity input is confirmed absent without invoking native code");
  visual_alive    = false;
  const auto gone = unity::TryAlive(&visual_renderer);
  Require(gone && !gone.value, "a false Unity liveness observation is available");
  visual_throws = true;
  Require(!unity::TryAlive(&visual_renderer), "a failed Unity liveness observation remains unavailable");
  Require(!unity::TryInstanceId(reinterpret_cast<il2cpp::Object*>(&visual_renderer)),
          "failed identity is not reported as observed zero");
  visual_throws  = false;
  visual_alive   = true;
  auto* renderer = reinterpret_cast<unity::Renderer*>(&visual_renderer);
  Require(unity::TryInstanceId(renderer).value == -17, "signed negative Unity identity is preserved");
  visual_id       = 0;
  const auto zero = unity::TryInstanceId(renderer);
  Require(zero && zero.value == 0, "observed Unity identity zero remains distinct from unavailable");
  visual_enabled      = false;
  const auto disabled = renderer->TryEnabled();
  Require(disabled && !disabled.value, "disabled renderer is not an unavailable getter");
  visual_set_throws = true;
  Require(!renderer->TrySetEnabled(true) && renderer->TryEnabled().value,
          "a failing setter can mutate before reporting failure");
  visual_set_throws = false;
  Require(renderer->TrySetEnabled(false).succeeded && !renderer->TryEnabled().value,
          "checked renderer setter and independent readback agree");
  auto*      material = reinterpret_cast<unity::Material*>(&visual_material);
  auto*      name     = reinterpret_cast<Il2CppString*>(&string_object);
  const auto depth    = material->TryGetFloat(name);
  Require(depth && depth.value == 0.0f, "finite zero material depth is an available observation");
  Require(material->TrySetFloat(name, -1.0f).succeeded && material->TryGetFloat(name).value == -1.0f,
          "checked material write can be confirmed separately");
  visual_property   = false;
  const auto absent = material->TryHasProperty(name);
  Require(absent && !absent.value, "absent material property is an available false observation");
  Require(material->TryShader().value == reinterpret_cast<unity::Shader*>(&visual_shader), "typed shader getter");
  Require(unity::TryName(material).value == name, "managed name observation is preserved before UTF8 conversion");
  const auto materials = renderer->TrySharedMaterials();
  Require(materials && il2cpp::TryReferenceArray<unity::Material*>(materials.value),
          "shared materials acquire a managed array before storage qualification");
  const auto components =
      selected_object.TryComponentsInChildren(reinterpret_cast<Il2CppReflectionType*>(&type_object), true);
  Require(components && il2cpp::TryReferenceArray<unity::Component*>(components.value)
              && !il2cpp::TryReferenceArray<unity::Renderer*>(components.value),
          "non-generic child lookup preserves Component[] storage instead of assuming Renderer[]");
  visual_active = false;
  Require(selected_object.TryActiveInHierarchy().succeeded && !selected_object.TryActiveInHierarchy().value,
          "inactive owner observation remains qualified false");
  visual_active = true;
}

void PlannedWarpBindingObservations()
{
  using Position    = il2cpp::Nullable<unity::Vector3>;
  const auto layout = [&] {
    return il2cpp::detail::NullableLayoutMatches(T(nullable_vector_class), &vec3_class.klass, sizeof(Position),
                                                 alignof(Position), offsetof(Position, has_value),
                                                 offsetof(Position, value));
  };
  Require(layout(), "constructed corlib Nullable<Vector3> has the qualified payload layout");
  Require(!il2cpp::detail::NullableLayoutMatches(T(nullable_vector_class), nullptr, sizeof(Position), alignof(Position),
                                                 offsetof(Position, has_value), offsetof(Position, value)),
          "missing expected Vector3 metadata does not establish a nullable payload");
  Require(!il2cpp::detail::ValueClassMatches(T(foreign_vector_class), &vec3_class.klass, sizeof(Vec3)),
          "same-sized foreign value classes are not Unity Vector3");
  nullable_generic.type = T(foreign_nullable_definition);
  Require(!layout(), "same-named foreign Nullable definition is not corlib Nullable");
  nullable_generic.type = T(nullable_definition);
  nullable_arguments[0] = T(foreign_vector_class);
  Require(!layout(), "different same-size generic payload is refused");
  nullable_arguments[0]   = T(vec3_class);
  nullable_inst.type_argc = 0;
  Require(!layout(), "missing generic argument is refused");
  nullable_inst.type_argc          = 1;
  auto*       flag                 = &nullable_vector_class.fields[0];
  auto*       value                = &nullable_vector_class.fields[1];
  const auto  original_flag        = *flag->type;
  const auto  original_value       = *value->type;
  const auto  original_flag_offset = flag->offset, original_value_offset = value->offset;
  const auto* original_flag_name = flag->name;
  flag->name                     = "MissingFlag";
  Require(!layout(), "missing nullable storage field is refused");
  flag->name                                = original_flag_name;
  const_cast<Il2CppType*>(flag->type)->type = IL2CPP_TYPE_I1;
  Require(!layout(), "same-size byte is not the nullable Boolean flag");
  *const_cast<Il2CppType*>(flag->type) = original_flag;
  for (auto attrs : {FIELD_ATTRIBUTE_STATIC, FIELD_ATTRIBUTE_LITERAL}) {
    const_cast<Il2CppType*>(value->type)->attrs |= attrs;
    Require(!layout(), "static or literal payload field is not instance storage");
    *const_cast<Il2CppType*>(value->type) = original_value;
  }
  const_cast<Il2CppType*>(value->type)->byref = 1;
  Require(!layout(), "byref nullable payload storage is refused");
  *const_cast<Il2CppType*>(value->type) = original_value;
  ++flag->offset;
  Require(!layout(), "wrong nullable flag offset is refused");
  flag->offset = original_flag_offset;
  ++value->offset;
  Require(!layout(), "wrong nullable payload offset is refused");
  value->offset                         = original_value_offset;
  nullable_vector_class.value_alignment = 8;
  Require(!layout(), "different native nullable alignment is refused");
  nullable_vector_class.value_alignment       = alignof(Position);
  nullable_vector_class.klass.byval_arg.byref = 1;
  Require(!layout(), "byref nullable parameter is refused");
  nullable_vector_class.klass.byval_arg.byref = 0;
  nullable_vector_class.value_size            = sizeof(Position) + 4;
  Require(!layout(), "wrong overall nullable allocation size is refused");
  nullable_vector_class.value_size = sizeof(Position);
  Require(layout(), "all malformed metadata cases restore the qualified canonical candidate");

  Require(il2cpp::detail::ExactScalarMatches(T(warp_result_enum), IL2CPP_TYPE_I4)
              && !il2cpp::detail::ExactScalarMatches(T(unsigned_int_class), IL2CPP_TYPE_I4)
              && !il2cpp::detail::ExactScalarMatches(T(unsigned_long_class), IL2CPP_TYPE_I8),
          "signed enum storage is qualified while equally sized unsigned scalars are refused");
  auto*      observed_course = reinterpret_cast<CourseData*>(&warp_course);
  const auto action          = observed_course->TryTargetAction();
  const auto exact_fleet     = observed_course->TryFleetId();
  Require(action && action.value == -1 && exact_fleet && exact_fleet.value == 9007199254740993LL,
          "actual course getters preserve signed enum and exact signed fleet observations");
  const auto reference = observed_course->TryTarget();
  const auto singleton = planned_warp_binding::StaticReference<"get_Instance", DeploymentManager, DeploymentManager>();
  Require(reference && reference.value == reinterpret_cast<CourseNode*>(&warp_node) && singleton
              && singleton.value == reinterpret_cast<DeploymentManager*>(&warp_deployment),
          "checked instance and static reference acquisition preserve their native targets");
  const auto base_signed =
      planned_warp_binding::Scalar<"get_CanonicalSigned", int32_t>(reinterpret_cast<Base*>(&base_object));
  const auto scalar_calls = warp_scalar_calls;
  const auto changed_signed =
      planned_warp_binding::Scalar<"get_CanonicalSigned", int32_t>(reinterpret_cast<Base*>(&derived_object));
  Require(base_signed && base_signed.value == -1 && !changed_signed && warp_scalar_calls == scalar_calls,
          "virtual getter signedness drift is refused before invoking the changed target");

  auto*                manager = reinterpret_cast<NavigationManager*>(&warp_navigation);
  const unity::Vector3 destination{0.0f, 7.5f, -3.0f};
  Require(NavigationManager::MoveFleetActionMethod().Info() == warp_move_method,
          "manual click hook and planned invocation share the same qualified move entry");
  Require(manager->TryMovePlanned(destination, 9007199254740993LL, 9007199254740995LL).succeeded,
          "actual named movement binding dispatches through the runtime invoker");
  Require(warp_move_calls == 1 && warp_seen_node == 9007199254740993LL && warp_seen_fleet == 9007199254740995LL
              && warp_seen_position.x == 0 && warp_seen_position.y == 7.5f && warp_seen_position.z == -3,
          "runtime arguments preserve exact IDs, position, present flag and force=false without a second dispatch");
  warp_move_failure = true;
  Require(!manager->TryMovePlanned(destination, 5, 7) && warp_move_calls == 2,
          "managed movement exception remains a failed observation without retry");
  warp_move_failure          = false;
  LocaleTextContext* message = nullptr;
  auto*              helper  = reinterpret_cast<FleetNavigationHelper*>(&warp_helper);
  const auto         valid   = helper->TryValidateInput(9007199254740993LL, message);
  Require(valid && valid.value && message == reinterpret_cast<LocaleTextContext*>(&warp_locale)
              && warp_seen_fleet == 9007199254740993LL && warp_validator_calls == 1,
          "actual validator binding preserves the long ID and byref LocaleTextContext output");
  warp_validator_value = false;
  const auto invalid   = helper->TryValidateInput(7, message);
  Require(invalid && !invalid.value, "native validator false is available, not a missing result");
  auto*      deployment = reinterpret_cast<DeploymentManager*>(&warp_deployment);
  const auto course     = deployment->TryPlannedCourse(9007199254740993LL);
  Require(course && course.value == reinterpret_cast<CourseData*>(&warp_course),
          "planned course retrieval does not depend on PlayerFleet being populated");
  warp_vector_box.value = {};
  auto*      node       = reinterpret_cast<CourseNode*>(&warp_node);
  const auto zero       = node->TrySystemPosition();
  Require(zero && zero.value.x == 0 && zero.value.y == 0 && zero.value.z == 0,
          "actual typed coordinate getter preserves an observed zero vector");
  auto* system_method        = const_cast<MethodInfo*>(warp_system_method);
  system_method->return_type = T(foreign_vector_class);
  const auto vector_calls    = warp_vector_calls;
  Require(!node->TrySystemPosition() && warp_vector_calls == vector_calls,
          "same-size foreign declared coordinate return is rejected before runtime invocation");
  system_method->return_type = T(vec3_class);
  warp_vector_wrong_type     = true;
  Require(!node->TryGalaxyPosition(), "wrong returned box cannot become an observed coordinate");
  warp_vector_wrong_type = false;
  warp_vector_null       = true;
  Require(!node->TrySystemPosition(), "successful null value box remains unavailable");
  warp_vector_null  = false;
  warp_root_failure = true;
  Require(!node->TrySystemPosition(), "failed local box custody prevents coordinate traversal");
  warp_root_failure = false;
  const auto projected =
      reinterpret_cast<NavigationTransitionAnimator*>(&warp_animator)->TryWorldToDepth(destination, 1);
  Require(projected && projected.value.y == destination.y && projected.value.z == destination.z,
          "actual projection binding passes owned Vector3 arguments through runtime marshaling");
  const auto position = reinterpret_cast<NavigationInteractionUIContext*>(&warp_context)->TryPosition();
  Require(position && position.value.x == 0 && position.value.y == 0 && position.value.z == 0,
          "strict context field schema preserves zero as an observed position");
  Require(warp_roots.empty(), "coordinate observations release every temporary managed root");
  std::cout << "IL2CPP planned warp value layouts and runtime dispatch passed\n";
}
} // namespace

int main(int argc, char** argv)
{
  BuildRuntime();
  il2cpp::SetErrorHandler([](std::string_view message) { errors.emplace_back(message); });
  if (argc == 2 && std::string_view(argv[1]).starts_with("chat-avatar-method-")) {
    ChatAvatarMethodRejection(argv[1]);
    return 0;
  }
  if (argc == 2 && std::string_view(argv[1]).starts_with("chat-avatar-")) {
    ChatAvatarMetadataRejection(argv[1]);
    return 0;
  }
  if (argc == 2 && std::string_view(argv[1]).starts_with("warp-reject-")) {
    const std::string_view mode(argv[1]);
    auto*                  move = const_cast<MethodInfo*>(warp_move_method);
    if (mode == "warp-reject-null-move-code")
      move->methodPointer = nullptr;
    else if (mode == "warp-reject-null-invoker")
      move->invoker_method = nullptr;
    else if (mode == "warp-reject-null-getter-code")
      const_cast<MethodInfo*>(warp_action_method)->methodPointer = nullptr;
    else if (mode == "warp-reject-null-course-code")
      const_cast<MethodInfo*>(warp_course_method)->virtualMethodPointer = nullptr;
    else if (mode == "warp-reject-null-validator-code")
      const_cast<MethodInfo*>(warp_validator_method)->methodPointer = nullptr;
    else if (mode == "warp-reject-null-vector-invoker")
      const_cast<MethodInfo*>(warp_world_method)->invoker_method = nullptr;
    else if (mode == "warp-reject-null-shared-invoker") {
      auto* getter                               = const_cast<MethodInfo*>(warp_action_method);
      getter->has_full_generic_sharing_signature = 1;
      getter->invoker_method                     = nullptr;
    } else if (mode == "warp-reject-null-reference-code")
      const_cast<MethodInfo*>(warp_reference_method)->virtualMethodPointer = nullptr;
    else if (mode == "warp-reject-u4-result")
      const_cast<MethodInfo*>(warp_action_method)->return_type = T(unsigned_int_class);
    else if (mode == "warp-reject-u8-course")
      const_cast<const Il2CppType**>(warp_course_method->parameters)[0] = T(unsigned_long_class);
    else if (mode == "warp-reject-u8-move")
      const_cast<const Il2CppType**>(move->parameters)[1] = T(unsigned_long_class);
    else if (mode == "warp-reject-u8-validator")
      const_cast<const Il2CppType**>(warp_validator_method->parameters)[0] = T(unsigned_long_class);
    else if (mode == "warp-reject-u4-depth")
      const_cast<const Il2CppType**>(warp_world_method->parameters)[1] = T(unsigned_int_class);
    else if (mode == "warp-reject-static")
      move->flags |= METHOD_ATTRIBUTE_STATIC;
    else if (mode == "warp-reject-virtual")
      move->flags |= METHOD_ATTRIBUTE_VIRTUAL;
    else if (mode == "warp-reject-missing")
      move->name = "UnavailableMovement";
    else if (mode == "warp-reject-byref-void") {
      auto& type        = field_types.emplace_back(*move->return_type);
      type.byref        = 1;
      move->return_type = &type;
    } else if (mode == "warp-reject-payload")
      nullable_arguments[0] = T(foreign_vector_class);
    else if (mode == "warp-reject-nullable-class")
      nullable_definition_missing = true;
    else if (mode == "warp-reject-vector-class")
      vector_class_missing = true;
    else
      Require(false, "unknown planned warp fixture mode");
    bool refused = false;
    if (mode == "warp-reject-u4-result" || mode == "warp-reject-null-getter-code"
        || mode == "warp-reject-null-shared-invoker")
      refused = !reinterpret_cast<CourseData*>(&warp_course)->TryTargetAction() && warp_scalar_calls == 0;
    else if (mode == "warp-reject-u8-course" || mode == "warp-reject-null-course-code")
      refused = !reinterpret_cast<DeploymentManager*>(&warp_deployment)->TryPlannedCourse(7) && warp_course_calls == 0;
    else if (mode == "warp-reject-u8-validator" || mode == "warp-reject-null-validator-code") {
      LocaleTextContext* message = nullptr;
      refused                    = !reinterpret_cast<FleetNavigationHelper*>(&warp_helper)->TryValidateInput(7, message)
                                   && warp_validator_calls == 0;
    } else if (mode == "warp-reject-u4-depth" || mode == "warp-reject-null-vector-invoker")
      refused = !reinterpret_cast<NavigationTransitionAnimator*>(&warp_animator)->TryWorldToDepth({}, 1)
                && warp_vector_calls == 0;
    else if (mode == "warp-reject-null-reference-code")
      refused = !reinterpret_cast<CourseData*>(&warp_course)->TryTarget() && warp_reference_calls == 0;
    else
      refused = !reinterpret_cast<NavigationManager*>(&warp_navigation)->TryMovePlanned({}, 5, 7);
    Require(refused && warp_move_calls == 0, "unqualified movement metadata must refuse before native invocation");
    std::cout << "IL2CPP planned warp invalid binding refused: " << mode << '\n';
    return 0;
  }
  if (argc == 2 && std::string_view(argv[1]) == "string-class-missing") {
    string_class_missing = true;
    Require(!reinterpret_cast<il2cpp::Object*>(&string_object)->Is<Il2CppString>(),
            "missing String metadata cannot qualify a candidate string for UTF8 traversal");
    std::cout << "IL2CPP missing string-class observation guard passed\n";
    return 0;
  }
  if (argc != 1) {
    Require(argc == 2 && std::string_view(argv[1]) == "array-class-missing", "unknown binding fixture mode");
    array_class_missing = true;
    const auto result =
        reinterpret_cast<Base*>(&base_object)->TryCall<"get_ReferenceParameters", il2cpp::ArrayObject*>();
    Require(!result && result.value == nullptr && array_getter_calls == 0,
            "missing System.Array prevents acquisition rather than inventing an observed null");
    Require(il2cpp::TryReferenceArray<Il2CppObject*>(nullptr).succeeded,
            "null shape qualification remains independent of source acquisition failure");
    std::cout << "IL2CPP missing array-class acquisition guard passed\n";
    return 0;
  }
  ChatAvatarRetentionObservations();
  ShipIdentityObservations();
  GalaxyObservations();
  ReferenceArrayObservations();
  ReferenceArrayGetterObservations();
  CheckedUnityVisualObservations();
  PlannedWarpBindingObservations();
  auto* base = reinterpret_cast<Base*>(&base_object);

  // Properties call the generated getter with the receiver and its MethodInfo.
  Require(base->Count() == 7 && seen_self == &base_object.header, "property getter receiver");
  Require(seen_method && std::string_view(seen_method->name) == "get_Count", "trailing MethodInfo");
  base->SetCount(9);
  Require(base->Count() == 9, "property setter");
  Require(base->Position().y == 2.f, "struct return");
  Require(base->Get<"State", State>() == State::kBusy, "enum property");

  // Fields read at their offset and write through the runtime.
  Require(base->count() == 9, "field read");
  base->SetCount_(4);
  Require(base_object.count == 4, "field write");
  base->SetTarget(base);
  Require(base->target() == base, "reference field");
  Base::Class::SetField<"s_flag">(int32_t{5});
  Require(Base::Class::Field<"s_flag", int32_t>() == 5 && static_flag == 5, "static field");

  // Overloads are chosen by C++ argument types.
  base->Call<"Select">(int32_t{3});
  Require(seen_overload == "int" && seen_int == 3, "int overload");
  base->Call<"Select">(true);
  Require(seen_overload == "bool" && seen_bool, "bool overload");
  Require(base->Call<"Find", bool>(reinterpret_cast<Il2CppString*>(&string_object)) && seen_overload == "string",
          "string overload");
  Require(base->Call<"Find", bool>(Other::Class::TypeObject()) && seen_overload == "type", "type overload");

  // ref/out arguments.
  Il2CppObject* found = nullptr;
  Require(base->Call<"TryGet", bool>(int32_t{1}, std::ref(found)) && found == &other_object.header, "out argument");

  // A named signature converts the arguments and binds T& parameters to the caller's lvalues.
  base->Call<"Select", void(int32_t)>(7);
  Require(seen_overload == "int" && seen_int == 7, "signature picks overload");
  found = nullptr;
  Require(base->Call<"TryGet", bool(int32_t, Il2CppObject*&)>(1, found) && found == &other_object.header,
          "signature out argument");
  Require(Base::Class::Call<"Twice", int32_t(int32_t)>(4) == 8, "static signature call");

  // Virtual calls dispatch on the receiver; non-virtual calls do not.
  auto* derived = reinterpret_cast<Derived*>(&derived_object);
  Require(std::string_view(base->Describe()) == "base", "base dispatch");
  Require(std::string_view(static_cast<Base*>(derived)->Describe()) == "derived", "virtual dispatch");

  // Static members initialize their class.
  Require(Base::Class::Call<"Twice", int32_t>(int32_t{21}) == 42, "static call");
  Require(Base::Class::Get<"Instance", Base*>() == base, "static property");
  Require(std::ranges::contains(initialized, &base_class.klass), "class init");

  // Construction runs the matching constructor.
  auto* made = il2cpp::New<Base>(int32_t{12});
  Require(made && made->klass == &base_class.klass && made->count() == 12, "construction");

  // Constructors are not inherited; a missing one is reported instead of running a base constructor.
  Require(il2cpp::New<Derived>(int32_t{1}) == nullptr && ErrorsMentioning("Game.Derived..ctor"), "ctor not inherited");

  // Lookups happen once per binding, not per call.
  const int lookups = method_lookups;
  base->Call<"Select", void(bool)>(false);
  Require(method_lookups > lookups, "first call looks the method up");
  const int resolved = method_lookups;
  for (int i = 0; i < 3; ++i)
    base->Call<"Select", void(bool)>(i == 1);
  Require(method_lookups == resolved && seen_overload == "bool", "cached method lookup");

  // Missing and drifted members are reported once and return defaults without calling native code.
  const int calls = native_calls;
  Require(base->Get<"Missing", int32_t>() == 0 && base->Get<"Missing", int32_t>() == 0, "missing default");
  Require(ErrorsMentioning("missing property Game.Base.Missing") == 1, "missing reported once");
  base->Call<"Select">(1.5f);
  Require(ErrorsMentioning("no overload of Game.Base.Select") == 1, "drift reported");
  Require(base->Get<"Count", bool>() == false && ErrorsMentioning("property Game.Base.Count is Int32") == 1,
          "property type drift");
  Require(base->Field<"_count", Other*>() == nullptr && ErrorsMentioning("field Game.Base._count") == 1,
          "field type drift");
  Require(base->Field<"_target", Gone*>() == nullptr && ErrorsMentioning("field Game.Base._target") == 1,
          "reference to a missing class rejected");
  Require(base->Call<"Find", bool>(reinterpret_cast<Base*>(&other_object)) == false, "unrelated reference rejected");
  Require(native_calls == calls, "rejected members do not call native code");
  Require(il2cpp::Class<"Game", "Game", "Nope">::Raw() == nullptr && ErrorsMentioning("missing class Game:Game.Nope"),
          "missing class");

  // Managed exceptions are contained, reported once per method, and return the default.
  Require(base->Call<"Explode", int32_t>() == 0 && base->Call<"Explode", int32_t>() == 0, "exception contained");
  Require(ErrorsMentioning("Game.Base.Explode threw: NullReferenceException") == 1, "exception reported once");

  // Checked calls preserve the same invocation/dispatch path, while separating
  // unavailable observations from real zero, false, null and void results.
  const auto old_count  = base->Count();
  const auto wrote_zero = base->TryCall<"set_Count", void(int32_t)>(0);
  const auto read_zero  = base->TryCall<"get_Count", int32_t>();
  Require(wrote_zero && read_zero && read_zero.value == 0, "checked zero and void remain successful observations");
  base->SetCount(old_count);
  const auto missing_checked = base->TryCall<"CheckedMissing", int32_t>();
  Require(!missing_checked && missing_checked.value == 0,
          "checked missing method is unavailable, not an observed zero");
  Require(!base->TryCall<"Select", void(float)>(1.5f), "checked signature drift does not call native code");
  Require(!base->TryCall<"Explode", int32_t>() && !base->TryCall<"ExplodeVoid", void>(),
          "checked numeric and void calls preserve exception failure status");
  found                    = &other_object.header;
  const auto checked_false = base->TryCall<"TryGet", bool(int32_t, Il2CppObject*&)>(0, found);
  Require(checked_false && !checked_false.value && found == nullptr,
          "checked false result and null out parameter are valid observations");
  const auto checked_virtual = static_cast<Base*>(derived)->TryCall<"Describe", const char*>();
  Require(checked_virtual && std::string_view(checked_virtual.value) == "derived", "checked virtual dispatch");
  const auto checked_static = Base::Class::TryCall<"Twice", int32_t(int32_t)>(0);
  Require(checked_static && checked_static.value == 0, "checked static zero result");
  Require(!il2cpp::detail::TryInvoke<int32_t>(nullptr, base), "checked null metadata is unavailable");
  Require(!il2cpp::detail::TryInvoke<int32_t>(Base::Class::Method("get_Count", 0).Info(), nullptr),
          "checked null receiver is unavailable");

  // Internal calls run when present; a missing one is reported once and returns the default.
  static const auto answer  = il2cpp::ICall<int32_t()>("Test::Answer()");
  static const auto renamed = il2cpp::ICall<int32_t()>("Test::Renamed()");
  Require(answer && answer() == 42, "internal call");
  Require(!renamed && renamed() == 0 && renamed() == 0, "missing internal call returns the default");
  Require(ErrorsMentioning("missing internal call Test::Renamed()") == 1, "missing internal call reported once");

  // Startup may discover the frame counter without executing a Unity engine
  // getter. The actual read is deferred to runtime and shares that resolver.
  const auto previous_frame_reads = frame_reads;
  Require(unity::Time::FrameCountAvailable() && unity::Time::FrameCountAvailable()
              && frame_reads == previous_frame_reads,
          "frame-count installation preflight invoked the engine getter");
  Require(unity::Time::frameCount() == input_frame && frame_reads == previous_frame_reads + 1,
          "runtime frame-count read no longer invokes its shared internal call");
  std::cout << "Unity frame-count startup preflight resolves without engine invocation passed\n";

  // Nested classes resolve through their declaring type.
  Require(il2cpp::Class<"Game", "Game", "Outer/Inner">::Raw() == &inner_class.klass, "nested class");

  // Unbound wrappers resolve against each runtime class.
  auto* first  = reinterpret_cast<il2cpp::List<Il2CppObject*>*>(&list_a);
  auto* second = reinterpret_cast<il2cpp::List<Il2CppObject*>*>(&list_b);
  Require(first->Count() == 2 && second->Count() == 3 && first->Count() == 2, "per-class lookup");
  int items = 0;
  for (auto* item : *second)
    items += item != nullptr;
  Require(items == 3 && (*first)[0] == &list_a.header, "list iteration");
  Require(std::ranges::count(*second, nullptr) == 0 && std::ranges::distance(il2cpp::Items(second)) == 3,
          "lists are ranges");
  Require(il2cpp::At(first, 0) == &list_a.header && il2cpp::At(first, 2) == nullptr && il2cpp::At(first, -1) == nullptr,
          "list At checks the index");
  il2cpp::List<Il2CppObject*>*  no_list  = nullptr;
  il2cpp::Array<Il2CppObject*>* no_array = nullptr;
  Require(il2cpp::Items(no_list).empty() && il2cpp::Items(no_array).empty() && il2cpp::At(no_list, 0) == nullptr
              && il2cpp::At(no_array, 0) == nullptr,
          "a null collection reads as empty");
  Require(first->HasMethod<int32_t()>("get_Count"), "runtime collection method schema");
  Require(!first->HasMethod<int32_t(int32_t)>("get_Item"), "runtime collection schema rejects a value result");

  first->Set(1, &other_object.header);
  Require(seen_self == &list_a.header && seen_int == 1 && list_a.target == &other_object.header,
          "list setter passes the receiver, index and value");
  Require(seen_method && seen_method->klass == &list_a_class.klass, "list setter receives its runtime MethodInfo");
  second->Set(2, nullptr);
  Require(seen_self == &list_b.header && seen_int == 2 && list_b.target == nullptr
              && seen_method->klass == &list_b_class.klass,
          "list setters resolve per runtime class and accept null");

  // Fully shared generic code is marshalled by the runtime.
  Require(base->Call<"Shared", int32_t>() == 41 && boxed_invocations == 1, "shared generic fallback");
  const auto checked_shared = base->TryCall<"Shared", int32_t>();
  Require(checked_shared && checked_shared.value == 41 && boxed_invocations == 2,
          "checked shared generic value uses runtime marshalling");
  boxed_failure            = true;
  const auto failed_shared = base->TryCall<"Shared", int32_t>();
  Require(!failed_shared && failed_shared.value == 0 && base->Call<"Shared", int32_t>() == 0,
          "boxed runtime exception remains unavailable while legacy Call still returns its default");
  boxed_failure = false;
  boxed_null    = true;
  Require(!base->TryCall<"Shared", int32_t>(), "an absent numeric box is not observed zero");
  const auto null_box = il2cpp::detail::TryInvokeBoxed(shared_method, base, nullptr);
  Require(null_box && null_box.value == nullptr, "a null raw boxed/reference result without exception is successful");
  boxed_failure = true;
  Require(!il2cpp::detail::TryInvokeBoxed(shared_method, base, nullptr), "raw boxed exception preserves failure");
  boxed_failure = boxed_null = false;

  // Delegates bind static methods and can jump to native code.
  auto  target   = Base::Class::Method("OnEvent");
  auto* delegate = reinterpret_cast<Il2CppDelegate*>(
      il2cpp::NewDelegate(&action_class.klass, target, reinterpret_cast<Il2CppMethodPointer>(&NativeTarget)));
  Require(delegate && delegate_target == target.Info()
              && delegate->method_ptr == reinterpret_cast<Il2CppMethodPointer>(&NativeTarget),
          "static delegate");

  MethodInfo closed_schema           = *Base::Class::Method("ExplodeVoid").Info();
  closed_schema.methodPointer        = reinterpret_cast<Il2CppMethodPointer>(&ClosedTarget);
  closed_schema.virtualMethodPointer = closed_schema.methodPointer;
  closed_schema.invoker_method       = &ClosedInvoker;
  auto* closed                       = reinterpret_cast<Il2CppDelegate*>(
      il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(&closed_schema)));
  Require(closed && closed->target == &base_object.header && closed->method == &closed_schema,
          "closed instance delegate preserves receiver and owned callback metadata");
  closed_schema.invoker_method(closed_schema.methodPointer, &closed_schema, closed->target, nullptr, nullptr);
  il2cpp::InvokeBound(closed);
  Require(closed->method_ptr == closed_schema.methodPointer && closed->invoke_impl == closed_schema.methodPointer
              && closed->invoke_impl_this == closed->target,
          "closed delegate establishes both native dispatch pointers and the exact closed receiver");
  reinterpret_cast<void (*)(Il2CppObject*, const MethodInfo*)>(closed->invoke_impl)(closed->invoke_impl_this,
                                                                                    closed->method);
  Require(closed_delegate_calls == 3,
          "reflection, bound method and generated closed delegate paths each invoke exactly once");
  Require(
      !il2cpp::NewClosedDelegate(&action_class.klass, nullptr, il2cpp::Method(&closed_schema))
          && !il2cpp::NewClosedDelegate(&action_class.klass, base, target)
          && !il2cpp::NewClosedDelegate(&action_class.klass, base, Base::Class::Method("Explode"))
          && !il2cpp::NewClosedDelegate(&action_class.klass, base, Base::Class::Method("Describe"))
          && !il2cpp::NewClosedDelegate(&other_class.klass, base, il2cpp::Method(&closed_schema)),
      "closed delegates reject null receivers, static targets, signature mismatch, virtual targets and nondelegates");
  auto wrong_schema  = closed_schema;
  wrong_schema.klass = &other_class.klass;
  Require(!il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(&wrong_schema)),
          "closed delegates reject unrelated receiver types");
  warp_root_failure = true;
  Require(!il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(&closed_schema)),
          "closed delegate declines when receiver custody cannot be established");
  warp_root_failure                    = false;
  const auto roots_before_ctor_failure = warp_roots.size();
  delegate_ctor_throws                 = true;
  Require(!il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(&closed_schema)),
          "closed delegate construction contains a managed constructor exception");
  delegate_ctor_throws = false;
  Require(warp_roots.size() == roots_before_ctor_failure, "failed delegate construction releases temporary custody");

  // Drive the same owned schemas used by Settings through the SDK's CLOSED delegate execution slots.
  // Direct MethodInfo invocation and InvokeBound alone would miss an uninitialized invoke_impl/receiver.
  auto* action_invoke = const_cast<MethodInfo*>(il2cpp::Class<"mscorlib", "System", "Action">::Method("Invoke").Info());
  const auto original_invoke = *action_invoke;
  auto       bool_schema     = closed_schema;
  bool_schema.return_type    = T(bool_class);
  action_invoke->return_type = bool_schema.return_type;
  mod_settings::NativeCallback<bool> bool_get;
  Require(bool_get.Initialize(&bool_schema, ClosedBoolGetter), "owned bool getter donor");
  auto* get_delegate = reinterpret_cast<Il2CppDelegate*>(
      il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(bool_get.method())));
  Require(get_delegate
              && reinterpret_cast<bool (*)(Il2CppObject*, const MethodInfo*)>(get_delegate->invoke_impl)(
                  get_delegate->invoke_impl_this, get_delegate->method)
              && closed_bool_get_calls == 1,
          "native closed bool getter dispatches once");
  bool reflected_get = false;
  bool_get.method()->invoker_method(bool_get.method()->methodPointer, bool_get.method(), base, nullptr, &reflected_get);
  Require(reflected_get && closed_bool_get_calls == 2, "bool getter reflection path remains independent");

  const Il2CppType* bool_parameters[]{T(bool_class)};
  auto              setter_schema = closed_schema;
  setter_schema.parameters        = bool_parameters;
  setter_schema.parameters_count  = 1;
  *action_invoke                  = original_invoke;

  const auto static_initializations = initialized.size();
  settings_singleton_initialized    = false;
  auto existing_settings            = settings_binding::ExistingSingleton<ExistingSettingsSingleton>();
  Require(existing_settings && !existing_settings.value && initialized.size() == static_initializations,
          "uninitialized Settings singleton remains absent without initializing static storage");
  Il2CppObject existing_settings_object{};
  existing_settings_object.klass = &settings_singleton_class.klass;
  settings_singleton_initialized = true;
  settings_singleton_instance    = &existing_settings_object;
  existing_settings              = settings_binding::ExistingSingleton<ExistingSettingsSingleton>();
  Require(existing_settings
              && existing_settings.value == reinterpret_cast<ExistingSettingsSingleton*>(&existing_settings_object)
              && initialized.size() == static_initializations,
          "existing Settings singleton is observed without its constructor or Instance getter");
  settings_singleton_instance = &other_object.header;
  Require(!settings_binding::ExistingSingleton<ExistingSettingsSingleton>(),
          "unrelated Settings singleton receiver is rejected");
  settings_singleton_instance = nullptr;
  existing_settings           = settings_binding::ExistingSingleton<ExistingSettingsSingleton>();
  Require(existing_settings && !existing_settings.value, "initialized-but-absent storage remains unavailable");
  action_invoke->parameters       = bool_parameters;
  action_invoke->parameters_count = 1;
  mod_settings::NativeCallback<void, bool> bool_set;
  Require(bool_set.Initialize(&setter_schema, ClosedBoolSetter), "owned bool setter donor");
  auto* set_delegate = reinterpret_cast<Il2CppDelegate*>(
      il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(bool_set.method())));
  Require(set_delegate, "native closed bool setter constructed");
  reinterpret_cast<void (*)(Il2CppObject*, bool, const MethodInfo*)>(set_delegate->invoke_impl)(
      set_delegate->invoke_impl_this, true, set_delegate->method);
  Require(closed_bool_set_calls == 1, "native closed bool setter dispatches once");
  bool  reflected_argument = true;
  void* reflected_args[]{&reflected_argument};
  bool_set.method()->invoker_method(bool_set.method()->methodPointer, bool_set.method(), base, reflected_args, nullptr);
  Require(closed_bool_set_calls == 2, "bool setter reflection path remains independent");

  auto query_schema          = closed_schema;
  query_schema.return_type   = T(state_class);
  *action_invoke             = original_invoke;
  action_invoke->return_type = query_schema.return_type;
  mod_settings::NativeCallback<int> query_callback;
  Require(query_callback.Initialize(&query_schema, ClosedQuery), "owned enum query donor");
  auto* query_delegate = reinterpret_cast<Il2CppDelegate*>(
      il2cpp::NewClosedDelegate(&action_class.klass, base, il2cpp::Method(query_callback.method())));
  Require(query_delegate
              && reinterpret_cast<int (*)(Il2CppObject*, const MethodInfo*)>(query_delegate->invoke_impl)(
                     query_delegate->invoke_impl_this, query_delegate->method)
                     == 2
              && closed_query_calls == 1,
          "native closed enum query dispatches once");
  int reflected_query = 0;
  query_callback.method()->invoker_method(query_callback.method()->methodPointer, query_callback.method(), base,
                                          nullptr, &reflected_query);
  Require(reflected_query == 2 && closed_query_calls == 2, "enum query reflection path remains independent");
  *action_invoke = original_invoke;

  // Hook lookups by count and by parameter type names.
  Require(Base::Class::Method("Select", {"System.Boolean"}).Pointer() == reinterpret_cast<void*>(&Base_Select_Bool),
          "hook lookup by parameter types");
  Require(!Base::Class::Method("Absent", 0) && ErrorsMentioning("missing method Game.Base.Absent"), "missing hook");
  Require(Base::Class::Method("OnEvent").Name() == "Game.Base.OnEvent", "method name for diagnostics");

  Require(Base::Class::Method<void(bool)>("Select").Pointer() == reinterpret_cast<void*>(&Base_Select_Bool),
          "typed hook lookup selects the complete instance signature");
  Require(!Base::Class::Method<int32_t(bool)>("Select"), "typed hook lookup rejects a changed return type");
  Require(!Base::Class::Method<int32_t(int32_t)>("Twice"), "instance hook lookup rejects a static method");
  Require(!Base::Class::Method<int32_t()>("Shared"), "typed hook lookup rejects fully shared generic code");
  Require(Base::Class::HasField<"_count", int32_t>(), "instance field schema accepts the declared type");
  Require(!Base::Class::HasField<"_count", Other*>(), "instance field schema rejects an incompatible type");
  Require(!Base::Class::HasField<"s_flag", int32_t>(), "instance field schema rejects a static field");
  Require(Base::Class::HasProperty<"Count", int32_t>(), "property schema accepts the declared type");
  Require(!Base::Class::HasProperty<"Count", Other*>(), "property schema rejects an incompatible type");

  // Static hook lookups check the signature without running the class constructor ahead of the game.
  initialized.clear();
  Require(Base::Class::StaticMethod<int32_t(int32_t)>("Twice").Pointer() == reinterpret_cast<void*>(&Base_Twice),
          "typed static hook lookup");
  Require(initialized.empty(), "a static hook lookup leaves the class uninitialized");
  Require(!Base::Class::StaticMethod<void(bool)>("Select"), "static hook lookup rejects an instance method");

  // Boxed primitives unbox only as their own type.
  Il2CppObject boxed{};
  boxed.klass    = &int_class.klass;
  int32_t number = 0;
  int64_t wide   = 7;
  Require(il2cpp::Unbox(&boxed, number) && number == shared_result, "unbox the boxed type");
  Require(!il2cpp::Unbox(&boxed, wide) && wide == 7, "unbox rejects another type and leaves the value");
  Require(!il2cpp::Unbox(static_cast<Il2CppObject*>(nullptr), number), "unbox rejects null");

  // A hook set installs every hook or none: a missing member, or a null address, leaves its targets untouched.
  {
    const int       scaled_before = Scaled(2);
    il2cpp::HookSet incomplete("Incomplete");
    IL2CPP_STAGE(incomplete, il2cpp::Method(), Scaled_Hook);
    IL2CPP_STAGE_AT(incomplete, &Scaled, Scaled_Hook);
    Require(!incomplete.Commit() && Scaled(2) == scaled_before, "a set with a missing method installs nothing");
    Require(ErrorsMentioning("Incomplete not installed: a method it hooks is missing") == 1,
            "an incomplete set says what it cost");
#if defined(__x86_64__) || defined(_M_X64)
    const int       offset_before = Offset(2);
    il2cpp::HookSet complete("Complete");
    IL2CPP_STAGE_AT(complete, &Scaled, Scaled_Hook);
    IL2CPP_STAGE_AT(complete, &Offset, Offset_Hook);
    Require(complete.Commit() && Scaled(2) == scaled_before + 100 && Offset(2) == offset_before + 1000,
            "a complete set installs every hook");

    // Bypass Missing() so SPUD must undo the first native chain addition after
    // the second handle rejects its zero target, preserving the earlier set.
    il2cpp::HookSet rollback("NativeRollback");
    IL2CPP_STAGE_AT(rollback, &Scaled, Scaled_Hook);
    rollback.Stage(SPUD_AUTO_HOOK(static_cast<void*>(nullptr), Scaled_Hook));
    const auto refused = rollback.CommitDetailed();
    Require(refused.state == il2cpp::HookInstallState::kRejected && refused.error == spud::detour_error::kNotFound
                && !refused.Unsafe() && !il2cpp::HookStateIndeterminate(),
            "a real native transaction safely rejects a zero target");
    Require(Scaled(2) == scaled_before + 100 && Offset(2) == offset_before + 1000,
            "native rollback preserves existing hooks without retaining the added chain member");

    // Named sets own installation attempts, not exclusive target addresses.
    il2cpp::HookSet chained("IntentionalChain");
    IL2CPP_STAGE_AT(chained, &Scaled, Scaled_Hook);
    Require(chained.Commit() && Scaled(2) == scaled_before + 200 && Offset(2) == offset_before + 1000,
            "an intentional second set chains on an already-hooked native target");
    Require(chained.Commit() && complete.Commit() && Scaled(2) == scaled_before + 200
                && Offset(2) == offset_before + 1000,
            "cached commits neither add a third hook nor disturb the existing set");
#endif
  }

  // Managed strings convert to UTF-8, and compare with ASCII without converting.
  const auto utf16 = [](std::u16string_view text) {
    auto* str   = static_cast<Il2CppString*>(std::calloc(1, sizeof(Il2CppString) + text.size() * sizeof(Il2CppChar)));
    str->length = static_cast<int32_t>(text.size());
    std::copy(text.begin(), text.end(), str->chars);
    return str;
  };
  auto* mixed = utf16(u"K\u00E9rk \u2192 \U0001F596");
  Require(il2cpp::ToUtf8(mixed) == "K\xC3\xA9rk \xE2\x86\x92 \xF0\x9F\x96\x96",
          "UTF-16 to UTF-8 with a surrogate pair");
  auto* lone = utf16(std::u16string_view(u"a\xD800"
                                         "b",
                                         3));
  Require(il2cpp::ToUtf8(lone)
              == "a\xEF\xBF\xBD"
                 "b",
          "a lone surrogate becomes U+FFFD");
  Require(il2cpp::ToUtf8(nullptr).empty(), "null string");
  auto* ascii = utf16(u"Galaxy");
  Require(il2cpp::Equals(ascii, "Galaxy") && !il2cpp::Equals(ascii, "Galax") && !il2cpp::Equals(nullptr, ""),
          "ASCII comparison");
  for (auto* str : {mixed, lone, ascii})
    std::free(str);

  KeyboardSnapshot();
  InputFocus();
  std::cout << "IL2CPP binding tests passed\n";
}
