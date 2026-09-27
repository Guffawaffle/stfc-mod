#include "patches/swap_ship_pin_input.h"

#include "patches/pinned_ship_sort.h"
#include "patches/screen_update_hook.h"
#include "patches/ship_identity.h"
#include "patches/ship_tech_indicators.h"
#include "patches/swap_ship_tile.h"
#include "prime/GameObject.h"
#include "prime/ShipTileWidget.h"
#include "prime/Transform.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp-tabledefs.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
constexpr auto kHoldTime = std::chrono::milliseconds(250);
constexpr float kDragDistanceSquared = 64.0f;
constexpr const char* kPinBadge = "CommunityMod_SwapShipPinBadge";

struct Vector2 {
  float x, y;
};

struct Vector3 {
  float x, y, z;
};

struct Rect {
  float x, y, width, height;
};

struct DragState {
  Il2CppGCHandle tile = nullptr;
  Il2CppGCHandle scroll_rect = nullptr;
  bool restore_scroll = false;
  int64_t ship_id = 0;
  Vector2 origin{};
  Clock::time_point pressed{};
  bool dragging = false;
  bool highlighted_source = false;
  Il2CppGCHandle highlighted_target = nullptr;
};

DragState drag;
int64_t suppress_click_id = 0;
std::vector<Il2CppGCHandle> tiles;
int group_refresh_ticks = 0;

struct InputMethods {
  bool (*down)(int) = nullptr;
  bool (*held)(int) = nullptr;
  bool (*up)(int) = nullptr;
  void (*mouse_position)(Vector3*) = nullptr;
  const MethodInfo* contains = nullptr;
  const MethodInfo* canvas_camera = nullptr;
  const MethodInfo* get_enabled = nullptr;
  const MethodInfo* set_enabled = nullptr;
  Il2CppClass* canvas = nullptr;
  Il2CppClass* scroll_rect = nullptr;
  bool ready = false;
  bool drag_ready = false;
};

InputMethods& Methods()
{
  static InputMethods methods = [] {
    InputMethods value;
    value.down = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonDown(System.Int32)");
    value.held = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
    value.up = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonUp(System.Int32)");
    value.mouse_position = il2cpp_resolve_icall_typed<void(Vector3*)>(
        "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
    auto utility = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "RectTransformUtility");
    auto canvas = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "Canvas");
    auto scroll = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "ScrollRect");
    auto behaviour = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Behaviour");
    value.contains = utility.GetMethodInfo("RectangleContainsScreenPoint", 3);
    value.canvas_camera = canvas.GetMethodInfo("get_worldCamera", 0);
    value.canvas = canvas.get_cls();
    value.scroll_rect = scroll.get_cls();
    value.get_enabled = behaviour.GetMethodInfo("get_enabled", 0);
    value.set_enabled = behaviour.GetMethodInfo("set_enabled", 1);
    value.ready = value.down && value.held && value.up && value.mouse_position && value.contains
                  && value.canvas_camera && value.canvas;
    value.drag_ready = value.ready && value.scroll_rect && value.get_enabled && value.set_enabled;
    if (!value.ready)
      spdlog::warn("[PinnedShipSort] pin pointer geometry unavailable; badge clicks remain disabled");
    else if (!value.drag_ready)
      spdlog::warn("[PinnedShipSort] scroll control unavailable; pin drag disabled");
    return value;
  }();
  return methods;
}

Il2CppObject* Invoke(const MethodInfo* method, Il2CppObject* object, void** args = nullptr)
{
  if (!method)
    return nullptr;
  Il2CppException* exception = nullptr;
  auto* result = il2cpp_runtime_invoke(method, object, args, &exception);
  return exception ? nullptr : result;
}

bool InvokeVoid(const MethodInfo* method, Il2CppObject* object, void** args)
{
  if (!method || !object)
    return false;
  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(method, object, args, &exception);
  return !exception;
}

void ResetDrag()
{
  if (drag.highlighted_target) {
    auto* target = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.highlighted_target));
    ship_tech_indicators::SetPinBadgeHighlight(target, ship_tech_indicators::PinBadgeHighlight::None);
    il2cpp_gchandle_free(drag.highlighted_target);
  }
  if (drag.highlighted_source && drag.tile) {
    auto* source = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.tile));
    ship_tech_indicators::SetPinBadgeHighlight(source, ship_tech_indicators::PinBadgeHighlight::None);
  }
  if (drag.scroll_rect) {
    auto* scroll = il2cpp_gchandle_get_target(drag.scroll_rect);
    if (scroll && drag.restore_scroll) {
      bool enabled = true;
      void* args[]{&enabled};
      if (!InvokeVoid(Methods().set_enabled, scroll, args))
        spdlog::warn("[PinnedShipSort] could not restore ship bar scrolling");
    }
    il2cpp_gchandle_free(drag.scroll_rect);
  }
  if (drag.tile)
    il2cpp_gchandle_free(drag.tile);
  drag = {};
}

Transform* DirectChild(Transform* parent, const char* name)
{
  if (!parent)
    return nullptr;
  for (int index = 0; index < parent->childCount; ++index) {
    auto* child = parent->GetChild(index);
    if (child && child->gameObject && child->gameObject->Name() == name)
      return child;
  }
  return nullptr;
}

Transform* TileTransform(ShipTileWidget* tile)
{
  if (!tile || !tile->isActiveAndEnabled || !swap_ship_tile::IsInSelection(tile))
    return nullptr;
  return reinterpret_cast<Transform*>(Invoke(
      ShipTileWidget::get_class_helper().GetMethodInfo("get_transform", 0), reinterpret_cast<Il2CppObject*>(tile)));
}

Transform* Badge(ShipTileWidget* tile)
{
  auto* badge = DirectChild(TileTransform(tile), kPinBadge);
  return badge && badge->gameObject && badge->gameObject->activeInHierarchy ? badge : nullptr;
}

Il2CppObject* ParentScrollRect(ShipTileWidget* tile)
{
  auto* transform = TileTransform(tile);
  auto* object = transform ? reinterpret_cast<Il2CppObject*>(transform->gameObject) : nullptr;
  auto* method = object ? IL2CppClassHelper(object->klass).GetMethodInfo("GetComponentInParent", 2) : nullptr;
  if (!method || !Methods().drag_ready)
    return nullptr;
  void* type = IL2CppClassHelper(Methods().scroll_rect).GetType();
  bool include_inactive = false;
  void* args[]{type, &include_inactive};
  return Invoke(method, object, args);
}

Vector2 MousePosition()
{
  Vector3 position{};
  Methods().mouse_position(&position);
  return {position.x, position.y};
}

bool Contains(Transform* badge, Vector2 position)
{
  auto& methods = Methods();
  if (!methods.ready || !badge)
    return false;
  auto* game_object = reinterpret_cast<Il2CppObject*>(badge->gameObject);
  auto* get_parent_component = IL2CppClassHelper(game_object->klass).GetMethodInfo("GetComponentInParent", 2);
  if (!get_parent_component)
    return false;
  void* canvas_type = IL2CppClassHelper(methods.canvas).GetType();
  bool include_inactive = false;
  void* canvas_args[]{canvas_type, &include_inactive};
  auto* canvas = Invoke(get_parent_component, game_object, canvas_args);
  if (!canvas)
    return false;
  auto* camera = Invoke(methods.canvas_camera, canvas);
  void* args[]{badge, &position, camera};
  auto* boxed = Invoke(methods.contains, nullptr, args);
  return boxed && *static_cast<bool*>(il2cpp_object_unbox(boxed));
}

std::optional<bool> InShipBar(Vector2 position)
{
  auto* scroll = drag.scroll_rect ? il2cpp_gchandle_get_target(drag.scroll_rect) : nullptr;
  if (!scroll)
    return std::nullopt;
  auto helper = IL2CppClassHelper(scroll->klass);
  auto* viewport = Invoke(helper.GetMethodInfo("get_viewport", 0), scroll);
  if (!viewport)
    viewport = Invoke(helper.GetMethodInfo("get_transform", 0), scroll);
  return viewport ? std::optional<bool>{Contains(reinterpret_cast<Transform*>(viewport), position)} : std::nullopt;
}

ShipTileWidget* TileAt(Vector2 position, bool badge_only)
{
  for (auto handle : tiles) {
    auto* tile = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(handle));
    auto* target = badge_only ? Badge(tile) : TileTransform(tile);
    if (target && Badge(tile) && Contains(target, position))
      return tile;
  }
  return nullptr;
}

ShipTileWidget* AnyTileAt(Vector2 position)
{
  for (auto handle : tiles) {
    auto* tile = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(handle));
    if (auto* transform = TileTransform(tile); transform && Contains(transform, position))
      return tile;
  }
  return nullptr;
}

std::optional<int64_t> Id(ShipTileWidget* tile)
{ return tile && tile->Context && tile->Context->HasShip ? ship_identity::InstanceId(tile->Context) : std::nullopt; }

std::optional<ship_tech_indicators::PinGroupBounds> BoundsInContent(Transform* tile, Transform* content)
{
  static auto rect_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto transform_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  auto* boxed_rect = Invoke(rect_helper.GetMethodInfo("get_rect", 0), reinterpret_cast<Il2CppObject*>(tile));
  auto* transform_point = transform_helper.GetMethodInfo("TransformPoint", 1);
  auto* inverse_point = transform_helper.GetMethodInfo("InverseTransformPoint", 1);
  if (!boxed_rect || !transform_point || !inverse_point)
    return std::nullopt;
  const auto rect = *static_cast<Rect*>(il2cpp_object_unbox(boxed_rect));
  Vector3 corners[]{{rect.x, rect.y, 0}, {rect.x + rect.width, rect.y + rect.height, 0}};
  Vector3 local[2]{};
  for (int index = 0; index < 2; ++index) {
    void* point_args[]{&corners[index]};
    auto* world = Invoke(transform_point, reinterpret_cast<Il2CppObject*>(tile), point_args);
    if (!world)
      return std::nullopt;
    auto world_point = *static_cast<Vector3*>(il2cpp_object_unbox(world));
    void* world_args[]{&world_point};
    auto* relative = Invoke(inverse_point, reinterpret_cast<Il2CppObject*>(content), world_args);
    if (!relative)
      return std::nullopt;
    local[index] = *static_cast<Vector3*>(il2cpp_object_unbox(relative));
    if (!std::isfinite(local[index].x) || !std::isfinite(local[index].y)
        || std::abs(local[index].x) > 1'000'000 || std::abs(local[index].y) > 1'000'000)
      return std::nullopt;
  }
  return ship_tech_indicators::PinGroupBounds{std::min(local[0].x, local[1].x),
                                               std::min(local[0].y, local[1].y),
                                               std::max(local[0].x, local[1].x),
                                               std::max(local[0].y, local[1].y)};
}

void RefreshGroupBand()
{
  Transform* content = nullptr;
  std::optional<ship_tech_indicators::PinGroupBounds> bounds;
  for (auto handle : tiles) {
    auto* tile = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(handle));
    auto* transform = TileTransform(tile);
    if (!transform)
      continue;
    if (!content) {
      auto* scroll = ParentScrollRect(tile);
      auto* raw_content = scroll ? Invoke(IL2CppClassHelper(scroll->klass).GetMethodInfo("get_content", 0), scroll)
                                 : nullptr;
      content = reinterpret_cast<Transform*>(raw_content);
    }
    if (!content || !pinned_ship_sort::Rank(tile->Context))
      continue;
    const auto current = BoundsInContent(transform, content);
    if (!current)
      continue;
    if (!bounds) {
      bounds = current;
    } else {
      bounds->left = std::min(bounds->left, current->left);
      bounds->bottom = std::min(bounds->bottom, current->bottom);
      bounds->right = std::max(bounds->right, current->right);
      bounds->top = std::max(bounds->top, current->top);
    }
  }
  if (content)
    ship_tech_indicators::UpdatePinGroupBand(content, bounds);
}

void HighlightTarget(ShipTileWidget* target)
{
  auto* current = drag.highlighted_target
                      ? reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.highlighted_target))
                      : nullptr;
  if (current == target)
    return;
  if (current)
    ship_tech_indicators::SetPinBadgeHighlight(current, ship_tech_indicators::PinBadgeHighlight::None);
  if (drag.highlighted_target)
    il2cpp_gchandle_free(drag.highlighted_target);
  drag.highlighted_target = target
                                ? il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(target), false)
                                : nullptr;
  if (drag.highlighted_target)
    ship_tech_indicators::SetPinBadgeHighlight(target, ship_tech_indicators::PinBadgeHighlight::Target);
}

void Update()
{
  if (group_refresh_ticks > 0 && --group_refresh_ticks == 0)
    RefreshGroupBand();
  auto& methods = Methods();
  if (!methods.ready)
    return;
  if (methods.down(0)) {
    ResetDrag();
    suppress_click_id = 0;
    const auto position = MousePosition();
    auto* tile = TileAt(position, true);
    const auto id = Id(tile);
    if (id) {
      auto* scroll = ParentScrollRect(tile);
      auto* boxed_enabled = scroll ? Invoke(methods.get_enabled, scroll) : nullptr;
      if (boxed_enabled) {
        const bool was_enabled = *static_cast<bool*>(il2cpp_object_unbox(boxed_enabled));
        bool disabled = false;
        void* args[]{&disabled};
        if (!was_enabled || InvokeVoid(methods.set_enabled, scroll, args)) {
          auto tile_handle = il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(tile), false);
          auto scroll_handle = il2cpp_gchandle_new_weakref(scroll, false);
          if (tile_handle && scroll_handle)
            drag = {tile_handle, scroll_handle, was_enabled, *id, position, Clock::now(), false};
          else {
            if (tile_handle)
              il2cpp_gchandle_free(tile_handle);
            if (scroll_handle)
              il2cpp_gchandle_free(scroll_handle);
            if (was_enabled) {
              bool enabled = true;
              void* restore_args[]{&enabled};
              InvokeVoid(methods.set_enabled, scroll, restore_args);
            }
          }
        }
      }
    }
  }
  if (drag.ship_id && methods.held(0) && Clock::now() - drag.pressed >= kHoldTime) {
    const auto position = MousePosition();
    auto* source = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.tile));
    if (!drag.highlighted_source && source) {
      ship_tech_indicators::SetPinBadgeHighlight(source, ship_tech_indicators::PinBadgeHighlight::Source);
      drag.highlighted_source = true;
    }
    const auto dx = position.x - drag.origin.x;
    const auto dy = position.y - drag.origin.y;
    if (dx * dx + dy * dy >= kDragDistanceSquared)
      drag.dragging = true;
    if (drag.dragging) {
      auto* target = TileAt(position, false);
      HighlightTarget(target != source ? target : nullptr);
    }
  }
  if (methods.up(0) && drag.ship_id) {
    if (drag.dragging) {
      suppress_click_id = drag.ship_id;
      const auto position = MousePosition();
      auto* target = AnyTileAt(position);
      const auto target_id = Id(target);
      auto* source = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.tile));
      auto* context = Badge(source) ? swap_ship_tile::SelectionContext(source) : nullptr;
      const auto source_id = drag.ship_id;
      const bool inside_bar = InShipBar(position).value_or(target_id.has_value());
      const bool valid_drop = context && Id(source) == source_id && (!target || target_id)
                              && inside_bar;
      const auto rank_target = target_id && pinned_ship_sort::Rank(target->Context) ? target_id : std::nullopt;
      if (valid_drop)
        pinned_ship_sort::PlacePinnedShip(source->Context, rank_target, context);
      else
        spdlog::info("[PinnedShipSort] drag canceled outside ship row");
      ResetDrag();
    } else {
      ResetDrag();
    }
  } else if (drag.ship_id && !methods.held(0)) {
    ResetDrag();
  }
}
} // namespace

namespace swap_ship_pin_input
{
bool Install()
{
  return Methods().ready && install_screen_manager_update_hook() && register_screen_manager_update_callback(Update);
}

void RegisterTile(ShipTileWidget* tile)
{
  if (!tile)
    return;
  group_refresh_ticks = 3;
  for (auto it = tiles.begin(); it != tiles.end();) {
    auto* tracked = il2cpp_gchandle_get_target(*it);
    if (!tracked) {
      il2cpp_gchandle_free(*it);
      it = tiles.erase(it);
    } else if (tracked == reinterpret_cast<Il2CppObject*>(tile)) {
      return;
    } else {
      ++it;
    }
  }
  if (tiles.size() >= 4096) {
    spdlog::warn("[PinnedShipSort] tile registry full; drag target unavailable");
    return;
  }
  auto handle = il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(tile), false);
  if (handle)
    tiles.push_back(handle);
}

void RefreshVisibleBadges()
{
  for (auto handle : tiles)
    if (auto* tile = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(handle)))
      ship_tech_indicators::RefreshPinBadge(tile);
  group_refresh_ticks = 3;
}

bool HandleTileClick(ShipTileWidget* tile)
{
  if (!Methods().ready)
    return false;
  const auto id = Id(tile);
  auto* badge = Badge(tile);
  if (!id || !badge)
    return false;
  if (suppress_click_id == *id || drag.dragging)
    return true;
  if (!Contains(badge, MousePosition()))
    return false;
  return pinned_ship_sort::HandleCardAction(tile->Context, swap_ship_tile::SelectionContext(tile));
}
} // namespace swap_ship_pin_input
