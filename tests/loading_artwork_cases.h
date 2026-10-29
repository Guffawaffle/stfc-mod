#pragma once

// Production login/transition consumers and Unity ImageAsset. Only the managed
// runtime, scene hierarchy, image decoder and filesystem path boundary are fake.
#include "../src/mod/features/loading_screens.cc"
#include <chrono>
#include <set>

namespace File
{
std::filesystem::path FromUtf8(std::string_view text)
{
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
} // namespace File

namespace
{
struct LoadingFixtureNode {
  Il2CppObject header{};
  LoadingFixtureNode *canvas = nullptr, *background = nullptr, *animator = nullptr, *image = nullptr;
  LoadingFixtureNode *object = nullptr, *transform = nullptr, *parent = nullptr;
  std::vector<LoadingFixtureNode*> children;
  std::string name, text;
  bool        alive = true, enabled = true;
  unity::Color color{1, 1, 1, 1};
  unity::Vector2 anchor_min{}, anchor_max{};
  unity::Sprite* sprite = nullptr;
};
struct LoadingFixtureType {
  Il2CppObject header{};
  Il2CppClass* type = nullptr;
};
std::deque<LoadingFixtureNode> loading_nodes;
std::map<Il2CppClass*, LoadingFixtureType> loading_types;
FakeClass *loading_transform_class, *loading_rect_class, *loading_image_class, *loading_canvas_class,
    *loading_texture_class, *loading_sprite_class;
std::set<const Il2CppObject*> loading_destroyed;
bool loading_fail_image = false, loading_fail_decode = false, loading_fail_color = false,
     loading_fail_parent = false, loading_fail_ctor = false;
int loading_decodes = 0, loading_destroys = 0, loading_created = 0, loading_animation_sets = 0;
uint8_t loading_decoded_first = 0;
std::vector<uint8_t> loading_decode_inputs;

LoadingFixtureNode* LoadingNode(Il2CppClass* type)
{
  auto& node        = loading_nodes.emplace_back();
  node.header.klass = type;
  return &node;
}
LoadingFixtureNode* LoadingGo(std::string name)
{
  auto* node       = LoadingNode(&game_object_class.klass);
  node->name       = std::move(name);
  node->object     = node;
  node->transform  = LoadingNode(&loading_transform_class->klass);
  node->transform->object = node;
  return node;
}
LoadingFixtureNode* LoadingNative(Il2CppObject* object)
{ return reinterpret_cast<LoadingFixtureNode*>(object); }
Il2CppObject* LoadingAllocate(const Il2CppClass* type, bool& handled)
{
  handled = type == &game_object_class.klass || type == &loading_texture_class->klass;
  if (!handled)
    return nullptr;
  if (type == &game_object_class.klass) {
    ++loading_created;
    return &LoadingGo("")->header;
  }
  return &LoadingNode(const_cast<Il2CppClass*>(type))->header;
}
Il2CppObject* LoadingType(const Il2CppType* type)
{
  auto* cls       = il2cpp_class_from_type(type);
  auto& reflected = loading_types[cls];
  reflected.header.klass = &type_class.klass;
  reflected.type         = cls;
  return &reflected.header;
}
bool LoadingAlive(Il2CppObject* object, const MethodInfo*)
{ return object && !loading_destroyed.contains(object); }
void LoadingDestroy(Il2CppObject* object, const MethodInfo*)
{
  if (object) {
    ++loading_destroys;
    loading_destroyed.insert(object);
  }
}
void LoadingCtor(Il2CppObject* object, Il2CppString* name, const MethodInfo*)
{
  if (loading_fail_ctor)
    throw Il2CppExceptionWrapper(&thrown);
  LoadingNative(object)->name = il2cpp::ToUtf8(name);
}
Il2CppString* LoadingName(Il2CppObject* object, const MethodInfo*)
{ return il2cpp::NewString(LoadingNative(object)->name); }
Il2CppObject* LoadingTransform(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(LoadingNative(object)->transform); }
Il2CppObject* LoadingGameObject(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(LoadingNative(object)->object); }
Il2CppObject* LoadingParent(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<Il2CppObject*>(LoadingNative(object)->parent); }
int32_t LoadingChildren(Il2CppObject* object, const MethodInfo*)
{ return static_cast<int32_t>(LoadingNative(object)->children.size()); }
Il2CppObject* LoadingChild(Il2CppObject* object, int32_t index, const MethodInfo*)
{ return &LoadingNative(object)->children.at(index)->header; }
Il2CppObject* LoadingGetComponent(Il2CppObject* object, Il2CppReflectionType* type, const MethodInfo*)
{
  auto* node = LoadingNative(object);
  auto* cls  = reinterpret_cast<LoadingFixtureType*>(type)->type;
  if (cls == &loading_image_class->klass)
    return reinterpret_cast<Il2CppObject*>(node->image);
  if (cls == &loading_transform_class->klass)
    return reinterpret_cast<Il2CppObject*>(node->transform);
  if (cls == &loading_rect_class->klass)
    return node->transform && node->transform->header.klass == cls ? &node->transform->header : nullptr;
  if (cls == &loading_canvas_class->klass)
    return reinterpret_cast<Il2CppObject*>(node->canvas);
  return nullptr;
}
Il2CppObject* LoadingAddComponent(Il2CppObject* object, Il2CppReflectionType* type, const MethodInfo*)
{
  if (loading_fail_image)
    return nullptr;
  auto* node = LoadingNative(object);
  Require(reinterpret_cast<LoadingFixtureType*>(type)->type == &loading_image_class->klass,
          "loading overlay adds only its native Image component");
  node->image = LoadingNode(&loading_image_class->klass);
  node->image->object = node;
  node->image->transform = node->transform;
  node->transform->header.klass = &loading_rect_class->klass;
  return &node->image->header;
}
void LoadingSetParent(Il2CppObject* object, unity::Transform* parent, bool world, const MethodInfo*)
{
  if (loading_fail_parent)
    throw Il2CppExceptionWrapper(&thrown);
  Require(!world && parent, "loading overlays use local parenting beneath a real canvas");
  auto* node = LoadingNative(object);
  node->parent = reinterpret_cast<LoadingFixtureNode*>(parent);
  node->parent->children.push_back(node);
}
void LoadingFirst(Il2CppObject*, const MethodInfo*) {}
void LoadingSprite(Il2CppObject* object, unity::Sprite* sprite, const MethodInfo*)
{ LoadingNative(object)->sprite = sprite; }
void LoadingColor(Il2CppObject* object, unity::Color color, const MethodInfo*)
{
  if (loading_fail_color)
    throw Il2CppExceptionWrapper(&thrown);
  LoadingNative(object)->color = color;
}
void LoadingImageType(Il2CppObject*, unity::Image::Type, const MethodInfo*) {}
void LoadingBool(Il2CppObject*, bool, const MethodInfo*) {}
void LoadingVector2(Il2CppObject*, unity::Vector2, const MethodInfo*) {}
void LoadingVector3(Il2CppObject*, unity::Vector3, const MethodInfo*) {}
void LoadingAnchorMin(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ LoadingNative(object)->anchor_min = value; }
void LoadingAnchorMax(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ LoadingNative(object)->anchor_max = value; }
void LoadingEnabled(Il2CppObject* object, bool value, const MethodInfo*)
{
  ++loading_animation_sets;
  LoadingNative(object)->enabled = value;
}
void LoadingText(Il2CppObject* object, Il2CppString* text, const MethodInfo*)
{ LoadingNative(object)->text = il2cpp::ToUtf8(text); }
void LoadingTextureCtor(Il2CppObject*, int32_t, int32_t, int32_t, bool, const MethodInfo*) {}
int32_t LoadingWidth(Il2CppObject*, const MethodInfo*)
{ return 600; }
int32_t LoadingHeight(Il2CppObject*, const MethodInfo*)
{ return 828; }
int32_t LoadingScreenWidth(const MethodInfo*)
{ return 1920; }
int32_t LoadingScreenHeight(const MethodInfo*)
{ return 1080; }
bool LoadingDecode(unity::Texture2D*, il2cpp::Array<uint8_t>* bytes, bool, const MethodInfo*)
{
  ++loading_decodes;
  loading_decoded_first = bytes->data()[0];
  loading_decode_inputs.push_back(loading_decoded_first);
  return !loading_fail_decode && loading_decoded_first != 0;
}
unity::Sprite* LoadingSpriteCreate(unity::Texture2D*, unity::Rect, unity::Vector2, const MethodInfo*)
{ return reinterpret_cast<unity::Sprite*>(LoadingNode(&loading_sprite_class->klass)); }

void LoadingArtworkConsumerCases(std::string_view mode)
{
  auto& behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  loading_transform_class = &Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &component_class);
  loading_rect_class = &Define("UnityEngine", "RectTransform", IL2CPP_TYPE_CLASS, loading_transform_class);
  loading_image_class = &Define("UnityEngine.UI", "Image", IL2CPP_TYPE_CLASS, &behaviour);
  loading_canvas_class = &Define("UnityEngine", "Canvas", IL2CPP_TYPE_CLASS, &component_class);
  loading_texture_class = &Define("UnityEngine", "Texture2D", IL2CPP_TYPE_CLASS, &unity_object_class);
  loading_sprite_class = &Define("UnityEngine", "Sprite", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& byte = Define("System", "Byte", IL2CPP_TYPE_U1, nullptr, 1);
  auto& bytes = Define("System", "Byte[]", IL2CPP_TYPE_SZARRAY, &array_class);
  bytes.klass.byval_arg.data.type = T(byte);
  auto& vector = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& color = Define("UnityEngine", "Color", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Color));
  auto& rect = Define("UnityEngine", "Rect", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Rect));
  auto& conversion = Define("UnityEngine", "ImageConversion", IL2CPP_TYPE_CLASS, &object_class);
  auto& screen = Define("UnityEngine", "Screen", IL2CPP_TYPE_CLASS, &object_class);
  auto& login = Define("Digit.Prime.Login", "LoginSequence", IL2CPP_TYPE_CLASS, &object_class);
  auto& transition = Define("Digit.Prime.LoadingScreen", "TransitionViewController", IL2CPP_TYPE_CLASS,
                            &component_class);
  auto& slide = Define("Digit.Prime.SlideShow", "SlideShowViewController", IL2CPP_TYPE_CLASS, &object_class);
  for (auto* cls : {&login, &transition, &slide})
    cls->klass.instance_size = sizeof(LoadingFixtureNode);
  AddField(login, "_mainCanvas", *loading_canvas_class, offsetof(LoadingFixtureNode, canvas));
  AddField(transition, "_staticOverride", *loading_image_class, offsetof(LoadingFixtureNode, background));
  AddField(transition, "_animator", behaviour, offsetof(LoadingFixtureNode, animator));
  AddField(slide, "_image", *loading_image_class, offsetof(LoadingFixtureNode, image));
  for (auto* method : unity_object_class.methods) {
    auto* changed = const_cast<MethodInfo*>(method);
    if (std::string_view(method->name) == "op_Implicit")
      changed->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&LoadingAlive);
    if (std::string_view(method->name) == "get_name")
      changed->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&LoadingName);
    changed->virtualMethodPointer = changed->methodPointer;
  }
  for (auto* method : game_object_class.methods)
    if (std::string_view(method->name) == "GetComponent") {
      auto* changed = const_cast<MethodInfo*>(method);
      changed->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&LoadingGetComponent);
      changed->virtualMethodPointer = changed->methodPointer;
      changed->return_type = T(component_class);
    }
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&LoadingDestroy), T(void_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  auto add = [&](FakeClass& cls, const char* name, auto function, const Il2CppType* result,
                 std::vector<const Il2CppType*> args, bool property = false, bool is_static = false) {
    auto& method = AddMethod(cls, name, reinterpret_cast<void*>(function), result, std::move(args),
                             METHOD_ATTRIBUTE_PUBLIC | (is_static ? METHOD_ATTRIBUTE_STATIC : 0));
    if (property && std::string_view(name).starts_with("get_"))
      AddProperty(cls, name + 4, &method);
  };
  add(game_object_class, ".ctor", &LoadingCtor, T(void_class), {T(string_class)});
  add(game_object_class, "AddComponent", &LoadingAddComponent, T(component_class), {T(type_class)});
  add(game_object_class, "get_transform", &LoadingTransform, T(*loading_transform_class), {}, true);
  add(component_class, "get_transform", &LoadingTransform, T(*loading_transform_class), {}, true);
  add(*loading_transform_class, "get_gameObject", &LoadingGameObject, T(game_object_class), {}, true);
  add(*loading_transform_class, "get_parent", &LoadingParent, T(*loading_transform_class), {}, true);
  add(*loading_transform_class, "get_childCount", &LoadingChildren, T(int_class), {}, true);
  add(*loading_transform_class, "GetChild", &LoadingChild, T(*loading_transform_class), {T(int_class)});
  add(*loading_transform_class, "SetParent", &LoadingSetParent, T(void_class), {T(*loading_transform_class), T(bool_class)});
  add(*loading_transform_class, "SetAsFirstSibling", &LoadingFirst, T(void_class), {});
  add(*loading_transform_class, "set_localEulerAngles", &LoadingVector3, T(void_class), {T(vec3_class)});
  add(*loading_transform_class, "set_localScale", &LoadingVector3, T(void_class), {T(vec3_class)});
  add(*loading_rect_class, "set_anchorMin", &LoadingAnchorMin, T(void_class), {T(vector)});
  add(*loading_rect_class, "set_anchorMax", &LoadingAnchorMax, T(void_class), {T(vector)});
  for (const auto* name : {"set_pivot", "set_sizeDelta", "set_anchoredPosition"})
    add(*loading_rect_class, name, &LoadingVector2, T(void_class), {T(vector)});
  add(*loading_image_class, "set_sprite", &LoadingSprite, T(void_class), {T(*loading_sprite_class)});
  add(*loading_image_class, "set_overrideSprite", &LoadingSprite, T(void_class), {T(*loading_sprite_class)});
  add(*loading_image_class, "set_color", &LoadingColor, T(void_class), {T(color)});
  add(*loading_image_class, "set_type", &LoadingImageType, T(void_class), {T(int_class)});
  add(*loading_image_class, "set_preserveAspect", &LoadingBool, T(void_class), {T(bool_class)});
  add(*loading_image_class, "set_raycastTarget", &LoadingBool, T(void_class), {T(bool_class)});
  add(*loading_image_class, "SetVerticesDirty", &LoadingFirst, T(void_class), {});
  for (auto* cls : {loading_image_class, loading_rect_class, loading_transform_class})
    for (auto* method : cls->methods)
      if (std::string_view(method->name).starts_with("set_"))
        AddProperty(*cls, method->name + 4, nullptr, method);
  add(behaviour, "set_enabled", &LoadingEnabled, T(void_class), {T(bool_class)});
  // SetEnabled uses the existing property setter rather than method Call.
  AddProperty(behaviour, "enabled", nullptr, behaviour.methods.back());
  add(*loading_texture_class, ".ctor", &LoadingTextureCtor, T(void_class),
      {T(int_class), T(int_class), T(int_class), T(bool_class)});
  add(*loading_texture_class, "get_width", &LoadingWidth, T(int_class), {}, true);
  add(*loading_texture_class, "get_height", &LoadingHeight, T(int_class), {}, true);
  add(conversion, "LoadImage", &LoadingDecode, T(bool_class), {T(*loading_texture_class), T(bytes), T(bool_class)},
      false, true);
  add(*loading_sprite_class, "Create", &LoadingSpriteCreate, T(*loading_sprite_class),
      {T(*loading_texture_class), T(rect), T(vector)}, false, true);
  add(screen, "get_width", &LoadingScreenWidth, T(int_class), {}, true, true);
  add(screen, "get_height", &LoadingScreenHeight, T(int_class), {}, true, true);
  // Name() calls the Object property. Its fixture initially supplies only the method.
  for (auto* method : unity_object_class.methods)
    if (std::string_view(method->name) == "get_name")
      AddProperty(unity_object_class, "name", method);
  tow_object_new_override = &LoadingAllocate;
  type_object_override = &LoadingType;
  auto* canvas_go = LoadingGo("Canvas");
  auto* canvas = LoadingNode(&loading_canvas_class->klass);
  canvas->object = canvas_go;
  canvas->transform = canvas_go->transform;
  canvas_go->canvas = canvas;
  auto* bg = LoadingGo("Background");
  bg->transform->parent = canvas_go->transform;
  canvas_go->transform->children.push_back(bg->transform);
  auto reflected_image = LoadingType(T(*loading_image_class));
  LoadingAddComponent(&bg->header, reinterpret_cast<Il2CppReflectionType*>(reflected_image), nullptr);
  auto* login_node = LoadingNode(&login.klass);
  login_node->canvas = canvas;
  auto* tvc = LoadingNode(&transition.klass);
  tvc->transform = canvas_go->transform;
  tvc->background = bg->image;
  tvc->animator = LoadingNode(&behaviour.klass);
  auto* slideshow = LoadingNode(&slide.klass);
  slideshow->image = bg->image;
  auto* login_view = reinterpret_cast<LoginSequence*>(login_node);
  auto* transition_view = reinterpret_cast<TransitionViewController*>(tvc);
  auto* slide_view = reinterpret_cast<SlideShowViewController*>(slideshow);
  auto& config = Config::Get().graphics;
  int originals = 0;
  auto login_original = [&](LoginSequence*) { ++originals; };
  auto transition_original = [&](TransitionViewController*) { ++originals; };
  auto slide_original = [&](SlideShowViewController*) { ++originals; };
  screen_images = {};
  transition_screen = {};
  if (mode == "loading-login-consumer") {
    config.loader_enabled = false;
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(originals == 1 && loading_created == 0 && loading_decodes == 0,
            "disabled login calls original and acquires no artwork");
    config.loader_enabled = true;
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(originals == 2 && loading_created == 2,
            "login creates both logos even when the build keeps the original background");
    const auto decodes = loading_decodes;
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(loading_decodes > decodes && loading_created == 4,
            "new login generation releases the live asset cache and recreates both scene logos");
    const auto custom_path = std::filesystem::temp_directory_path()
                             / File::FromUtf8("stfc-loading-\xc3\xa9-"
                                 + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".img");
    struct LoadingFixtureFile {
      std::filesystem::path path;
      ~LoadingFixtureFile() { std::error_code error; std::filesystem::remove(path, error); }
    } custom{custom_path};
    const auto utf8 = custom_path.u8string();
    config.loader_image.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    { std::ofstream file(custom_path, std::ios::binary); file.put(42); }
    auto input = loading_decode_inputs.size();
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(loading_decode_inputs[input] == 42 && bg->image->sprite,
            "actual login consumer reads a Unicode custom path, including original-background builds");
    { std::ofstream file(custom_path, std::ios::binary); file.put(43); }
    input = loading_decode_inputs.size();
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(loading_decode_inputs[input] == 43,
            "a changed file is decoded on the next login generation rather than reusing the old rooted texture");
    { std::ofstream file(custom_path, std::ios::binary); file.put(0); }
    auto* prior_sprite = bg->image->sprite;
    input = loading_decode_inputs.size();
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(loading_decode_inputs[input] == 0,
            "invalid custom input reaches the decoder rather than being silently cached");
#ifdef _USE_ORIGINAL_BG
    Require(bg->image->sprite == prior_sprite,
            "original-background builds preserve native artwork after a failed custom decode");
#else
    Require(loading_decode_inputs[input + 1] == g_embeddedLoadingImage[0] && bg->image->sprite != prior_sprite,
            "failed custom decode falls back to the production embedded background");
#endif
    config.loader_image.clear();
    loading_fail_decode = true;
    const auto creates = loading_created;
    LoginSequence_Awake_Hook(login_original, login_view);
    Require(loading_created == creates && originals == 7,
            "decoder failure preserves the native login path without constructing empty overlays");
  } else {
    config.loader_transition = false;
    TransitionViewController_Awake_Hook(transition_original, transition_view);
    Require(originals == 1 && loading_created == 0 && tvc->animator->enabled,
            "disabled transition keeps native artwork and animation");
    config.loader_transition = true;
    config.loader_transition_black = false;
    loading_fail_image = true;
    TransitionViewController_Awake_Hook(transition_original, transition_view);
#ifndef _USE_ORIGINAL_BG
    Require(!transition_screen.overlay && !transition_screen.customized && loading_destroys == 1
                && tvc->animator->enabled && bg->image->color.a == 1,
            "failed background construction destroys its provisional object and preserves native visuals/animation");
    SlideShowViewController_ShowCurrentSlide_Hook(slide_original, slide_view);
    Require(bg->image->color.a == 1, "slideshow is not hidden without a successful background replacement");
    loading_fail_image = false;
    loading_fail_parent = true;
    TransitionViewController_AboutToShow_Hook(transition_original, transition_view);
    Require(loading_destroys == 2 && !transition_screen.overlay && tvc->animator->enabled,
            "failed parenting destroys its provisional GameObject and remains retryable");
    loading_fail_parent = false;
    loading_fail_color = true;
    TransitionViewController_AboutToShow_Hook(transition_original, transition_view);
    Require(loading_destroys == 3 && !transition_screen.overlay && tvc->animator->enabled,
            "failed image configuration destroys the already-parented overlay without disabling native animation");
    loading_fail_color = false;
    TransitionViewController_AboutToShow_Hook(transition_original, transition_view);
    Require(transition_screen.overlay && transition_screen.customized && !tvc->animator->enabled
                && bg->image->color.a == 0 && loading_created == 6,
            "AboutToShow retries failed construction and then applies background plus both logos");
    const auto creates = loading_created;
    TransitionViewController_AboutToShow_Hook(transition_original, transition_view);
    Require(loading_created == creates, "successful transition customization is idempotent for its controller");
#else
    Require(!transition_screen.overlay && tvc->animator->enabled && bg->image->color.a == 1,
            "original-background build preserves native transition visuals and animation");
    loading_fail_image = false;
#endif
    TransitionViewController_AboutToHide_Hook(transition_original, transition_view);
    Require(tvc->animator->enabled, "AboutToHide re-enables only the animator this controller customized");
    auto* replacement = LoadingNode(&transition.klass);
    replacement->transform = tvc->transform;
    replacement->background = tvc->background;
    replacement->animator = LoadingNode(&behaviour.klass);
    config.loader_transition_black = true;
    const auto decodes = loading_decodes;
    TransitionViewController_AboutToShow_Hook(transition_original,
        reinterpret_cast<TransitionViewController*>(replacement));
    Require(transition_screen.owner.Get() == &replacement->header && !transition_screen.overlay
                && !replacement->animator->enabled && loading_decodes >= decodes,
            "a replacement controller does not inherit the former overlay/complete flags; black mode keeps its background");
    MonoSingleton_PrepareAllForReload_Hook([&] { ++originals; });
    Require(!transition_screen.owner && !transition_screen.overlay && tip_picker.first_screen,
            "reload forgets former scene ownership and resets the welcome tip before native reload");
  }
  loading_fail_decode = false;
  auto* asset = screen_images.Logo();
  Require(asset && asset->Sprite(), "failure checks have a successfully decoded native logo");
  const auto roots = warp_roots.size(), destroys = static_cast<size_t>(loading_destroys);
  loading_fail_ctor = true;
  Require(!CreateOverlay("FailedLoadingConstructor", *asset,
                         reinterpret_cast<unity::Transform*>(canvas_go->transform), {}, {1, 1}, false, false)
              && warp_roots.size() == roots && static_cast<size_t>(loading_destroys) == destroys,
          "a contained GameObject constructor failure is not published and releases its managed root");
  loading_fail_ctor = false;
  warp_root_failure = true;
  Require(!CreateOverlay("FailedLoadingCustody", *asset,
                         reinterpret_cast<unity::Transform*>(canvas_go->transform), {}, {1, 1}, false, false)
              && warp_roots.size() == roots && static_cast<size_t>(loading_destroys) == destroys,
          "failed overlay root custody stops before constructing a native GameObject");
  warp_root_failure = false;
  screen_images = {};
  transition_screen = {};
  tow_object_new_override = nullptr;
  type_object_override = nullptr;
  std::cout << "Loading artwork production consumer passed: " << mode << '\n';
}
} // namespace
