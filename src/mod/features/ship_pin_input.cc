// Pin badges and held badge dragging in Swap Ship. Identity, sorting and durable
// publication belong to dock.cc; this adapter owns only weak native UI custody.
#include "features/ship_pin_input.h"
#include "features/ship_tech_indicators.h"
#include "pin_pointer_policy.h"

#include "game/fleet.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/application.h>
#include <unity/input.h>
#include <unity/ui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace
{
namespace pointer                   = ship_pin_pointer;
constexpr const char*  kPinBadge    = "CommunityMod_SwapShipPinBadge";
constexpr const char*  kPinBand     = "CommunityMod_SwapShipPinGroup";
constexpr const char*  kPinCue      = "CommunityMod_FirstPinDropCue";
constexpr const char*  kPinCueLabel = "CommunityMod_FirstPinDropLabel";
constexpr const char*  kPinCueEdge  = "CommunityMod_FirstPinDropEdge";
constexpr const char*  kPinGroupEdges[]{"CommunityMod_PinGroupTop", "CommunityMod_PinGroupBottom",
                                        "CommunityMod_PinGroupLeft", "CommunityMod_PinGroupRight"};
constexpr const char*  kPinDragEdges[]{"CommunityMod_PinDragTop", "CommunityMod_PinDragBottom",
                                       "CommunityMod_PinDragLeft", "CommunityMod_PinDragRight"};
constexpr unity::Color kPinnedColor{0.97f, 0.84f, 0.48f, 1.0f};
constexpr unity::Color kUnpinnedColor{0.54f, 0.63f, 0.66f, 0.94f};
struct PinTile {
  il2cpp::GcHandle tile, context;
  int64_t          ship       = 0;
  uint64_t         generation = 0;
};
struct PinInputState {
  std::vector<PinTile> tiles;
  uint64_t             generation = 0;
  pointer::Policy      policy;
  il2cpp::GcHandle     source, scroll, cue, target, band, cue_viewport;
  bool                 group_hovered      = false;
  bool                 source_highlighted = false;
  bool                 installed = false, attempted = false;
  int                  group_refresh_frames = 0;
};
PinInputState& PinInput()
{
  static auto* state = new PinInputState;
  return *state;
}
template <typename T> std::uintptr_t PinToken(T* value)
{ return reinterpret_cast<std::uintptr_t>(value); }
bool PinFinite(unity::Vector2 value)
{ return std::isfinite(value.x) && std::isfinite(value.y); }
uint64_t PinInputEpoch()
{
  const auto frame = unity::Time::frameCount();
  return frame >= 0 ? static_cast<uint64_t>(frame) + 1 : 0;
}
std::optional<pointer::Point> PinPosition(unity::PointerEventData* data, bool press = false)
{
  if (!data)
    return std::nullopt;
  const auto observed = press ? data->PressPosition() : data->Position();
  return observed && PinFinite(observed.value) ? std::optional{pointer::Point{observed.value.x, observed.value.y}}
                                               : std::nullopt;
}
std::optional<unity::Vector2> PinMouse()
{
  const auto           observed = unity::Input::TryMousePosition();
  const unity::Vector2 point{observed.value.x, observed.value.y};
  return observed && PinFinite(point) ? std::optional{point} : std::nullopt;
}
unity::RectTransform* PinRect(ShipTileWidget* tile)
{
  auto* transform = tile ? tile->Transform() : nullptr;
  return transform ? transform->As<unity::RectTransform>() : nullptr;
}
unity::ScrollRect* PinScroll(ShipTileWidget* tile)
{
  auto* transform = tile ? tile->Transform() : nullptr;
  return transform ? transform->FindInParents<unity::ScrollRect>() : nullptr;
}
unity::RectTransform* PinViewport(unity::ScrollRect* scroll)
{
  if (!scroll)
    return nullptr;
  const auto viewport = scroll->TryCall<"get_viewport", unity::RectTransform*>();
  if (!viewport)
    return nullptr;
  if (viewport.value)
    return viewport.value;
  // Preserve PLAY's unset-viewport fallback to the ScrollRect's own rectangle.
  // A failed observation is distinct from a successfully observed null viewport.
  const auto transform = scroll->TryCall<"get_transform", unity::Transform*>();
  return transform && transform.value ? transform.value->As<unity::RectTransform>() : nullptr;
}
unity::RectTransform* PinChild(unity::Transform* parent, const char* name)
{
  auto* child = parent ? parent->Find(name) : nullptr;
  return child ? child->As<unity::RectTransform>() : nullptr;
}
void PinVisible(unity::Transform* transform, bool visible)
{
  if (auto* object = transform ? transform->gameObject() : nullptr)
    object->SetActive(visible);
}
std::optional<bool> PinContains(unity::RectTransform* rect, unity::Vector2 position)
{
  if (!rect || !PinFinite(position) || !unity::Alive(rect))
    return std::nullopt;
  auto* canvas = rect->FindInParents<unity::Canvas>();
  if (!canvas)
    return std::nullopt;
  const auto camera = canvas->TryCall<"get_worldCamera", unity::Camera*>();
  if (!camera)
    return std::nullopt;
  const auto hit = unity::RectTransformUtility::Contains(rect, position, camera.value);
  return hit ? std::optional{hit.value} : std::nullopt;
}
unity::Image* PinImage(unity::RectTransform* rect)
{
  auto* object = rect ? rect->gameObject() : nullptr;
  return object ? object->GetComponent<unity::Image>() : nullptr;
}
unity::RectTransform* PinSurface(unity::Transform* parent, const char* name, unity::Vector2 anchor,
                                 unity::Vector2 pivot, unity::Vector2 size, unity::Vector2 position, unity::Color color,
                                 bool exclude_from_layout = false)
{
  auto* rect = PinChild(parent, name);
  if (!rect) {
    il2cpp::GcHandle root;
    rect         = unity::CreateUiObject(name, parent, root);
    auto* object = rect ? rect->gameObject() : nullptr;
    auto* image  = object ? object->AddComponent<unity::Image>() : nullptr;
    if (!image) {
      if (object)
        unity::Destroy(object);
      return nullptr;
    }
    image->SetRaycastTarget(false);
  }
  if (exclude_from_layout) {
    // Decorations under native content/viewport/tile parents are not layout
    // children. Keep them inactive until exclusion is observed, not just set.
    PinVisible(rect, false);
    auto* object = rect->gameObject();
    auto* layout = object ? object->GetComponent<unity::LayoutElement>() : nullptr;
    if (!layout && object)
      layout = object->AddComponent<unity::LayoutElement>();
    if (!layout || !layout->ExcludeFromLayout())
      return nullptr;
  }
  rect->Place(anchor, anchor, pivot, size, position);
  if (auto* image = PinImage(rect))
    image->SetColor(color);
  else
    return nullptr;
  PinVisible(rect, true);
  return rect;
}
unity::RectTransform* PinDrawBadge(unity::Transform* parent, const char* name, unity::Vector2 anchor,
                                   unity::Vector2 position, bool pinned)
{
  auto* badge = PinSurface(parent, name, anchor, {1, 0}, {34, 24}, position, {0.06f, 0.10f, 0.12f, 0.82f}, true);
  if (!badge)
    return nullptr;
  const auto color = pinned ? kPinnedColor : kUnpinnedColor;
  struct Piece {
    const char*    name;
    unity::Vector2 size, position;
    bool           filled;
  };
  constexpr Piece pieces[] = {{"Cap", {11, 3}, {17, 17}, false},         {"Head", {7, 6}, {17, 13}, true},
                              {"Stem", {2, 9}, {17, 6}, false},          {"Left", {1.5f, 6}, {14.25f, 13}, false},
                              {"Right", {1.5f, 6}, {19.75f, 13}, false}, {"Base", {7, 1.5f}, {17, 10.75f}, false}};
  for (const auto& piece : pieces) {
    auto* child = PinSurface(badge, piece.name, {0, 0}, {0.5f, 0.5f}, piece.size, piece.position, color);
    if (!child) {
      PinVisible(badge, false);
      return nullptr;
    }
    PinVisible(child, piece.filled ? pinned : true);
  }
  // Unpinned heads remain an outline; pinned heads are filled.
  for (const char* part : {"Left", "Right", "Base"})
    PinVisible(PinChild(badge, part), !pinned);
  return badge;
}
bool PinEdge(unity::Transform* parent, const char* name, unity::Vector2 anchor_min, unity::Vector2 anchor_max,
             unity::Vector2 pivot, unity::Vector2 size, unity::Vector2 position, unity::Color color, bool visible,
             bool unmasked = false, bool exclude_from_layout = false)
{
  if (!visible) {
    PinVisible(PinChild(parent, name), false);
    return true;
  }
  auto* edge  = PinSurface(parent, name, anchor_min, pivot, size, position, color, exclude_from_layout);
  auto* image = PinImage(edge);
  if (!edge || !image || (unmasked && !image->TrySetMaskable(false))) {
    PinVisible(edge, false);
    return false;
  }
  edge->Place(anchor_min, anchor_max, pivot, size, position);
  return true;
}
bool PinStyleBand(unity::RectTransform* band, bool active, bool hovered)
{
  auto* image = PinImage(band);
  if (!image)
    return false;
  const auto  fill  = hovered  ? unity::Color{0.13f, 0.35f, 0.32f, 0.90f}
                      : active ? unity::Color{0.12f, 0.24f, 0.25f, 0.78f}
                               : unity::Color{0.06f, 0.17f, 0.20f, 0.72f};
  const auto  rim   = hovered  ? unity::Color{0.26f, 0.96f, 0.77f, 1.0f}
                      : active ? unity::Color{1.0f, 0.76f, 0.16f, 1.0f}
                               : unity::Color{0.83f, 0.67f, 0.32f, 0.82f};
  const float width = hovered ? 5.0f : active ? 4.5f : 3.0f;
  image->SetColor(fill);
  // Keep the fill clipped to the viewport, while the frozen outer rim remains complete below the cards.
  const bool ready =
      PinEdge(band, kPinGroupEdges[0], {0, 1}, {1, 1}, {0.5f, 1}, {0, width}, {}, rim, true, true)
      && PinEdge(band, kPinGroupEdges[1], {0, 0}, {1, 0}, {0.5f, 0}, {0, width}, {}, rim, true, true)
      && PinEdge(band, kPinGroupEdges[2], {0, 0}, {0, 1}, {0, 0.5f}, {width, -2 * width}, {}, rim, true, true)
      && PinEdge(band, kPinGroupEdges[3], {1, 0}, {1, 1}, {1, 0.5f}, {width, -2 * width}, {}, rim, true, true);
  if (!ready)
    PinVisible(band, false);
  return ready;
}
enum class PinHighlightKind { None, Source, Target, UnpinTarget };
void PinStyleTile(unity::RectTransform* rect, PinHighlightKind highlight)
{
  if (auto* image = PinImage(PinChild(rect, kPinBadge)))
    image->SetColor(highlight == PinHighlightKind::Source        ? unity::Color{0.52f, 0.27f, 0.02f, 0.98f}
                    : highlight == PinHighlightKind::Target      ? unity::Color{0.03f, 0.40f, 0.34f, 0.95f}
                    : highlight == PinHighlightKind::UnpinTarget ? unity::Color{0.42f, 0.14f, 0.07f, 0.95f}
                                                                 : unity::Color{0.06f, 0.10f, 0.12f, 0.82f});
  const bool  source  = highlight == PinHighlightKind::Source;
  const bool  visible = highlight != PinHighlightKind::None;
  const float width   = source ? 4.5f : 2.5f;
  const auto  color   = source                                       ? unity::Color{1.0f, 0.76f, 0.16f, 1.0f}
                        : highlight == PinHighlightKind::UnpinTarget ? unity::Color{1.0f, 0.43f, 0.25f, 0.95f}
                                                                     : unity::Color{0.10f, 0.82f, 0.66f, 0.9f};
  (void)PinEdge(rect, kPinDragEdges[0], {0, 1}, {1, 1}, {0.5f, 1}, {-8, width}, {0, -3}, color, visible, false, true);
  (void)PinEdge(rect, kPinDragEdges[1], {0, 0}, {1, 0}, {0.5f, 0}, {-8, width}, {0, 3}, color, visible, false, true);
  (void)PinEdge(rect, kPinDragEdges[2], {0, 0}, {0, 1}, {0, 0.5f}, {width, -8}, {3, 0}, color, visible, false, true);
  (void)PinEdge(rect, kPinDragEdges[3], {1, 0}, {1, 1}, {1, 0.5f}, {width, -8}, {-3, 0}, color, visible, false, true);
}
PinTile* PinRecord(ShipTileWidget* tile)
{
  auto&      rows  = PinInput().tiles;
  const auto found = std::ranges::find_if(rows, [&](const PinTile& row) { return row.tile.Get() == tile; });
  return found == rows.end() ? nullptr : &*found;
}
pointer::Identity PinIdentity(ShipTileWidget* tile)
{
  auto* row = PinRecord(tile);
  if (!row || !tile->isActiveAndEnabled() || !CanvasController::Shows(tile) || tile->PinContext() != row->context.Get())
    return {};
  const auto fleet = tile->TryCall<"get_Context", FleetPlayerData*>();
  const auto id    = fleet && fleet.value ? fleet.value->ShipInstanceId() : std::nullopt;
  return id && *id == row->ship ? pointer::Identity{PinToken(tile), row->generation, *id} : pointer::Identity{};
}
unity::RectTransform* PinBadge(ShipTileWidget* tile)
{
  if (!PinIdentity(tile))
    return nullptr;
  auto* badge  = PinChild(PinRect(tile), kPinBadge);
  auto* object = badge ? badge->gameObject() : nullptr;
  return object && object->activeInHierarchy() ? badge : nullptr;
}
void PinHideCue()
{
  auto& state = PinInput();
  PinVisible(state.cue.Get<unity::RectTransform>(), false);
  PinVisible(PinChild(state.cue_viewport.Get<unity::RectTransform>(), kPinCueEdge), false);
  state.cue.Reset();
  state.cue_viewport.Reset();
}
void PinRefreshTile(ShipTileWidget* tile);
void PinReleaseRoots()
{
  auto& state = PinInput();
  PinHideCue();
  if (auto* band = state.band.Get<unity::RectTransform>())
    (void)PinStyleBand(band, false, false);
  state.band.Reset();
  state.group_hovered      = false;
  state.source_highlighted = false;
  if (auto* target = state.target.Get<ShipTileWidget>(); target && target->isActiveAndEnabled())
    PinRefreshTile(target);
  if (auto* source = state.source.Get<ShipTileWidget>(); source && source->isActiveAndEnabled())
    PinRefreshTile(source);
  state.source.Reset();
  state.scroll.Reset();
  state.target.Reset();
}
template <typename Fn> bool PinUiAttempt(Fn&& work) noexcept
{
  try {
    work();
    return true;
  } catch (...) {
    // Contain our own allocation/adapter failures, never exceptions from the
    // game's original callback. A failed gesture cannot remain publishable.
    PinInput().policy.Reset();
    PinInput().installed = false;
    try {
      PinReleaseRoots();
    } catch (...) {
    }
    try {
      spdlog::warn("[ShipPinInput] native pin presentation became unavailable; gesture cancelled");
    } catch (...) {
    }
    return false;
  }
}
void PinHighlight(ShipTileWidget* tile, PinHighlightKind highlight)
{
  if (PinIdentity(tile))
    PinStyleTile(PinRect(tile), highlight);
}
void PinHighlightSource(ShipTileWidget* tile)
{
  if (!PinInput().source_highlighted && PinIdentity(tile)) {
    PinHighlight(tile, PinHighlightKind::Source);
    PinInput().source_highlighted = true;
  }
}
void PinRefreshTile(ShipTileWidget* tile)
{
  auto*      rect     = PinRect(tile);
  const auto identity = PinIdentity(tile);
  PinStyleTile(rect, PinHighlightKind::None);
  if (!ship_pins::InputReady() || !identity) {
    PinVisible(PinChild(rect, kPinBadge), false);
    return;
  }
  const auto fleet = tile->TryCall<"get_Context", FleetPlayerData*>();
  (void)PinDrawBadge(rect, kPinBadge, {1, 0}, {-3, 8}, fleet && ship_pins::IsPinned(fleet.value));
}
void PinBind(ShipTileWidget* tile)
{
  auto& state = PinInput();
  if (!state.installed) {
    PinVisible(PinChild(PinRect(tile), kPinBadge), false);
    return;
  }
  state.group_refresh_frames = 3;
  std::erase_if(state.tiles, [](const PinTile& row) { return row.tile.Get() == nullptr; });
  auto*      row     = PinRecord(tile);
  auto*      context = tile->PinContext();
  const auto fleet   = tile->TryCall<"get_Context", FleetPlayerData*>();
  const auto id      = context && fleet && fleet.value ? fleet.value->ShipInstanceId() : std::nullopt;
  if (!row && context && state.tiles.size() < 4096) {
    auto weak = il2cpp::GcHandle::Weak(tile);
    if (weak) {
      state.tiles.push_back({std::move(weak), {}, 0, 0});
      row = &state.tiles.back();
    }
  }
  if (!row && context && state.installed) {
    state.installed = false;
    state.policy.Reset();
    spdlog::warn("[ShipPinInput] weak tile custody unavailable; pin input disabled rather than guessing drop targets");
  }
  if (row) {
    // Every SetWidgetData starts another native presentation episode, including
    // same-context/same-ship rebinds. No in-flight drag or click crosses that bind.
    state.policy.InvalidateTile(PinToken(tile));
    row->ship = id.value_or(0);
    if (!context)
      row->context.Reset();
    else if (row->context.Get() != context)
      row->context = il2cpp::GcHandle::Weak(context);
    if (context && !row->context) {
      state.installed = false;
      state.policy.Reset();
      spdlog::warn("[ShipPinInput] selection context custody unavailable; pin input disabled");
    }
    if (state.generation == std::numeric_limits<uint64_t>::max()) {
      state.installed = false;
      state.policy.Reset();
    } else {
      row->generation = ++state.generation;
    }
  }
  if (!state.policy.Source())
    PinReleaseRoots();
  PinRefreshTile(tile);
}
ShipTileWidget* PinTileAt(unity::Vector2 position, bool badge_only, unity::ScrollRect* scroll = nullptr)
{
  for (const auto& row : PinInput().tiles) {
    auto* tile = row.tile.Get<ShipTileWidget>();
    if (!tile || !PinIdentity(tile) || (scroll && PinScroll(tile) != scroll))
      continue;
    if (PinContains(badge_only ? PinBadge(tile) : PinRect(tile), position).value_or(false))
      return tile;
  }
  return nullptr;
}
ShipTileWidget* PinAnyTileAt(unity::Vector2 position, unity::ScrollRect* scroll, ShipManagementViewContext* context)
{
  for (const auto& row : PinInput().tiles) {
    auto* tile = row.tile.Get<ShipTileWidget>();
    if (tile && tile->isActiveAndEnabled() && tile->PinContext() == context && row.context.Get() == context
        && PinScroll(tile) == scroll && PinContains(PinRect(tile), position).value_or(false))
      return tile;
  }
  return nullptr;
}
unity::RectTransform* PinRefreshBand(unity::ScrollRect* scroll, ShipManagementViewContext* context)
{
  auto* content = scroll ? scroll->content() : nullptr;
  if (!content)
    return nullptr;
  if (!context || !ship_pins::InputReady()) {
    PinVisible(PinChild(content, kPinBand), false);
    return nullptr;
  }
  float left = 0, right = 0, bottom = 0, top = 0;
  bool  found = false;
  for (const auto& row : PinInput().tiles) {
    auto* tile = row.tile.Get<ShipTileWidget>();
    if (!tile || !PinIdentity(tile) || PinScroll(tile) != scroll || row.context.Get() != context)
      continue;
    const auto fleet = tile->TryCall<"get_Context", FleetPlayerData*>();
    if (!fleet || !ship_pins::IsPinned(fleet.value))
      continue;
    // Decorations and overflowing text descendants must not enlarge the card's pin/drop rectangle.
    const auto bounds = unity::RectTransformUtility::OwnBounds(content, PinRect(tile));
    if (!bounds || !std::isfinite(bounds.value.center.x) || !std::isfinite(bounds.value.center.y)
        || !std::isfinite(bounds.value.extents.x) || !std::isfinite(bounds.value.extents.y)
        || bounds.value.extents.x <= 0 || bounds.value.extents.y <= 0)
      continue;
    const auto& b  = bounds.value;
    const float x0 = b.center.x - b.extents.x, x1 = b.center.x + b.extents.x;
    const float y0 = b.center.y - b.extents.y, y1 = b.center.y + b.extents.y;
    if (!found) {
      left   = x0;
      right  = x1;
      bottom = y0;
      top    = y1;
      found  = true;
    } else {
      left   = std::min(left, x0);
      right  = std::max(right, x1);
      bottom = std::min(bottom, y0);
      top    = std::max(top, y1);
    }
  }
  if (!found) {
    PinVisible(PinChild(content, kPinBand), false);
    return nullptr;
  }
  const auto pivot = content->TryCall<"get_pivot", unity::Vector2>();
  if (!pivot)
    return nullptr;
  auto* band = PinSurface(content, kPinBand, pivot.value, {0.5f, 0.5f}, {right - left + 20, top - bottom + 20},
                          {(left + right) * 0.5f, (top + bottom) * 0.5f}, {}, true);
  if (!band
      || !PinStyleBand(band, PinInput().band.Get() == band && PinInput().policy.Dragging(),
                       PinInput().band.Get() == band && PinInput().group_hovered))
    return nullptr;
  band->SetAsFirstSibling();
  return band;
}
void PinDrawCue(unity::RectTransform* viewport, bool hovered)
{
  auto&      state = PinInput();
  const auto rect  = viewport ? viewport->TryCall<"get_rect", unity::Rect>() : il2cpp::CallResult<unity::Rect>{};
  if (!rect || !std::isfinite(rect.value.width) || !std::isfinite(rect.value.height) || rect.value.width < 16
      || rect.value.height < 16) {
    PinHideCue();
    return;
  }
  if (state.cue_viewport.Get() != viewport)
    PinHideCue();
  if (!state.cue_viewport)
    state.cue_viewport = il2cpp::GcHandle::Weak(viewport);
  if (!state.cue_viewport)
    return;
  const unity::Vector2 size{std::min(90.0f, rect.value.width - 8), std::min(90.0f, rect.value.height - 8)};
  auto*                cue =
      PinSurface(viewport, kPinCue, {0, 0.5f}, {0, 0.5f}, size, {4, 0},
                 hovered ? unity::Color{0.39f, 0.30f, 0.03f, 0.98f} : unity::Color{0.25f, 0.20f, 0.05f, 0.88f}, true);
  auto* label_rect = PinChild(cue, kPinCueLabel);
  if (!label_rect && cue) {
    il2cpp::GcHandle root;
    label_rect   = unity::CreateUiObject(kPinCueLabel, cue, root);
    auto* object = label_rect ? label_rect->gameObject() : nullptr;
    if (!object || !object->AddComponent<unity::TextMeshProUGUI>()) {
      if (object)
        unity::Destroy(object);
      label_rect = nullptr;
    }
  }
  auto* object = label_rect ? label_rect->gameObject() : nullptr;
  auto* label  = object ? object->GetComponent<unity::TextMeshProUGUI>() : nullptr;
  if (!cue || !label || !label_rect) {
    PinVisible(cue, false);
    PinHideCue();
    return;
  }
  label_rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {size.x - 8, size.y - 8}, {});
  label->SetText("Pin here");
  label->SetFontSize(13);
  label->SetAlignment(unity::TMP_Text::Alignment::Center);
  label->SetRaycastTarget(false);
  label->SetEnableWordWrapping(false);
  label->SetColor(hovered ? unity::Color{1.0f, 0.96f, 0.72f, 1.0f} : unity::Color{0.98f, 0.87f, 0.56f, 0.98f});
  PinVisible(label_rect, true);
  state.cue = il2cpp::GcHandle::Weak(cue);
  if (!state.cue
      || !PinEdge(viewport, kPinCueEdge, {0, 0.5f}, {0, 0.5f}, {0, 0.5f}, {3, size.y}, {1, 0},
                  hovered ? unity::Color{1.0f, 0.94f, 0.57f, 1.0f} : unity::Color{0.96f, 0.78f, 0.34f, 0.9f}, true,
                  false, true)) {
    PinVisible(cue, false);
    PinHideCue();
    return;
  }
  cue->Call<"SetAsLastSibling">();
  if (auto* edge = PinChild(viewport, kPinCueEdge))
    edge->Call<"SetAsLastSibling">();
}
ShipTileWidget* PinSource()
{
  auto*      tile     = PinInput().source.Get<ShipTileWidget>();
  const auto expected = PinInput().policy.Source();
  return tile && expected && PinIdentity(tile) == expected ? tile : nullptr;
}
bool PinHasGroup(unity::ScrollRect* scroll, ShipManagementViewContext* context)
{
  for (const auto& row : PinInput().tiles) {
    auto* tile = row.tile.Get<ShipTileWidget>();
    if (!tile || row.context.Get() != context || PinScroll(tile) != scroll || !PinIdentity(tile))
      continue;
    const auto fleet = tile->TryCall<"get_Context", FleetPlayerData*>();
    if (!fleet || ship_pins::IsPinned(fleet.value))
      return true; // Missing geometry/data must not turn an existing group into a first-pin invitation.
  }
  return false;
}
void PinPreview(unity::Vector2 position)
{
  auto& state  = PinInput();
  auto* source = PinSource();
  auto* scroll = state.scroll.Get<unity::ScrollRect>();
  if (!source || !scroll)
    return;
  auto* band = PinRefreshBand(scroll, source->PinContext());
  if (state.band.Get() != band) {
    if (auto* previous = state.band.Get<unity::RectTransform>())
      (void)PinStyleBand(previous, false, false);
    state.band = il2cpp::GcHandle::Weak(band);
  }
  if (band && !state.band) {
    (void)PinStyleBand(band, false, false);
    state.policy.Cancel();
    PinReleaseRoots();
    return;
  }
  auto* target = PinTileAt(position, false, scroll);
  if (target && target->PinContext() != source->PinContext())
    target = nullptr;
  const auto target_data =
      target ? target->TryCall<"get_Context", FleetPlayerData*>() : il2cpp::CallResult<FleetPlayerData*>{};
  // Keep preview eligibility aligned with the existing checked band/drop rule when geometry is unavailable.
  state.group_hovered = PinContains(band, position).value_or(false);
  if (band)
    (void)PinStyleBand(band, true, state.group_hovered);
  if (!band && !PinHasGroup(scroll, source->PinContext())) {
    PinDrawCue(PinViewport(scroll), PinContains(state.cue.Get<unity::RectTransform>(), position).value_or(false));
  } else {
    PinHideCue();
  }
  PinHighlightSource(source);
  if (auto* previous = state.target.Get<ShipTileWidget>(); previous && previous != source)
    PinRefreshTile(previous);
  const auto source_data   = source->TryCall<"get_Context", FleetPlayerData*>();
  const bool source_pinned = source_data && ship_pins::IsPinned(source_data.value);
  if ((state.group_hovered && (!target_data || !ship_pins::IsPinned(target_data.value)))
      || (!state.group_hovered && !source_pinned))
    target = nullptr;
  state.target = il2cpp::GcHandle::Weak(target);
  if (target && target != source)
    PinHighlight(target, state.group_hovered ? PinHighlightKind::Target : PinHighlightKind::UnpinTarget);
}
void PinDrop(ShipTileWidget* source, unity::ScrollRect* scroll, unity::Vector2 position, pointer::Identity expected)
{
  if (!source || !scroll || !expected || PinIdentity(source) != expected
      || !PinContains(PinViewport(scroll), position).value_or(false))
    return;
  auto*      context = source->PinContext();
  const auto fleet   = source->TryCall<"get_Context", FleetPlayerData*>();
  if (!context || !fleet || !fleet.value)
    return;
  const bool source_pinned = ship_pins::IsPinned(fleet.value);
  auto*      band          = PinRefreshBand(scroll, context);
  const bool in_cue        = PinContains(PinInput().cue.Get<unity::RectTransform>(), position).value_or(false);
  auto*      hovered       = PinAnyTileAt(position, scroll, context);
  if (hovered && !PinIdentity(hovered) && !in_cue)
    return;
  if (in_cue) {
    if (!source_pinned)
      (void)ship_pins::Place(fleet.value, {}, context);
  } else if (PinContains(band, position).value_or(false)) {
    auto*                  target = hovered;
    std::optional<int64_t> target_id;
    if (target) {
      const auto data = target->TryCall<"get_Context", FleetPlayerData*>();
      if (data && PinIdentity(target) && ship_pins::IsPinned(data.value))
        target_id = data.value->ShipInstanceId();
    }
    (void)ship_pins::Place(fleet.value, target_id, context);
  } else if (source_pinned) {
    (void)ship_pins::Toggle(fleet.value, context);
  }
}
void ShipTileWidget_SetWidgetData_SharedHook(auto original, ShipTileWidget* _this)
{
  original(_this);
  if (PinInput().installed)
    (void)PinUiAttempt([&] { PinBind(_this); });
  try {
    ship_tech_indicators::Bind(_this);
  } catch (...) {
    static bool reported = false;
    if (!std::exchange(reported, true)) {
      try {
        spdlog::warn("[ShipTechIndicators] native tile decoration unavailable; original binding retained");
      } catch (...) {
      }
    }
  }
}
void ScrollRect_OnInitializePotentialDrag_PinHook(auto original, unity::ScrollRect* scroll,
                                                  unity::PointerEventData* data)
{
  original(scroll, data);
  (void)PinUiAttempt([&] {
    auto&      state    = PinInput();
    const auto button   = data ? data->Button() : il2cpp::CallResult<int32_t>{};
    const auto id       = data ? data->PointerId() : il2cpp::CallResult<int32_t>{};
    const auto position = PinPosition(data, true);
    if (!state.installed || !ship_pins::InputReady() || !button || button.value != 0 || !id || id.value != -1
        || !position || !unity::Input::GetMouseButtonDown(0)) {
      state.policy.ForgetClick();
      return;
    }
    auto*      tile     = PinTileAt({position->x, position->y}, true, scroll);
    const auto identity = tile ? PinIdentity(tile) : pointer::Identity{};
    if (!state.policy.Press(identity, PinToken(scroll), id.value, *position, pointer::Clock::now(), PinInputEpoch())) {
      if (!state.policy.Source())
        PinReleaseRoots();
      return;
    }
    PinReleaseRoots();
    state.source = il2cpp::GcHandle::Weak(tile);
    state.scroll = il2cpp::GcHandle::Weak(scroll);
    if (!state.source || !state.scroll)
      state.policy.Cancel();
  });
}
void ScrollRect_OnBeginDrag_PinHook(auto original, unity::ScrollRect* scroll, unity::PointerEventData* data)
{
  bool consumed = false;
  (void)PinUiAttempt([&] {
    auto&      state    = PinInput();
    const auto id       = data ? data->PointerId() : il2cpp::CallResult<int32_t>{};
    const auto position = PinPosition(data);
    consumed =
        id
        && state.policy.Begin(PinToken(scroll), id.value, position, PinSource() != nullptr, pointer::Clock::now())
               == pointer::Route::Consume;
    if (consumed && state.policy.Dragging() && position) {
      PinPreview({position->x, position->y});
    }
    if (!state.policy.Source())
      PinReleaseRoots();
  });
  if (!consumed)
    original(scroll, data);
}
void ScrollRect_OnDrag_PinHook(auto original, unity::ScrollRect* scroll, unity::PointerEventData* data)
{
  bool consumed = false;
  (void)PinUiAttempt([&] {
    auto&      state    = PinInput();
    const auto id       = data ? data->PointerId() : il2cpp::CallResult<int32_t>{};
    const auto position = PinPosition(data);
    consumed =
        id
        && state.policy.Drag(PinToken(scroll), id.value, position, PinSource() != nullptr) == pointer::Route::Consume;
    if (consumed && state.policy.Dragging() && position)
      PinPreview({position->x, position->y});
    if (!state.policy.Source())
      PinReleaseRoots();
  });
  if (!consumed)
    original(scroll, data);
}
void ScrollRect_OnEndDrag_PinHook(auto original, unity::ScrollRect* scroll, unity::PointerEventData* data)
{
  bool consumed = false;
  (void)PinUiAttempt([&] {
    auto&      state    = PinInput();
    const auto expected = state.policy.Source();
    auto       root     = il2cpp::GcHandle::Strong(PinSource());
    auto*      source   = root.Get<ShipTileWidget>();
    const auto id       = data ? data->PointerId() : il2cpp::CallResult<int32_t>{};
    const auto position = PinPosition(data);
    const auto decision =
        id ? state.policy.End(PinToken(scroll), id.value, source && expected && PinIdentity(source) == expected,
                              position.has_value())
           : pointer::Policy::EndResult{pointer::Route::Native, false};
    consumed = decision.route == pointer::Route::Consume;
    if (decision.commit && position)
      PinDrop(source, scroll, {position->x, position->y}, expected);
    if (!state.policy.Source())
      PinReleaseRoots();
    ship_pin_input::RefreshVisible();
  });
  if (!consumed)
    original(scroll, data);
}
} // namespace

bool EnsureShipTileBinding()
{
  static const bool installed = [] {
    const auto bind = ShipTileWidget::Class::Method<void()>("SetWidgetData");
    if (bind && bind.Info()->klass != ShipTileWidget::Class::Raw()) {
      spdlog::warn("[ShipTileBinding] tile has no own SetWidgetData override; shared widget binding is left native");
      return false;
    }
    il2cpp::HookSet hooks("ShipTileBinding");
    IL2CPP_STAGE(hooks, bind, ShipTileWidget_SetWidgetData_SharedHook);
    return hooks.Commit();
  }();
  return installed;
}

namespace ship_pin_input
{
bool Install()
{
  auto& state = PinInput();
  if (state.attempted)
    return state.installed;
  state.attempted = true;
  // Installation may resolve engine entry points, but must not read Unity's
  // frame state before the first real frame. Runtime input still requires a
  // qualified nonzero PinInputEpoch for each interaction.
  if (!unity::Input::PointerAvailable() || !unity::Time::FrameCountAvailable()
      || !ShipSelectionViewController::Class::HasField<"m_context", ShipManagementViewContext*>()
      || !unity::PointerEventData::Class::HasProperty<"position", unity::Vector2>()
      || !unity::PointerEventData::Class::HasProperty<"pressPosition", unity::Vector2>()
      || !unity::PointerEventData::Class::HasProperty<"pointerId", int32_t>()
      || !unity::PointerEventData::Class::HasProperty<"button", int32_t>()
      || !unity::Canvas::Class::HasProperty<"worldCamera", unity::Camera*>()
      || !unity::RectTransformUtility::Class::StaticMethod<bool(unity::RectTransform*, unity::Vector2, unity::Camera*)>(
          "RectangleContainsScreenPoint")
      || !unity::RectTransformUtility::OwnBoundsAvailable())
    return false;
  if (!EnsureShipTileBinding())
    return false;
  il2cpp::HookSet hooks("ShipPinInput");
  IL2CPP_STAGE(hooks, unity::ScrollRect::Class::Method<void(unity::PointerEventData*)>("OnInitializePotentialDrag"),
               ScrollRect_OnInitializePotentialDrag_PinHook);
  IL2CPP_STAGE(hooks, unity::ScrollRect::Class::Method<void(unity::PointerEventData*)>("OnBeginDrag"),
               ScrollRect_OnBeginDrag_PinHook);
  IL2CPP_STAGE(hooks, unity::ScrollRect::Class::Method<void(unity::PointerEventData*)>("OnDrag"),
               ScrollRect_OnDrag_PinHook);
  IL2CPP_STAGE(hooks, unity::ScrollRect::Class::Method<void(unity::PointerEventData*)>("OnEndDrag"),
               ScrollRect_OnEndDrag_PinHook);
  state.installed = hooks.Commit();
  return state.installed;
}
bool Available()
{ return PinInput().installed && PinInputEpoch() != 0; }
bool ConsumeTileClick(ShipTileWidget* tile)
{
  bool consumed = false;
  (void)PinUiAttempt([&] {
    auto& state = PinInput();
    if (!state.installed || !ship_pins::InputReady())
      return;
    const auto identity = PinIdentity(tile);
    const auto position = PinMouse();
    const bool inside   = position && PinContains(PinBadge(tile), *position).value_or(false);
    const auto decision = state.policy.Release(identity, unity::Input::GetMouseButtonUp(0), inside,
                                               pointer::Clock::now(), PinInputEpoch());
    consumed            = decision != pointer::Click::Native;
    if (decision == pointer::Click::Toggle) {
      auto*      context = tile->PinContext();
      const auto fleet   = tile->TryCall<"get_Context", FleetPlayerData*>();
      if (fleet && context)
        (void)ship_pins::Toggle(fleet.value, context);
    }
    if (!state.policy.Source())
      PinReleaseRoots();
  });
  return consumed;
}
void RefreshVisible()
{
  (void)PinUiAttempt([&] {
    std::vector<std::pair<il2cpp::GcHandle, il2cpp::GcHandle>> groups;
    for (const auto& row : PinInput().tiles) {
      auto* tile = row.tile.Get<ShipTileWidget>();
      if (!tile || !tile->isActiveAndEnabled())
        continue;
      PinRefreshTile(tile);
      auto* scroll  = PinScroll(tile);
      auto* context = tile->PinContext();
      if (scroll && context && std::ranges::none_of(groups, [&](const auto& group) {
            return group.first.Get() == scroll && group.second.Get() == context;
          }))
        groups.emplace_back(il2cpp::GcHandle::Strong(scroll), il2cpp::GcHandle::Strong(context));
    }
    for (auto& group : groups)
      (void)PinRefreshBand(group.first.Get<unity::ScrollRect>(), group.second.Get<ShipManagementViewContext>());
  });
}
void Tick()
{
  (void)PinUiAttempt([&] {
    auto& state = PinInput();
    // No polling, traversal or repeated badge updates away from a mouse gesture.
    // Weak registry retirement happens on a native tile bind, not every frame.
    if (state.group_refresh_frames > 0 && --state.group_refresh_frames == 0)
      RefreshVisible();
    if (!state.policy.Active())
      return;
    if (const auto clicked = state.policy.LastClickIdentity(); clicked) {
      const auto record = std::ranges::find_if(state.tiles, [&](const PinTile& row) {
        return PinToken(row.tile.Get()) == clicked.tile && row.generation == clicked.generation;
      });
      auto*      tile   = record == state.tiles.end() ? nullptr : record->tile.Get<ShipTileWidget>();
      if (!tile || PinIdentity(tile) != clicked || !PinBadge(tile))
        state.policy.ForgetClick();
    }
    const auto now = pointer::Clock::now();
    state.policy.Tick(unity::Application::isFocused(), PinSource() != nullptr, unity::Input::GetMouseButton(0), now);
    if (!ship_pins::InputReady()) {
      state.policy.Reset();
      RefreshVisible();
    }
    if (state.policy.Source() && state.policy.Armed(now))
      PinHighlightSource(PinSource());
    else if (!state.policy.Source())
      PinReleaseRoots();
  });
}
} // namespace ship_pin_input
