#pragma once

// Production backdrop callbacks and binding resolution over controlled metadata.
// Native scene creation, Unity rendering and real hook installation are not exercised.
namespace backdrop_fixture
{
struct TransformObject {
  Il2CppObject   header;
  unity::Vector3 scale{2, 3, 4};
  int            reads = 0, attempts = 0, writes = 0;
  bool           fail_read = false, fail_write = false;
};
struct ComponentObject {
  Il2CppObject      header;
  unity::Transform* transform = nullptr;
};
struct PopulationObject {
  Il2CppObject  header;
  Il2CppObject* variation = nullptr;
};
struct ViewObject {
  Il2CppObject         header;
  PopulatedSystemData* population   = nullptr;
  unity::Component*    renderable   = nullptr;
  int                  getter_calls = 0;
};
const MethodInfo* scale_method = nullptr;
bool              null_box = false, wrong_box = false;

Il2CppObject* Invoke(const MethodInfo* method, void* target, void**, Il2CppException** exception, bool& handled)
{
  if (method != scale_method)
    return nullptr;
  handled      = true;
  auto* object = static_cast<TransformObject*>(target);
  ++object->reads;
  if (object->fail_read) {
    *exception = &thrown;
    return nullptr;
  }
  warp_vector_box.value = object->scale;
  return null_box ? nullptr : wrong_box ? &string_object : &warp_vector_box.header;
}

unity::Transform* Transform(unity::Component* component, const MethodInfo*)
{ return reinterpret_cast<ComponentObject*>(component)->transform; }
unity::Vector3 Scale(unity::Transform* transform, const MethodInfo*)
{
  auto* object = reinterpret_cast<TransformObject*>(transform);
  ++object->reads;
  if (object->fail_read)
    throw Il2CppExceptionWrapper(&thrown);
  return object->scale;
}
void SetScale(unity::Transform* transform, unity::Vector3 value, const MethodInfo*)
{
  auto* object = reinterpret_cast<TransformObject*>(transform);
  ++object->attempts;
  if (object->fail_write)
    throw Il2CppExceptionWrapper(&thrown);
  object->scale = value;
  ++object->writes;
}
unity::Component* FlatRenderable(PlanetViewUtils* view, const MethodInfo*)
{
  return PlanetViewUtils_get_FlatRenderable_Hook(
      [](PlanetViewUtils* current) {
        auto* object = reinterpret_cast<ViewObject*>(current);
        ++object->getter_calls;
        return object->renderable;
      },
      view);
}
bool Equal(unity::Vector3 a, unity::Vector3 b)
{ return a.x == b.x && a.y == b.y && a.z == b.z; }

void Cases(std::string_view mode)
{
  const bool missing         = mode == "camera-backdrop-missing-scale";
  const bool foreign         = mode == "camera-backdrop-foreign-scale";
  auto&      transform_class = Define("UnityEngine", "Transform", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto&      view_class      = Define("Digit.Prime.Navigation", "PlanetViewUtils", IL2CPP_TYPE_CLASS, &object_class);
  auto& population_class = Define("Digit.Prime.Navigation", "PopulatedSystemData", IL2CPP_TYPE_CLASS, &object_class);
  auto& transform =
      AddMethod(component_class, "get_transform", reinterpret_cast<void*>(&Transform), T(transform_class), {});
  AddProperty(component_class, "transform", &transform);
  const MethodInfo* get_scale = nullptr;
  if (!missing) {
    auto& getter          = AddMethod(transform_class, "get_localScale", reinterpret_cast<void*>(&Scale),
                                      T(foreign ? foreign_vector_class : vec3_class), {});
    getter.invoker_method = &WarpInvokerSentinel;
    get_scale = scale_method = &getter;
  }
  auto& set_scale =
      AddMethod(transform_class, "set_localScale", reinterpret_cast<void*>(&SetScale), T(void_class), {T(vec3_class)});
  AddProperty(transform_class, "localScale", get_scale, &set_scale);
  auto& flat =
      AddMethod(view_class, "get_FlatRenderable", reinterpret_cast<void*>(&FlatRenderable), T(component_class), {});
  AddProperty(view_class, "FlatRenderable", &flat);
  AddField(view_class, "_popData", population_class, offsetof(ViewObject, population));
  AddField(population_class, "<VariationCollection>k__BackingField", object_class,
           offsetof(PopulationObject, variation));

  TransformObject model{{&transform_class.klass}};
  ComponentObject renderable{{&component_class.klass}, reinterpret_cast<unity::Transform*>(&model)};
  ViewObject      view{{&view_class.klass}, nullptr, reinterpret_cast<unity::Component*>(&renderable)};
  auto*           component       = reinterpret_cast<unity::Component*>(&renderable);
  auto*           receiver        = reinterpret_cast<PlanetViewUtils*>(&view);
  Config::Get().graphics.fr_scale = 2;
  tow_runtime_invoke_override     = Invoke;

  if (missing || foreign) {
    ScaleFlatRenderable(component, false);
    Require(model.reads == 0 && model.attempts == 0 && Equal(model.scale, {2, 3, 4}),
            "unavailable or foreign local-scale metadata cannot become a zero transform write");
  } else if (mode == "camera-backdrop-null-box" || mode == "camera-backdrop-wrong-box") {
    null_box  = mode == "camera-backdrop-null-box";
    wrong_box = mode == "camera-backdrop-wrong-box";
    ScaleFlatRenderable(component, false);
    Require(model.reads == 1 && model.attempts == 0 && Equal(model.scale, {2, 3, 4}),
            "absent or foreign returned scale box cannot authorize a write");
    null_box = wrong_box = false;
    ScaleFlatRenderable(component, false);
    Require(model.writes == 1 && Equal(model.scale, {4, 6, 8}), "valid boxed scale recovers without a poisoned cache");
  } else if (mode == "camera-backdrop-failed-scale") {
    model.fail_read = true;
    ScaleFlatRenderable(component, false);
    Require(model.reads == 1 && model.attempts == 0 && Equal(model.scale, {2, 3, 4}),
            "failed scale observation leaves the authored transform and cache untouched");
    model.fail_read = false;
    ScaleFlatRenderable(component, false);
    Require(model.writes == 1 && Equal(model.scale, {4, 6, 8}), "a later available scale can still be applied");
  } else if (mode == "camera-backdrop-failed-set") {
    model.fail_write = true;
    ScaleFlatRenderable(component, false);
    Require(model.attempts == 1 && model.writes == 0 && Equal(model.scale, {2, 3, 4}),
            "failed native scale write leaves authored values intact");
    model.fail_write = false;
    ScaleFlatRenderable(component, false);
    Require(model.attempts == 2 && model.writes == 1 && Equal(model.scale, {4, 6, 8}),
            "failed scale write does not suppress its next retry");
  } else if (mode == "camera-backdrop-cache-cap") {
    std::deque<TransformObject> models;
    std::deque<ComponentObject> renderables;
    for (int i = 0; i < 300; ++i) {
      auto& entry = models.emplace_back(TransformObject{{&transform_class.klass}});
      auto& item  = renderables.emplace_back(
          ComponentObject{{&component_class.klass}, reinterpret_cast<unity::Transform*>(&entry)});
      ScaleFlatRenderable(reinterpret_cast<unity::Component*>(&item), false);
    }
    for (auto& item : renderables)
      ScaleFlatRenderable(reinterpret_cast<unity::Component*>(&item), false);
    Require(std::ranges::all_of(models,
                                [](const auto& entry) { return entry.writes == 1 && Equal(entry.scale, {4, 6, 8}); }),
            "more than 256 still-live transforms retain their own applied scale without compounding");
  } else if (mode == "camera-backdrop-reuse") {
    ScaleFlatRenderable(component, false);
    Require(Equal(model.scale, {4, 6, 8}), "first managed transform receives its authored multiplier");
    for (auto& [_, target] : warp_roots)
      if (target == &model.header)
        target = nullptr; // The old weak identity expires before this storage is reused.
    model = TransformObject{{&transform_class.klass}, {4, 5, 6}};
    ScaleFlatRenderable(component, false);
    Require(model.writes == 1 && Equal(model.scale, {8, 10, 12}),
            "recycled transform address cannot inherit the previous managed object's scale observation");
  } else if (mode == "camera-backdrop-nonfinite") {
    Config::Get().graphics.fr_scale = std::numeric_limits<float>::infinity();
    ScaleFlatRenderable(component, false);
    Require(model.attempts == 0 && Equal(model.scale, {2, 3, 4}), "infinite multiplier cannot write infinite scales");
    Config::Get().graphics.fr_scale = 2;
    model.scale.y                   = std::numeric_limits<float>::infinity();
    ScaleFlatRenderable(component, false);
    Require(model.attempts == 0 && model.scale.x == 2, "nonfinite authored scale is not a usable scaling observation");
  } else if (mode == "camera-backdrop-native") {
    ScaleFlatRenderable(nullptr, false);
    for (const float factor : {0.0f, -2.0f, 1.0f, std::numeric_limits<float>::quiet_NaN()}) {
      Config::Get().graphics.fr_scale = factor;
      ScaleFlatRenderable(component, false);
    }
    Require(model.attempts == 0 && model.reads == 0, "off, native and NaN choices leave authored scales alone");
    Config::Get().graphics.fr_scale = 2;
    ScaleFlatRenderable(component, false);
    ScaleFlatRenderable(component, false);
    Require(model.writes == 1 && Equal(model.scale, {4, 6, 8}), "repeated getter use does not compound one transform");
    TransformObject second{{&transform_class.klass}, {3, 4, 5}};
    ComponentObject other{{&component_class.klass}, reinterpret_cast<unity::Transform*>(&second)};
    ScaleFlatRenderable(reinterpret_cast<unity::Component*>(&other), false);
    ScaleFlatRenderable(component, false);
    Require(second.writes == 1 && Equal(second.scale, {6, 8, 10}) && model.writes == 1,
            "two different authored sizes keep independent applied-scale observations");
    TransformObject zero{{&transform_class.klass}, {0, 0, 0}};
    ComponentObject empty{{&component_class.klass}, reinterpret_cast<unity::Transform*>(&zero)};
    ScaleFlatRenderable(reinterpret_cast<unity::Component*>(&empty), false);
    Require(zero.reads == 1 && zero.writes == 1 && Equal(zero.scale, {}),
            "an observed zero scale remains distinct from an unavailable getter");
  } else {
    Require(mode == "camera-backdrop-callbacks", "known backdrop fixture mode");
    FlatRenderable(receiver, nullptr);
    Require(view.getter_calls == 1 && model.writes == 1 && Equal(model.scale, {4, 6, 8}),
            "production getter callback scales the native renderable and returns it");
    int zooms = 0;
    PlanetViewUtils_CameraZoomedEventHandler_Hook(
        [&](PlanetViewUtils* actual, float distance, float normalized) {
          Require(actual == receiver && distance == 15 && normalized == .25f && view.getter_calls == 1,
                  "native zoom event precedes the explicit renderable getter and keeps its parameters");
          ++zooms;
        },
        receiver, 15, .25f);
    Require(zooms == 1 && view.getter_calls == 2 && model.writes == 1,
            "zoom getter probe preserves already scaled content");

    Il2CppObject backdrop{}, object_resource{}, collection_resource{}, system{}, variation{};
    int          loads = 0;
    const auto   load  = [&](PlanetViewUtils* actual, Il2CppObject* tree, Il2CppObject* resource, bool using_default) {
      Require(actual == receiver && tree == &backdrop && using_default, "loader preserves its native arguments");
      ++loads;
      model.scale = resource == &collection_resource ? unity::Vector3{9, 10, 11} : unity::Vector3{7, 8, 9};
    };
    PlanetViewUtils_CreateBackdrop_Hook(load, receiver, &backdrop, &object_resource, true);
    Require(loads == 1 && model.writes == 2 && Equal(model.scale, {14, 16, 18}),
            "object loader scales newly authored content after native creation");
    PlanetViewUtils_CreateBackdrop_Hook(load, receiver, &backdrop, &collection_resource, true);
    Require(loads == 2 && model.writes == 3 && Equal(model.scale, {18, 20, 22}),
            "collection loader follows the same original-first authored-scale path");
    const auto before_null = view.getter_calls;
    PlanetViewUtils_CreateBackdrop_Hook(load, receiver, &backdrop, nullptr, true);
    Require(loads == 3 && model.writes == 3 && view.getter_calls == before_null,
            "null loader resource performs only native creation");

    PopulationObject first{{&population_class.klass}, &variation};
    PopulationObject prefab{{&population_class.klass}, nullptr};
    PopulationObject same{{&population_class.klass}, &variation};
    PopulationObject galaxy{{&population_class.klass}, &variation};
    PopulationObject nested{{&population_class.klass}, &variation};
    const auto       populate = [&](PopulationObject& population, unity::Vector3 authored, NodeDepth depth) {
      PlanetViewUtils_Populate_Hook(
          [&](PlanetViewUtils* actual, Il2CppObject* tree, int64_t node, NodeDepth native_depth) {
            Require(actual == receiver && tree == &system && node == 9007199254740993LL && native_depth == depth,
                    "Populate preserves its object, wide node ID and depth");
            view.population = reinterpret_cast<PopulatedSystemData*>(&population);
            model.scale     = authored;
          },
          receiver, &system, 9007199254740993LL, depth);
    };
    populate(first, {11, 12, 13}, NodeDepth::SolarSystem);
    Require(model.writes == 4 && Equal(model.scale, {22, 24, 26}), "cached variation reload scales authored content");
    populate(prefab, model.scale, NodeDepth::SolarSystem);
    Require(model.writes == 4 && Equal(model.scale, {22, 24, 26}),
            "cached prefab retains its existing scaled transform");
    view.population = reinterpret_cast<PopulatedSystemData*>(&same);
    populate(same, model.scale, NodeDepth::SolarSystem);
    Require(model.writes == 4, "unchanged population identity does not force another multiplication");
    populate(galaxy, model.scale, NodeDepth::Galaxy);
    Require(model.writes == 4, "non-system Populate leaves backdrop scaling to native behavior");
    PlanetViewUtils_Populate_Hook(
        [&](PlanetViewUtils* actual, Il2CppObject*, int64_t, NodeDepth) {
          view.population = reinterpret_cast<PopulatedSystemData*>(&nested);
          PlanetViewUtils_CreateBackdrop_Hook(load, actual, &backdrop, &object_resource, true);
        },
        receiver, &system, 9007199254740993LL, NodeDepth::SolarSystem);
    Require(model.writes == 5 && Equal(model.scale, {14, 16, 18}),
            "CreateBackdrop inside Populate consumes the authored scaling once, without a second Populate multiply");
  }
  tow_runtime_invoke_override = nullptr;
  std::cout << "Camera backdrop fixture passed: " << mode << '\n';
}
} // namespace backdrop_fixture

namespace
{
void CameraBackdropCases(std::string_view mode)
{ backdrop_fixture::Cases(mode); }
} // namespace
