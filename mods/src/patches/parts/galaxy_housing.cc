#include "galaxy_housing.h"
#include "config.h"

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
#include "embedded_station_housing.h"
#include "settings/native/ui_helpers.h"
#include <algorithm>
#include <cstdint>
#include <il2cpp/method_contract.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

namespace galaxy_housing
{
namespace
{
  using namespace mod_settings::native;
  namespace ui               = mod_settings::native::ui;
  constexpr float slot_width = 36.f;
  struct Vec2 {
    float x, y;
  };
  struct Color {
    float r, g, b, a;
  };
  struct Marker {
    Il2CppGCHandle owner = nullptr, object = nullptr;
  };
  // Explicit lifecycle cleanup only: Unity is unavailable during static destruction.
  std::unordered_map<void*, Marker> markers;
  std::vector<std::int64_t>         housing_ids;
  const MethodInfo*                 node_id = nullptr;
  bool                              ready = false, warned = false;

  Il2CppObject* Field(void* widget, std::size_t offset)
  { return *reinterpret_cast<Il2CppObject**>(static_cast<char*>(widget) + offset); }

  void Destroy(Il2CppObject* object)
  {
    if (!ui::Alive(object))
      return;
    SetActive(object, false);
    static auto* destroy =
        method_contract::Resolve(ui::UnityClass("Object"), "Destroy", true, "System.Void", {"UnityEngine.Object"});
    void* args[]{object};
    ui::Static(destroy, args);
  }

  Il2CppObject* GameObject(const char* name, Il2CppObject* parent, int layer)
  {
    Root  object(il2cpp_object_new(ui::UnityClass("GameObject")));
    Root  label(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(name)));
    auto* ctor = method_contract::Resolve(object.get()->klass, ".ctor", false, "System.Void", {"System.String"});
    void* args[]{label.get()};
    Invoke(ctor, object.get(), args);
    try {
      // Hide before joining the native row, including partial construction failures.
      SetActive(object.get(), false);
      ui::WithType(object.get(), "AddComponent", ui::UnityClass("RectTransform"));
      ui::Value(object.get(), "set_layer", layer);
      Root  transform(ui::UiCall(object.get(), "get_transform"));
      bool  world = false;
      void* parent_args[]{parent, &world};
      ui::UiCall(transform.get(), "SetParent", 2, parent_args);
      ui::Value(transform.get(), "set_anchorMin", Vec2{.5f, .5f});
      ui::Value(transform.get(), "set_anchorMax", Vec2{.5f, .5f});
      ui::Value(transform.get(), "set_pivot", Vec2{.5f, .5f});
    } catch (...) {
      try {
        Destroy(object.get());
      } catch (const std::exception&) {
      }
      throw;
    }
    return object.get();
  }

  Il2CppObject* HomeImage(Il2CppObject* home)
  {
    auto* image_class = ui::Class("UnityEngine.UI", "UnityEngine.UI", "Image");
    auto* method      = method_contract::Resolve(home->klass, "GetComponentInChildren", false, "UnityEngine.Component",
                                                 {"System.Type", "System.Boolean"});
    Root  type(reinterpret_cast<Il2CppObject*>(il2cpp_type_get_object(il2cpp_class_get_type(image_class))));
    bool  inactive = true;
    void* args[]{type.get(), &inactive};
    return Invoke(method, home, args);
  }
} // namespace

bool Available()
{ return ready; }
bool Enabled()
{ return ready && Config::Get().galaxy_station_housing; }

void Install(Il2CppClass* star)
{
  try {
    auto* context = il2cpp_class_get_field_from_name(star, "m_context");
    auto* home    = il2cpp_class_get_field_from_name(star, "_homeIcon");
    if (!context || context->offset != 0x50 || context->type->type != IL2CPP_TYPE_VALUETYPE || !home
        || home->offset != 0xd8 || !method_contract::Type(home->type, "UnityEngine.GameObject"))
      throw std::runtime_error("housing widget fields incompatible");
    node_id = method_contract::Resolve(il2cpp_class_from_type(context->type), "get_ID", false, "System.Int64", {});
    if (!node_id)
      throw std::runtime_error("housing node ID unavailable");
    const auto data =
        nlohmann::json::parse(g_embeddedStationHousing, g_embeddedStationHousing + g_embeddedStationHousing_SIZE);
    if (data.at("schema_version") != 1)
      throw std::runtime_error("housing dataset schema");
    const auto read = [](const auto& source) {
      std::vector<std::int64_t> ids;
      if (!source.is_array() || source.empty())
        throw std::runtime_error("housing dataset IDs");
      for (const auto& id : source) {
        if (!id.is_number_integer())
          throw std::runtime_error("housing dataset noninteger ID");
        const auto value = id.template get<std::int64_t>();
        if (value <= 0 || (!ids.empty() && value <= ids.back()))
          throw std::runtime_error("housing dataset ordering");
        ids.push_back(value);
      }
      return ids;
    };
    auto       housing = read(data.at("housing_system_ids"));
    const auto covered = read(data.at("covered_system_ids"));
    if (!std::includes(covered.begin(), covered.end(), housing.begin(), housing.end()))
      throw std::runtime_error("housing dataset coverage");
    housing_ids = std::move(housing);
    ready       = true;
    spdlog::info("[GalaxyHousing] ready: {} housing systems / {} covered", housing_ids.size(), covered.size());
  } catch (const std::exception& error) {
    spdlog::warn("[GalaxyHousing] unavailable: {}", error.what());
  }
}

void Hide(void* widget)
{
  const auto it = markers.find(widget);
  if (it == markers.end())
    return;
  try {
    auto* object = Target(it->second.object);
    if (ui::Alive(object))
      SetActive(object, false);
  } catch (const std::exception&) { /* Never interrupt the native draw or release. */
  }
}

void Remove(void* widget)
{
  const auto it = markers.find(widget);
  if (it == markers.end())
    return;
  try {
    Destroy(Target(it->second.object));
  } catch (const std::exception&) {
    Hide(widget);
  }
  Free(it->second.owner);
  Free(it->second.object);
  markers.erase(it);
}

void Update(void* widget, int detail)
{
  if (!Enabled() || detail != 1)
    return;
  try {
    Root id(Invoke(node_id, reinterpret_cast<Il2CppObject*>(static_cast<char*>(widget) + 0x50), nullptr));
    if (!id.get() || !method_contract::Type(il2cpp_class_get_type(id.get()->klass), "System.Int64"))
      throw std::runtime_error("housing node ID result");
    const auto system = *static_cast<std::int64_t*>(il2cpp_object_unbox(id.get()));
    if (!std::binary_search(housing_ids.begin(), housing_ids.end(), system))
      return;
    Root name(ui::UiCall(Field(widget, 0x80), "get_Target"));
    Root name_object(ui::UiCall(name.get(), "get_gameObject"));
    if (!Boolean(ui::UiCall(name_object.get(), "get_activeInHierarchy")))
      return;
    Root name_transform(ui::UiCall(name.get(), "get_transform"));
    Root row(ui::UiCall(name_transform.get(), "get_parent"));
    Root row_object(ui::UiCall(row.get(), "get_gameObject"));
    // Fail closed if this prefab has no horizontal row to reserve the slot.
    Root layout(ui::WithType(row_object.get(), "GetComponent",
                             ui::Class("UnityEngine.UI", "UnityEngine.UI", "HorizontalLayoutGroup")));
    if (!ui::Alive(layout.get()))
      return;
    auto it = markers.find(widget);
    if (it != markers.end() && (Target(it->second.owner) != widget || !ui::Alive(Target(it->second.object)))) {
      Remove(widget);
      it = markers.end();
    }
    if (it == markers.end()) {
      Root home(Field(widget, 0xd8));
      if (!ui::Alive(home.get()))
        return;
      Root image(HomeImage(home.get()));
      if (!ui::Alive(image.get()))
        return;
      Root sprite(ui::UiCall(image.get(), "get_sprite"));
      if (!ui::Alive(sprite.get()))
        return;
      Root      layer_value(ui::UiCall(home.get(), "get_layer"));
      const int layer = *static_cast<int*>(il2cpp_object_unbox(layer_value.get()));
      Root      object(GameObject("CommunityMod.StationHousing", row.get(), layer));
      it               = markers.emplace(widget, Marker{}).first;
      it->second.owner = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(widget), false);
      ui::Retain(it->second.object, object.get());
      if (!it->second.owner)
        throw std::runtime_error("housing owner root");
      Root transform(ui::UiCall(object.get(), "get_transform"));
      ui::Value(transform.get(), "set_sizeDelta", Vec2{slot_width, 32.f});
      Root element(
          ui::WithType(object.get(), "AddComponent", ui::Class("UnityEngine.UI", "UnityEngine.UI", "LayoutElement")));
      ui::Value(element.get(), "set_minWidth", slot_width);
      ui::Value(element.get(), "set_preferredWidth", slot_width);
      ui::Value(element.get(), "set_preferredHeight", 32.f);
      ui::Value(element.get(), "set_flexibleWidth", 0.f);
      Root glyph(GameObject("HousingIcon", transform.get(), layer));
      Root rect(ui::UiCall(glyph.get(), "get_transform"));
      ui::Value(rect.get(), "set_sizeDelta", Vec2{26.f, 26.f});
      ui::Value(rect.get(), "set_anchoredPosition", Vec2{0, 0});
      Root graphic(ui::WithType(glyph.get(), "AddComponent", ui::Class("UnityEngine.UI", "UnityEngine.UI", "Image")));
      ui::Set(graphic.get(), "set_sprite", sprite.get());
      ui::Value(graphic.get(), "set_color", Color{.45f, .9f, 1.f, 1.f});
      ui::Value(graphic.get(), "set_preserveAspect", true);
      ui::Value(graphic.get(), "set_raycastTarget", false);
      SetActive(glyph.get(), true);
    }
    auto* object = Target(it->second.object);
    Root  transform(ui::UiCall(object, "get_transform"));
    bool  world = false;
    void* args[]{row.get(), &world};
    ui::UiCall(transform.get(), "SetParent", 2, args);
    Root index_value(ui::UiCall(name_transform.get(), "GetSiblingIndex"));
    Root own_index(ui::UiCall(transform.get(), "GetSiblingIndex"));
    int  index = *static_cast<int*>(il2cpp_object_unbox(index_value.get()));
    if (*static_cast<int*>(il2cpp_object_unbox(own_index.get())) < index)
      --index;
    ui::Value(transform.get(), "SetSiblingIndex", index);
    SetActive(object, true);
  } catch (const std::exception& error) {
    Remove(widget);
    if (!warned) {
      warned = true;
      spdlog::warn("[GalaxyHousing] marker skipped: {}", error.what());
    }
  }
}

void Refresh()
{
  std::vector<void*> remove;
  for (const auto& [widget, marker] : markers) {
    try {
      if (!Enabled() || Target(marker.owner) != widget || !ui::Alive(Target(marker.owner))
          || !ui::Alive(Target(marker.object)))
        remove.push_back(widget);
    } catch (const std::exception&) {
      remove.push_back(widget);
    }
  }
  for (auto* widget : remove)
    Remove(widget);
}
} // namespace galaxy_housing
#else
namespace galaxy_housing
{
void Install(Il2CppClass*) {}
bool Available()
{ return false; }
bool Enabled()
{ return false; }
void Hide(void*) {}
void Remove(void*) {}
void Update(void*, int) {}
void Refresh() {}
} // namespace galaxy_housing
#endif
