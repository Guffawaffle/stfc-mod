#pragma once

// Production Unity adapter, with managed boxing/transform calls supplied by the controlled runtime.
// This is not a rendered pin band or native pointer-dispatch qualification.
namespace
{
struct PinGeometryTransform {
  Il2CppObject   header;
  unity::Vector3 scale{1, 1, 1}, offset{};
  unity::Rect    rect{};
};
PinGeometryTransform pin_geometry_child{}, pin_geometry_root{};
const MethodInfo *   pin_geometry_rect = nullptr, *pin_geometry_point = nullptr, *pin_geometry_inverse = nullptr;
Il2CppObject         pin_geometry_rect_box{};
unity::Rect          pin_geometry_rect_value{};
int                  pin_geometry_calls     = 0;
const MethodInfo*    pin_geometry_failure   = nullptr;
bool                 pin_geometry_wrong_box = false, pin_geometry_null_box = false, pin_geometry_box_refusal = false;

bool PinGeometryRooted(const void* object)
{
  return std::ranges::any_of(warp_roots, [object](const auto& item) { return item.second == object; });
}

Il2CppObject* PinGeometryInvoke(const MethodInfo* method, void* receiver, void** args, Il2CppException** exception,
                                bool& handled)
{
  handled = method == pin_geometry_rect || method == pin_geometry_point || method == pin_geometry_inverse;
  if (!handled)
    return nullptr;
  ++pin_geometry_calls;
  Require(PinGeometryRooted(&pin_geometry_child) && PinGeometryRooted(&pin_geometry_root),
          "both native geometry owners remain rooted across managed invocations");
  if (method == pin_geometry_failure) {
    *exception = &thrown;
    return nullptr;
  }
  if (pin_geometry_wrong_box)
    return &string_object;
  if (pin_geometry_null_box)
    return nullptr;
  if (method == pin_geometry_rect) {
    Require(receiver == &pin_geometry_child, "only the card's own rect is measured");
    pin_geometry_rect_value = pin_geometry_child.rect;
    if (pin_geometry_box_refusal)
      warp_root_failure = true;
    return &pin_geometry_rect_box;
  }
  Require(PinGeometryRooted(&pin_geometry_rect_box) && args && args[0],
          "the checked rectangle box remains rooted while projecting its corners");
  const auto point = *static_cast<unity::Vector3*>(args[0]);
  if (method == pin_geometry_point) {
    Require(receiver == &pin_geometry_child, "local card corners project through the card transform");
    const auto& c         = pin_geometry_child;
    warp_vector_box.value = {point.x * c.scale.x + c.offset.x, point.y * c.scale.y + c.offset.y,
                             point.z * c.scale.z + c.offset.z};
  } else {
    Require(receiver == &pin_geometry_root, "world corners project into the current content transform");
    const auto& r         = pin_geometry_root;
    warp_vector_box.value = {(point.x - r.offset.x) / r.scale.x, (point.y - r.offset.y) / r.scale.y,
                             (point.z - r.offset.z) / r.scale.z};
  }
  return &warp_vector_box.header;
}

void PinGeometryCases(std::string_view mode)
{
  auto& transform      = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &component_class);
  auto& rect_transform = Define("UnityEngine", "RectTransform", IL2CPP_TYPE_CLASS, &transform);
  auto& rect           = Define("UnityEngine", "Rect", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Rect));
  auto& foreign_rect   = Define("PinFixture", "ForeignRect", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Rect));
  auto& foreign_vector = Define("PinFixture", "ForeignVector", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector3));
  auto& rect_getter    = AddMethod(rect_transform, "get_rect", reinterpret_cast<void*>(&Never),
                                   T(mode == "pin-geometry-foreign-rect" ? foreign_rect : rect), {});
  auto& point          = AddMethod(transform, "TransformPoint", reinterpret_cast<void*>(&Never), T(vec3_class),
                                   {T(mode == "pin-geometry-foreign-param" ? foreign_vector : vec3_class)});
  auto& inverse = AddMethod(transform, "InverseTransformPoint", reinterpret_cast<void*>(&Never),
                            T(mode == "pin-geometry-foreign-result" ? foreign_vector : vec3_class), {T(vec3_class)});
  for (auto* method : {&rect_getter, &point, &inverse})
    method->invoker_method = &WarpInvokerSentinel;
  if (mode == "pin-geometry-no-invoker")
    inverse.invoker_method = nullptr;
  pin_geometry_rect           = &rect_getter;
  pin_geometry_point          = &point;
  pin_geometry_inverse        = &inverse;
  pin_geometry_rect_box.klass = &rect.klass;
  extra_value_box             = &pin_geometry_rect_box;
  extra_value_payload         = &pin_geometry_rect_value;
  pin_geometry_child          = {{&rect_transform.klass}, {2, 3, 1}, {300, 200, 0}, {-50, -20, 100, 40}};
  pin_geometry_root           = {{&transform.klass}, {2, 2, 1}, {100, 100, 0}};
  tow_runtime_invoke_override = &PinGeometryInvoke;
  auto measure                = [] {
    return unity::RectTransformUtility::OwnBounds(reinterpret_cast<unity::Transform*>(&pin_geometry_root),
                                                  reinterpret_cast<unity::RectTransform*>(&pin_geometry_child));
  };
  if (mode != "pin-geometry-native") {
    Require(!unity::RectTransformUtility::OwnBoundsAvailable() && !measure() && pin_geometry_calls == 0
                && warp_roots.empty(),
            "foreign geometry metadata or missing invoker refuses before native observation");
    return;
  }
  Require(unity::RectTransformUtility::OwnBoundsAvailable(), "qualified own-card methods are available");
  const auto bounds = measure();
  Require(bounds && bounds.value.center.x == 100 && bounds.value.center.y == 50 && bounds.value.extents.x == 50
              && bounds.value.extents.y == 30 && pin_geometry_calls == 5 && warp_roots.empty(),
          "own-card bounds follow scaled/translated card and content coordinates without descendant traversal");
  pin_geometry_root.scale.x = -2;
  const auto flipped        = measure();
  Require(flipped && flipped.value.center.x == -100 && flipped.value.extents.x == 50,
          "opposite corner ordering preserves the same source min/max union");
  pin_geometry_root.scale.x = 2;
  for (auto* method : {pin_geometry_rect, pin_geometry_point, pin_geometry_inverse}) {
    pin_geometry_failure = method;
    Require(!measure() && warp_roots.empty(), "each contained managed failure refuses geometry and releases roots");
  }
  pin_geometry_failure        = nullptr;
  pin_geometry_rect_box.klass = &foreign_rect.klass;
  Require(!measure() && warp_roots.empty(), "same-size foreign boxed rectangle is unavailable");
  pin_geometry_rect_box.klass = &rect.klass;
  pin_geometry_wrong_box      = true;
  Require(!measure() && warp_roots.empty(), "foreign reference box cannot become an observed rectangle");
  pin_geometry_wrong_box = false;
  pin_geometry_null_box  = true;
  Require(!measure() && warp_roots.empty(), "successful null boxing is unavailable geometry");
  pin_geometry_null_box   = false;
  warp_root_failure       = true;
  const auto calls_before = pin_geometry_calls;
  Require(!measure() && pin_geometry_calls == calls_before && warp_roots.empty(), "failed owner custody cannot invoke");
  warp_root_failure        = false;
  pin_geometry_box_refusal = true;
  Require(!measure() && warp_roots.empty(), "failed box custody refuses the geometry");
  pin_geometry_box_refusal = warp_root_failure = false;
  pin_geometry_child.rect.width                = std::numeric_limits<float>::infinity();
  Require(!measure() && warp_roots.empty(), "nonfinite rectangle is not projected");
  pin_geometry_child.rect.width = 100;
  pin_geometry_root.scale.x     = 0;
  Require(!measure() && warp_roots.empty(), "nonfinite transformed corner cannot authorize a band");
  pin_geometry_root.scale.x   = 2;
  pin_geometry_child.offset.x = 3'000'000;
  Require(!measure() && warp_roots.empty(), "frozen coordinate range guard remains in force");
  pin_geometry_child.offset.x = 300;
  Require(measure() && warp_roots.empty(), "geometry can recover after unavailable observations");
  tow_runtime_invoke_override = nullptr;
  extra_value_box             = nullptr;
  extra_value_payload         = nullptr;
  std::cout << "Pin own-card geometry adapter: source coordinates, typed boxes, failures and custody passed\n";
}
} // namespace
