#pragma once

// Reuse the existing controlled Unity scene. The installer name is isolated from the ship-tech donor stub;
// these cases execute presentation/cleanup helpers, not hooks, native rendering, pointer routing or persistence.
#define EnsureShipTileBinding PinPresentationEnsureShipTileBinding
#include "../src/mod/features/ship_pin_input.cc"
#undef EnsureShipTileBinding

namespace ship_pins
{
bool InputReady()
{ return false; }
bool IsPinned(FleetPlayerData*)
{ return false; }
bool Toggle(FleetPlayerData*, ShipManagementViewContext*)
{ return false; }
bool Place(FleetPlayerData*, std::optional<int64_t>, ShipManagementViewContext*)
{ return false; }
} // namespace ship_pins

namespace pin_presentation_fixture
{
struct Display {
  unity::Vector2 size{}, position{}, pivot{};
  unity::Rect    rect{0, 0, 300, 140};
  bool           maskable = true, excluded = false, raycast = true;
  float          font_size = 0;
};
std::map<LoadingFixtureNode*, Display>             display;
std::map<LoadingFixtureNode*, LoadingFixtureNode*> layouts, labels;
FakeClass *                                        layout_class, *text_class;
bool                                               fail_mask = false;

Il2CppObject* Component(Il2CppObject* object, Il2CppReflectionType* type, const MethodInfo* method)
{
  auto* node  = LoadingNative(object);
  auto* klass = reinterpret_cast<LoadingFixtureType*>(type)->type;
  if (klass == &layout_class->klass)
    return reinterpret_cast<Il2CppObject*>(layouts[node]);
  if (klass == &text_class->klass)
    return reinterpret_cast<Il2CppObject*>(labels[node]);
  return LoadingGetComponent(object, type, method);
}
Il2CppObject* AddComponent(Il2CppObject* object, Il2CppReflectionType* type, const MethodInfo*)
{
  auto* node  = LoadingNative(object);
  auto* klass = reinterpret_cast<LoadingFixtureType*>(type)->type;
  if (klass == &loading_rect_class->klass) {
    node->transform->header.klass = klass;
    return &node->transform->header;
  }
  auto* component      = LoadingNode(klass);
  component->object    = node;
  component->transform = node->transform;
  if (klass == &loading_image_class->klass)
    node->image = component;
  else if (klass == &layout_class->klass)
    layouts[node] = component;
  else if (klass == &text_class->klass)
    labels[node] = component;
  else
    throw std::logic_error("unexpected pin presentation component");
  return &component->header;
}
Il2CppObject* Find(Il2CppObject* receiver, Il2CppString* name, const MethodInfo*)
{
  const auto expected = il2cpp::ToUtf8(name);
  for (auto* child : LoadingNative(receiver)->children)
    if (child->object && child->object->name == expected)
      return &child->header;
  return nullptr;
}
void Active(Il2CppObject* object, bool value, const MethodInfo*)
{ LoadingNative(object)->enabled = value; }
void Size(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ display[LoadingNative(object)].size = value; }
void Position(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ display[LoadingNative(object)].position = value; }
void Pivot(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ display[LoadingNative(object)].pivot = value; }
void Mask(Il2CppObject* object, bool value, const MethodInfo*)
{
  if (fail_mask)
    throw Il2CppExceptionWrapper(&thrown);
  display[LoadingNative(object)].maskable = value;
}
void Raycast(Il2CppObject* object, bool value, const MethodInfo*)
{ display[LoadingNative(object)].raycast = value; }
void Exclude(Il2CppObject* object, bool value, const MethodInfo*)
{ display[LoadingNative(object)].excluded = value; }
bool Excluded(Il2CppObject* object, const MethodInfo*)
{ return display[LoadingNative(object)].excluded; }
void FontSize(Il2CppObject* object, float value, const MethodInfo*)
{ display[LoadingNative(object)].font_size = value; }
void        Alignment(Il2CppObject*, int32_t, const MethodInfo*) {}
unity::Rect Rectangle(Il2CppObject* receiver, const MethodInfo*)
{ return display[LoadingNative(receiver)].rect; }

void Replace(FakeClass& cls, std::string_view name, void* function)
{
  for (auto* method : cls.methods)
    if (method->name == name) {
      auto* entry          = const_cast<MethodInfo*>(method);
      entry->methodPointer = entry->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(function);
      return;
    }
  throw std::logic_error("fixture method not found");
}
unity::RectTransform* Rect(std::string name)
{
  auto* object                    = LoadingGo(std::move(name));
  object->transform->header.klass = &loading_rect_class->klass;
  return reinterpret_cast<unity::RectTransform*>(object->transform);
}
LoadingFixtureNode* Node(unity::Transform* value)
{ return reinterpret_cast<LoadingFixtureNode*>(value); }
bool Visible(unity::Transform* value)
{ return value && Node(value)->object->enabled; }

void Run()
{
  // This existing case initializes and qualifies the shared managed scene before our presentation-only adapters.
  LoadingArtworkConsumerCases("loading-login-consumer");
  tow_object_new_override = &LoadingAllocate;
  type_object_override    = &LoadingType;
  layout_class            = &Define("UnityEngine.UI", "LayoutElement", IL2CPP_TYPE_CLASS, &component_class);
  auto& tmp               = Define("TMPro", "TMP_Text", IL2CPP_TYPE_CLASS, &component_class);
  text_class              = &Define("TMPro", "TextMeshProUGUI", IL2CPP_TYPE_CLASS, &tmp);
  auto find_type          = [](std::string_view name) -> FakeClass& {
    const auto found = std::ranges::find_if(classes, [&](const FakeClass& cls) {
      return cls.klass.namespaze == std::string_view("UnityEngine") && cls.klass.name == name;
    });
    Require(found != classes.end(), "shared Unity fixture type is present");
    return *found;
  };
  auto& color = find_type("Color");
  auto  add   = [](FakeClass& cls, const char* name, auto fn, const Il2CppType* result,
                   std::vector<const Il2CppType*> args) -> const MethodInfo& {
    return AddMethod(cls, name, reinterpret_cast<void*>(fn), result, std::move(args));
  };
  Replace(game_object_class, "GetComponent", reinterpret_cast<void*>(&Component));
  Replace(game_object_class, "AddComponent", reinterpret_cast<void*>(&AddComponent));
  Replace(*loading_rect_class, "set_sizeDelta", reinterpret_cast<void*>(&Size));
  Replace(*loading_rect_class, "set_anchoredPosition", reinterpret_cast<void*>(&Position));
  Replace(*loading_rect_class, "set_pivot", reinterpret_cast<void*>(&Pivot));
  Replace(*loading_image_class, "set_raycastTarget", reinterpret_cast<void*>(&Raycast));
  add(game_object_class, "SetActive", &Active, T(void_class), {T(bool_class)});
  add(*loading_transform_class, "Find", &Find, T(*loading_transform_class), {T(string_class)});
  add(*loading_transform_class, "SetAsLastSibling", &LoadingFirst, T(void_class), {});
  add(*loading_image_class, "set_maskable", &Mask, T(void_class), {T(bool_class)});
  const auto& excluded = add(*layout_class, "get_ignoreLayout", &Excluded, T(bool_class), {});
  const auto& exclude  = add(*layout_class, "set_ignoreLayout", &Exclude, T(void_class), {T(bool_class)});
  AddProperty(*layout_class, "ignoreLayout", &excluded, &exclude);
  for (const auto* name : {"set_raycastTarget", "set_enableWordWrapping"})
    add(tmp, name, &LoadingBool, T(void_class), {T(bool_class)});
  add(tmp, "set_color", &LoadingColor, T(void_class), {T(color)});
  add(tmp, "set_text", &LoadingText, T(void_class), {T(string_class)});
  add(tmp, "set_fontSize", &FontSize, T(void_class), {T(float_class)});
  add(tmp, "set_alignment", &Alignment, T(void_class), {T(int_class)});
  for (auto* method : tmp.methods)
    AddProperty(tmp, method->name + 4, nullptr, method);
  auto& rect_type = find_type("Rect");
  add(*loading_rect_class, "get_rect", &Rectangle, T(rect_type), {});
  auto* content = Rect("Pin content");
  auto* band    = PinSurface(content, kPinBand, {0.5f, 0.5f}, {0.5f, 0.5f}, {200, 100}, {}, {}, true);
  Require(band && PinStyleBand(band, false, false), "native pin group presentation is created");
  auto* top = PinChild(band, kPinGroupEdges[0]);
  Require(top && !display[Node(top)->object->image].maskable && display[Node(top)].size.y == 3
              && Node(band)->object->image->color.a == 0.72f,
          "normal group has a complete unmasked three-unit rim and the frozen clipped fill");
  Require(PinStyleBand(band, true, false) && display[Node(top)].size.y == 4.5f && PinStyleBand(band, true, true)
              && display[Node(top)].size.y == 5,
          "drag then hover update the existing native group edges");
  PinInput().band          = il2cpp::GcHandle::Weak(band);
  PinInput().group_hovered = true;
  auto* tile               = Rect("Pin tile");
  PinStyleTile(tile, PinHighlightKind::Source);
  auto* drag_top = PinChild(tile, kPinDragEdges[0]);
  Require(Visible(drag_top) && display[Node(drag_top)].size.y == 4.5f
              && display[layouts[Node(drag_top)->object]].excluded,
          "armed source shows gold card edges excluded from native layout");
  PinStyleTile(tile, PinHighlightKind::UnpinTarget);
  Require(Node(drag_top)->object->image->color.r == 1 && Node(drag_top)->object->image->color.g == 0.43f,
          "unpin target replaces source feedback on the same reused native card");
  PinStyleTile(tile, PinHighlightKind::None);
  Require(!Visible(drag_top), "tile reset hides prior drag edges");
  auto* viewport = Rect("Pin viewport");
  PinDrawCue(viewport, false);
  auto* cue   = PinChild(viewport, kPinCue);
  auto* label = PinChild(cue, kPinCueLabel);
  auto* edge  = PinChild(viewport, kPinCueEdge);
  Require(Visible(cue) && Visible(edge) && label && labels[Node(label)->object]->text == "Pin here"
              && display[labels[Node(label)->object]].font_size == 13 && display[Node(cue)].size.x == 90
              && display[Node(cue)].size.y == 90,
          "first pin cue creates the source label, compact box and independent leading edge");
  display[Node(viewport)].rect = {0, 0, 64, 40};
  PinDrawCue(viewport, true);
  Require(display[Node(cue)].size.x == 56 && display[Node(cue)].size.y == 32
              && Node(cue)->object->image->color.a == 0.98f,
          "reused cue fits a smaller viewport and receives hover feedback");
  auto* replacement = Rect("Replacement viewport");
  PinDrawCue(replacement, false);
  Require(!Visible(cue) && !Visible(edge) && Visible(PinChild(replacement, kPinCue)),
          "viewport replacement retires both decorations under the former owner");
  PinReleaseRoots();
  Require(!Visible(PinChild(replacement, kPinCue)) && !Visible(PinChild(replacement, kPinCueEdge))
              && display[Node(top)].size.y == 3 && !PinInput().band && !PinInput().cue_viewport,
          "gesture cleanup restores group style and hides cue plus edge while releasing weak custody");
  display[Node(replacement)].rect = {0, 0, 8, 40};
  PinDrawCue(replacement, false);
  Require(!Visible(PinChild(replacement, kPinCue)), "invalid viewport cannot retain a visible first-pin invitation");
  fail_mask = true;
  Require(!PinStyleBand(band, true, false) && !Visible(band),
          "contained maskability failure hides an incomplete group decoration");
  fail_mask         = false;
  warp_root_failure = true;
  PinDrawCue(viewport, false);
  warp_root_failure = false;
  Require(!Visible(cue) && !Visible(edge) && !PinInput().cue,
          "failed weak viewport custody cannot recreate an actionable cue");
  std::cout << "Pin production presentation transitions, layout, cue ownership and cleanup passed\n";
}
} // namespace pin_presentation_fixture
