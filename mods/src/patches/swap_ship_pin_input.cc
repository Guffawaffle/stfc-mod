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
#include <spud/detour.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
constexpr auto kHoldTime = std::chrono::milliseconds(125);
constexpr float kDragDistanceSquared = 64.0f;
constexpr const char* kPinBadge = "CommunityMod_SwapShipPinBadge";
constexpr const char* kPinGroupBand = "CommunityMod_SwapShipPinGroup";
constexpr const char* kFirstPinDropCue = "CommunityMod_FirstPinDropCue";

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
  int pointer_id = 0;
  int64_t ship_id = 0;
  Vector2 origin{};
  Clock::time_point pressed{};
  bool dragging = false;
  bool armed = false;
  bool highlighted_source = false;
  Il2CppGCHandle highlighted_target = nullptr;
  ship_tech_indicators::PinBadgeHighlight target_highlight = ship_tech_indicators::PinBadgeHighlight::None;
  Il2CppGCHandle cue_viewport = nullptr;
  bool group_hovered = false;
  bool cue_hovered = false;
};

DragState drag;
int64_t suppress_click_id = 0;
int64_t badge_press_id = 0;
std::vector<Il2CppGCHandle> tiles;
int group_refresh_ticks = 0;
bool hooks_ready = false;

struct InputMethods {
  bool (*held)(int) = nullptr;
  void (*mouse_position)(Vector3*) = nullptr;
  const MethodInfo* contains = nullptr;
  const MethodInfo* canvas_camera = nullptr;
  const MethodInfo* pointer_position = nullptr;
  const MethodInfo* pointer_press_position = nullptr;
  const MethodInfo* pointer_button = nullptr;
  const MethodInfo* pointer_id = nullptr;
  Il2CppClass* canvas = nullptr;
  Il2CppClass* scroll_rect = nullptr;
  bool ready = false;
  bool drag_ready = false;
};

InputMethods& Methods()
{
  static InputMethods methods = [] {
    InputMethods value;
    value.held = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
    value.mouse_position = il2cpp_resolve_icall_typed<void(Vector3*)>(
        "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
    auto utility = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "RectTransformUtility");
    auto canvas = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "Canvas");
    auto scroll = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "ScrollRect");
    auto pointer = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "PointerEventData");
    value.contains = utility.GetMethodInfo("RectangleContainsScreenPoint", 3);
    value.canvas_camera = canvas.GetMethodInfo("get_worldCamera", 0);
    value.pointer_position = pointer.GetMethodInfo("get_position", 0);
    value.pointer_press_position = pointer.GetMethodInfo("get_pressPosition", 0);
    value.pointer_button = pointer.GetMethodInfo("get_button", 0);
    value.pointer_id = pointer.GetMethodInfo("get_pointerId", 0);
    value.canvas = canvas.get_cls();
    value.scroll_rect = scroll.get_cls();
    value.ready = value.mouse_position && value.contains && value.canvas_camera && value.canvas;
    value.drag_ready = value.ready && value.scroll_rect && value.pointer_position && value.pointer_press_position
                       && value.pointer_button && value.pointer_id && value.held;
    if (!value.ready)
      spdlog::warn("[PinnedShipSort] pin pointer geometry unavailable; badge clicks remain disabled");
    else if (!value.drag_ready)
      spdlog::warn("[PinnedShipSort] pointer event data unavailable; pin drag disabled");
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

void ResetDrag()
{
  if (drag.cue_viewport) {
    auto* viewport = reinterpret_cast<Transform*>(il2cpp_gchandle_get_target(drag.cue_viewport));
    ship_tech_indicators::SetFirstPinDropCue(viewport, false, false);
    il2cpp_gchandle_free(drag.cue_viewport);
  }
  if (drag.group_hovered && drag.scroll_rect) {
    auto* scroll = il2cpp_gchandle_get_target(drag.scroll_rect);
    auto* content = scroll ? Invoke(IL2CppClassHelper(scroll->klass).GetMethodInfo("get_content", 0), scroll)
                           : nullptr;
    ship_tech_indicators::SetPinGroupHover(reinterpret_cast<Transform*>(content), false);
  }
  if (drag.highlighted_target) {
    auto* target = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.highlighted_target));
    ship_tech_indicators::SetPinBadgeHighlight(target, ship_tech_indicators::PinBadgeHighlight::None);
    il2cpp_gchandle_free(drag.highlighted_target);
  }
  if (drag.highlighted_source && drag.tile) {
    auto* source = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.tile));
    ship_tech_indicators::SetPinBadgeHighlight(source, ship_tech_indicators::PinBadgeHighlight::None);
  }
  if (drag.scroll_rect)
    il2cpp_gchandle_free(drag.scroll_rect);
  if (drag.tile)
    il2cpp_gchandle_free(drag.tile);
  drag = {};
}

void CancelDragClick()
{
  if (drag.ship_id)
    suppress_click_id = drag.ship_id;
  badge_press_id = 0;
  ResetDrag();
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

Transform* GroupContent()
{
  auto* scroll = drag.scroll_rect ? il2cpp_gchandle_get_target(drag.scroll_rect) : nullptr;
  auto* content = scroll ? Invoke(IL2CppClassHelper(scroll->klass).GetMethodInfo("get_content", 0), scroll)
                         : nullptr;
  return reinterpret_cast<Transform*>(content);
}

Vector2 MousePosition()
{
  Vector3 position{};
  Methods().mouse_position(&position);
  return {position.x, position.y};
}

bool BoxedValue(Il2CppObject* boxed, const char* namespaze, const char* name, std::size_t size)
{
  auto* cls = boxed ? il2cpp_object_get_class(boxed) : nullptr;
  return cls && il2cpp_class_is_valuetype(cls) && il2cpp_class_value_size(cls, nullptr) == size
         && !std::strcmp(il2cpp_class_get_namespace(cls), namespaze)
         && !std::strcmp(il2cpp_class_get_name(cls), name);
}

std::optional<Vector2> PointerPosition(Il2CppObject* event_data, const MethodInfo* getter)
{
  auto* boxed = event_data ? Invoke(getter, event_data) : nullptr;
  if (!BoxedValue(boxed, "UnityEngine", "Vector2", sizeof(Vector2)))
    return std::nullopt;
  const auto position = *static_cast<Vector2*>(il2cpp_object_unbox(boxed));
  return std::isfinite(position.x) && std::isfinite(position.y) ? std::optional<Vector2>{position} : std::nullopt;
}

bool LeftButton(Il2CppObject* event_data)
{
  auto* boxed = event_data ? Invoke(Methods().pointer_button, event_data) : nullptr;
  auto* cls = boxed ? il2cpp_object_get_class(boxed) : nullptr;
  auto* owner = cls ? il2cpp_class_get_declaring_type(cls) : nullptr;
  return cls && il2cpp_class_is_valuetype(cls) && il2cpp_class_value_size(cls, nullptr) == sizeof(int32_t)
         && !std::strcmp(il2cpp_class_get_name(cls), "InputButton") && owner
         && !std::strcmp(il2cpp_class_get_namespace(owner), "UnityEngine.EventSystems")
         && !std::strcmp(il2cpp_class_get_name(owner), "PointerEventData")
         && *static_cast<int32_t*>(il2cpp_object_unbox(boxed)) == 0;
}

std::optional<int32_t> PointerId(Il2CppObject* event_data)
{
  auto* boxed = event_data ? Invoke(Methods().pointer_id, event_data) : nullptr;
  return BoxedValue(boxed, "System", "Int32", sizeof(int32_t))
             ? std::optional<int32_t>{*static_cast<int32_t*>(il2cpp_object_unbox(boxed))}
             : std::nullopt;
}

bool MatchesDrag(Il2CppObject* scroll, Il2CppObject* event_data)
{
  const auto pointer_id = PointerId(event_data);
  return drag.ship_id && drag.scroll_rect && il2cpp_gchandle_get_target(drag.scroll_rect) == scroll
         && pointer_id && *pointer_id == drag.pointer_id;
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

Transform* ShipBarViewport()
{
  auto* scroll = drag.scroll_rect ? il2cpp_gchandle_get_target(drag.scroll_rect) : nullptr;
  if (!scroll)
    return nullptr;
  auto helper = IL2CppClassHelper(scroll->klass);
  auto* viewport = Invoke(helper.GetMethodInfo("get_viewport", 0), scroll);
  if (!viewport)
    viewport = Invoke(helper.GetMethodInfo("get_transform", 0), scroll);
  return reinterpret_cast<Transform*>(viewport);
}

std::optional<bool> InShipBar(Vector2 position)
{
  auto* viewport = ShipBarViewport();
  return viewport ? std::optional<bool>{Contains(viewport, position)} : std::nullopt;
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

ShipTileWidget* DragSource()
{
  auto* source = drag.tile ? reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.tile)) : nullptr;
  return Badge(source) && Id(source) == drag.ship_id ? source : nullptr;
}

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
    if (!content || !pinned_ship_sort::IsPinnedForDisplay(tile->Context))
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

bool HasVisiblePinGroup()
{
  auto* scroll = drag.scroll_rect ? il2cpp_gchandle_get_target(drag.scroll_rect) : nullptr;
  for (auto handle : tiles) {
    auto* tile = reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(handle));
    if (Id(tile) && ParentScrollRect(tile) == scroll && pinned_ship_sort::IsPinnedForDisplay(tile->Context))
      return true;
  }
  return false;
}

bool InPinGroup(Vector2 position)
{
  auto* tile = TileAt(position, false);
  if (tile && pinned_ship_sort::IsPinnedForDisplay(tile->Context))
    return true;
  auto* band = DirectChild(GroupContent(), kPinGroupBand);
  return band && band->gameObject && band->gameObject->activeInHierarchy && Contains(band, position);
}

Transform* CueViewport()
{ return drag.cue_viewport ? reinterpret_cast<Transform*>(il2cpp_gchandle_get_target(drag.cue_viewport)) : nullptr; }

bool InFirstPinCue(Vector2 position)
{
  auto* cue = DirectChild(CueViewport(), kFirstPinDropCue);
  return cue && cue->gameObject && cue->gameObject->activeInHierarchy && Contains(cue, position);
}

void UpdateDropPreview(Vector2 position)
{
  const bool group_hovered = InPinGroup(position);
  if (group_hovered != drag.group_hovered) {
    ship_tech_indicators::SetPinGroupHover(GroupContent(), group_hovered);
    drag.group_hovered = group_hovered;
  }
  if (auto* cue = CueViewport()) {
    const bool cue_hovered = InFirstPinCue(position);
    if (cue_hovered != drag.cue_hovered) {
      ship_tech_indicators::SetFirstPinDropCue(cue, true, cue_hovered);
      drag.cue_hovered = cue_hovered;
    }
  }
}

void HighlightTarget(ShipTileWidget* target, ship_tech_indicators::PinBadgeHighlight highlight)
{
  auto* current = drag.highlighted_target
                      ? reinterpret_cast<ShipTileWidget*>(il2cpp_gchandle_get_target(drag.highlighted_target))
                      : nullptr;
  if (current == target && drag.target_highlight == highlight)
    return;
  if (current)
    ship_tech_indicators::SetPinBadgeHighlight(current, ship_tech_indicators::PinBadgeHighlight::None);
  if (drag.highlighted_target)
    il2cpp_gchandle_free(drag.highlighted_target);
  drag.highlighted_target = target
                                ? il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(target), false)
                                : nullptr;
  drag.target_highlight = drag.highlighted_target ? highlight : ship_tech_indicators::PinBadgeHighlight::None;
  if (drag.highlighted_target)
    ship_tech_indicators::SetPinBadgeHighlight(target, highlight);
}

float DragDistanceSquared(Vector2 position)
{
  const auto dx = position.x - drag.origin.x;
  const auto dy = position.y - drag.origin.y;
  return dx * dx + dy * dy;
}

bool StartPinDrag()
{
  auto* source = DragSource();
  if (!source)
    return false;
  badge_press_id = 0;
  drag.dragging = true;
  if (!drag.highlighted_source) {
    drag.highlighted_source = true;
    ship_tech_indicators::SetPinBadgeHighlight(source, ship_tech_indicators::PinBadgeHighlight::Source);
  }
  RefreshGroupBand();
  if (!HasVisiblePinGroup()) {
    if (auto* viewport = ShipBarViewport()) {
      drag.cue_viewport = il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(viewport), false);
      if (drag.cue_viewport) {
        ship_tech_indicators::SetFirstPinDropCue(viewport, true, false);
        spdlog::info("[PinnedShipSort] first-pin drop cue shown at ship-row leading edge");
      }
    } else {
      spdlog::warn("[PinnedShipSort] no ship-row viewport for first-pin drop cue");
    }
  }
  spdlog::info("[PinnedShipSort] pin drag started ship={}", drag.ship_id);
  return true;
}

void UpdatePinDrag(Vector2 position)
{
  auto* source = DragSource();
  if (!source) {
    ResetDrag();
    return;
  }
  UpdateDropPreview(position);
  auto* target = TileAt(position, false);
  if (target == source)
    target = nullptr;
  if (InPinGroup(position)) {
    HighlightTarget(target && pinned_ship_sort::IsPinnedForDisplay(target->Context) ? target : nullptr,
                    ship_tech_indicators::PinBadgeHighlight::Target);
  } else {
    HighlightTarget(target && pinned_ship_sort::IsPinnedForDisplay(source->Context) ? target : nullptr,
                    ship_tech_indicators::PinBadgeHighlight::UnpinTarget);
  }
}

void FinishPinDrag(Vector2 position)
{
  suppress_click_id = drag.ship_id;
  badge_press_id = 0;
  auto* target = AnyTileAt(position);
  const auto target_id = Id(target);
  auto* source = DragSource();
  auto* context = source ? swap_ship_tile::SelectionContext(source) : nullptr;
  const bool in_cue = InFirstPinCue(position);
  const bool in_group = InPinGroup(position);
  const bool source_pinned = source && pinned_ship_sort::IsPinnedForDisplay(source->Context);
  const bool inside_bar = InShipBar(position).value_or(target_id.has_value() || in_cue);
  if (!context || !inside_bar || (target && !target_id && !in_cue)) {
    spdlog::info("[PinnedShipSort] drag canceled outside ship row");
  } else if (in_cue) {
    if (!source_pinned)
      pinned_ship_sort::PlacePinnedShip(source->Context, std::nullopt, context);
  } else if (in_group) {
    const auto rank_target = target_id && pinned_ship_sort::IsPinnedForDisplay(target->Context)
                                 ? target_id
                                 : std::nullopt;
    pinned_ship_sort::PlacePinnedShip(source->Context, rank_target, context);
  } else if (source_pinned) {
    pinned_ship_sort::HandleCardAction(source->Context, context);
  } else {
    spdlog::info("[PinnedShipSort] unpinned drag ended outside pin group; no pin change");
  }
  ResetDrag();
}

void ScrollRect_OnInitializePotentialDrag_Hook(auto original, Il2CppObject* scroll, Il2CppObject* event_data)
{
  original(scroll, event_data);
  if (!hooks_ready || !LeftButton(event_data))
    return;
  const auto pointer_id = PointerId(event_data);
  if (!pointer_id || (drag.ship_id && *pointer_id != drag.pointer_id))
    return;
  ResetDrag();
  suppress_click_id = 0;
  badge_press_id = 0;
  const auto position = PointerPosition(event_data, Methods().pointer_press_position);
  auto* tile = position ? TileAt(*position, true) : nullptr;
  const auto id = Id(tile);
  if (!id || !pointer_id || ParentScrollRect(tile) != scroll)
    return;
  auto tile_handle = il2cpp_gchandle_new_weakref(reinterpret_cast<Il2CppObject*>(tile), false);
  auto scroll_handle = il2cpp_gchandle_new_weakref(scroll, false);
  if (!tile_handle || !scroll_handle) {
    if (tile_handle)
      il2cpp_gchandle_free(tile_handle);
    if (scroll_handle)
      il2cpp_gchandle_free(scroll_handle);
    return;
  }
  drag.tile = tile_handle;
  drag.scroll_rect = scroll_handle;
  drag.pointer_id = *pointer_id;
  drag.ship_id = *id;
  badge_press_id = *id;
  drag.origin = *position;
  drag.pressed = Clock::now();
  spdlog::info("[PinnedShipSort] pin drag candidate ship={}", *id);
}

void ScrollRect_OnBeginDrag_Hook(auto original, Il2CppObject* scroll, Il2CppObject* event_data)
{
  if (!hooks_ready || !MatchesDrag(scroll, event_data)) {
    original(scroll, event_data);
    return;
  }
  const auto position = PointerPosition(event_data, Methods().pointer_position);
  if (!position || !DragSource()) {
    CancelDragClick();
    original(scroll, event_data);
    return;
  }
  if (Clock::now() - drag.pressed < kHoldTime) {
    spdlog::info("[PinnedShipSort] badge movement passed to ship-bar scroll ship={}", drag.ship_id);
    CancelDragClick();
    original(scroll, event_data);
    return;
  }
  suppress_click_id = drag.ship_id;
  badge_press_id = 0;
  if (DragDistanceSquared(*position) >= kDragDistanceSquared && StartPinDrag())
    UpdatePinDrag(*position);
  // A small move after the hold remains pending until OnDrag reaches eight pixels.
}

void ScrollRect_OnDrag_Hook(auto original, Il2CppObject* scroll, Il2CppObject* event_data)
{
  if (!hooks_ready || !MatchesDrag(scroll, event_data)) {
    original(scroll, event_data);
    return;
  }
  const auto position = PointerPosition(event_data, Methods().pointer_position);
  if (!position || !DragSource()) {
    CancelDragClick();
    return;
  }
  if (!drag.dragging && DragDistanceSquared(*position) >= kDragDistanceSquared && !StartPinDrag()) {
    CancelDragClick();
    return;
  }
  if (drag.dragging)
    UpdatePinDrag(*position);
}

void ScrollRect_OnEndDrag_Hook(auto original, Il2CppObject* scroll, Il2CppObject* event_data)
{
  if (!hooks_ready || !MatchesDrag(scroll, event_data)) {
    original(scroll, event_data);
    return;
  }
  if (drag.dragging) {
    const auto position = PointerPosition(event_data, Methods().pointer_position);
    spdlog::info("[PinnedShipSort] pin drag ended ship={}", drag.ship_id);
    if (position)
      FinishPinDrag(*position);
    else
      CancelDragClick();
  } else {
    spdlog::debug("[PinnedShipSort] pin drag candidate ended without movement ship={}", drag.ship_id);
    CancelDragClick();
  }
}

void UpdateGroupBand()
{
  if (group_refresh_ticks > 0 && --group_refresh_ticks == 0)
    RefreshGroupBand();
  if (!drag.ship_id || drag.dragging)
    return;
  // This release check runs only while a badge press is pending. A release outside
  // the Button below Unity's drag threshold has no ScrollRect or click callback.
  if (drag.pointer_id == -1 && !Methods().held(0)) {
    ResetDrag();
    return;
  }
  if (!drag.armed && Clock::now() - drag.pressed >= kHoldTime) {
    if (auto* source = DragSource()) {
      drag.armed = true;
      drag.highlighted_source = true;
      ship_tech_indicators::SetPinBadgeHighlight(source, ship_tech_indicators::PinBadgeHighlight::Source);
      spdlog::debug("[PinnedShipSort] pin drag armed ship={}", drag.ship_id);
    } else {
      ResetDrag();
    }
  }
}
} // namespace

namespace swap_ship_pin_input
{
bool Install()
{
  if (!Methods().drag_ready)
    return false;
  auto scroll = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "ScrollRect");
  auto* potential_drag = scroll.GetMethod("OnInitializePotentialDrag", 1);
  auto* begin_drag = scroll.GetMethod("OnBeginDrag", 1);
  auto* on_drag = scroll.GetMethod("OnDrag", 1);
  auto* end_drag = scroll.GetMethod("OnEndDrag", 1);
  if (!potential_drag || !begin_drag || !on_drag || !end_drag) {
    spdlog::warn("[PinnedShipSort] ScrollRect drag callbacks unavailable; pin input disabled");
    return false;
  }
  const bool potential_hooked = SPUD_STATIC_DETOUR(potential_drag, ScrollRect_OnInitializePotentialDrag_Hook) != nullptr;
  const bool begin_hooked = SPUD_STATIC_DETOUR(begin_drag, ScrollRect_OnBeginDrag_Hook) != nullptr;
  const bool drag_hooked = SPUD_STATIC_DETOUR(on_drag, ScrollRect_OnDrag_Hook) != nullptr;
  const bool end_hooked = SPUD_STATIC_DETOUR(end_drag, ScrollRect_OnEndDrag_Hook) != nullptr;
  if (!potential_hooked || !begin_hooked || !drag_hooked || !end_hooked) {
    spdlog::error("[PinnedShipSort] ScrollRect drag detours incomplete ({}, {}, {}, {}); pin input disabled",
                  potential_hooked, begin_hooked, drag_hooked, end_hooked);
    return false;
  }
  hooks_ready = true;
  if (!install_screen_manager_update_hook() || !register_screen_manager_update_callback(UpdateGroupBand))
    spdlog::warn("[PinnedShipSort] delayed pin group outline unavailable");
  spdlog::info("[PinnedShipSort] ScrollRect pin drag callbacks installed");
  return true;
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
  if (suppress_click_id == *id || drag.dragging) {
    suppress_click_id = 0;
    badge_press_id = 0;
    ResetDrag();
    return true;
  }
  if (badge_press_id != *id)
    return false;
  const bool on_badge = Contains(badge, MousePosition());
  badge_press_id = 0;
  if (drag.ship_id == *id)
    ResetDrag();
  return on_badge && pinned_ship_sort::HandleCardAction(tile->Context, swap_ship_tile::SelectionContext(tile));
}
} // namespace swap_ship_pin_input
