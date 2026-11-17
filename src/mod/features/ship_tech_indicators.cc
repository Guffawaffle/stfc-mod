// Swap Ship FT/CT artwork uses the game's ImageSelector and the exact native
// equipment/art identifiers. Pin input and tile binding retain their existing owner.
#include "features/ship_tech_indicators.h"
#include "config.h"
#include "features/features.h"

#include "game/ship_tech.h"

#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/tracker.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>
#include <vector>

namespace
{
using Selector = ship_tech_binding::ImageSelector;
constexpr const char* kIcons[]{"CommunityMod_SwapShipFT", "CommunityMod_SwapShipCT"};
constexpr const char* kBackings[]{"CommunityMod_SwapShipFTBacking", "CommunityMod_SwapShipCTBacking"};
constexpr uint8_t     kRoundedBackground[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x04, 0x67, 0x41, 0x4d, 0x41,
    0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc, 0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
    0x0e, 0xc3, 0x00, 0x00, 0x0e, 0xc3, 0x01, 0xc7, 0x6f, 0xa8, 0x64, 0x00, 0x00, 0x00, 0x48, 0x49, 0x44, 0x41,
    0x54, 0x58, 0x47, 0xed, 0xd7, 0xb1, 0x0d, 0x00, 0x30, 0x08, 0x03, 0x41, 0xf6, 0x5f, 0xda, 0x91, 0x52, 0x05,
    0xfa, 0x60, 0x17, 0xff, 0x12, 0xb5, 0xaf, 0xa5, 0x24, 0xd5, 0xb8, 0xdf, 0xb5, 0xbd, 0xcd, 0xe1, 0x59, 0x03,
    0xb8, 0xca, 0x00, 0x58, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04,
    0xc0, 0x89, 0xc8, 0xf8, 0x8c, 0x5c, 0x88, 0xbb, 0xfb, 0x02, 0xb6, 0x20, 0x6d, 0xef, 0x00, 0xba, 0xbb, 0xd1,
    0x02, 0x42, 0x5d, 0x7b, 0xec, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
struct ShipTechState {
  bool                                  installed = false, binding = false;
  bool                                  shown = true, backgrounds = false;
  il2cpp::GcHandle                      donor;
  il2cpp::GcHandle                      donor_context;
  std::chrono::steady_clock::time_point donor_retry{};
  bool                                  donor_fallback_attempted = false;
  std::vector<il2cpp::GcHandle>         tiles;
  unity::ImageAsset                     rounded;
};
ShipTechState& ShipTech()
{
  // Native hierarchy owns decorations; process state only roots the small shared
  // background asset and weakly observes native donors/tiles.
  static auto* state = new ShipTechState;
  return *state;
}

unity::RectTransform* TechRect(unity::Transform* transform)
{ return transform ? transform->As<unity::RectTransform>() : nullptr; }

unity::Transform* TechChild(unity::Transform* parent, const char* name)
{ return parent ? parent->Find(name) : nullptr; }

void TechShow(unity::Transform* transform, bool shown)
{
  if (auto* object = transform ? transform->gameObject() : nullptr)
    object->SetActive(shown);
}

void HideTech(unity::Transform* parent)
{
  for (const auto* name : kIcons)
    TechShow(TechChild(parent, name), false);
  for (const auto* name : kBackings)
    TechShow(TechChild(parent, name), false);
}

unity::Vector2 TechSize(unity::Transform* parent, bool inset)
{
  const auto* path     = "OverlayElements/TickItem";
  auto*       tick     = TechRect(parent ? parent->Find(path) : nullptr);
  const auto  measured = tick ? tick->TryCall<"get_sizeDelta", unity::Vector2>() : il2cpp::CallResult<unity::Vector2>{};
  auto        size     = measured ? measured.value : unity::Vector2{46, 46};
  if (!(size.x >= 12 && size.x <= 128 && size.y >= 12 && size.y <= 128))
    size = {46, 46};
  if (inset) {
    size.x -= 8;
    size.y -= 8;
  }
  return size;
}

bool TechSelected(unity::Transform* parent)
{
  auto* tick   = parent ? parent->Find("OverlayElements/TickItem") : nullptr;
  auto* object = tick ? tick->gameObject() : nullptr;
  return object && object->activeInHierarchy();
}

Selector* TechDonor(ShipTileWidget* tile, ShipManagementViewContext* context,
                    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now())
{
  auto& state = ShipTech();
  if (auto* donor = state.donor.Get<Selector>(); unity::Alive(donor))
    return donor;
  state.donor.Reset();
  auto fallback = [&] { return ship_tech_binding::ShipImage(tile); };
  if (now < state.donor_retry)
    return fallback();
  state.donor_retry = now + std::chrono::seconds(5);
  auto context_root = il2cpp::GcHandle::Strong(context);
  if (!context_root)
    return fallback();
  // Snapshot custody before invoking any owner/slot getters. The existing
  // tracker owns discovery; this adds no hook or recurring Unity resource scan.
  std::vector<il2cpp::GcHandle> owners;
  for (auto* owner : unity::Instances<ShipManagementViewController>()) {
    auto root = il2cpp::GcHandle::Strong(owner);
    if (root)
      owners.push_back(std::move(root));
  }
  for (auto it = owners.rbegin(); it != owners.rend(); ++it) {
    if (auto* donor = ship_tech_binding::OwnedTokenSource(it->Get<ShipManagementViewController>())) {
      state.donor = il2cpp::GcHandle::Weak(donor);
      return donor;
    }
  }
  // Preserve the frozen once-per-selection-context global fallback. Failed
  // weak custody cannot authorize an unbounded stream of fallback scans.
  if (state.donor_context.Get() != context) {
    state.donor_context            = il2cpp::GcHandle::Weak(context);
    state.donor_fallback_attempted = false;
  }
  if (state.donor_context && !std::exchange(state.donor_fallback_attempted, true)) {
    if (auto* donor = ship_tech_binding::TokenSource()) {
      state.donor = il2cpp::GcHandle::Weak(donor);
      return donor;
    }
  }
  // Preserve play's native ship-image fallback when no tech token prefab is loaded.
  return ship_tech_binding::ShipImage(tile);
}

bool ExcludeTechLayout(unity::GameObject* object)
{
  auto* layout = object ? object->GetComponent<unity::LayoutElement>() : nullptr;
  if (!layout && object)
    layout = object->AddComponent<unity::LayoutElement>();
  return layout && layout->ExcludeFromLayout();
}

unity::GameObject* TechIcon(ShipTileWidget* tile, ShipManagementViewContext* context, unity::Transform* parent,
                            size_t index)
{
  if (auto* child = TechChild(parent, kIcons[index]))
    return child->gameObject();
  auto  source   = il2cpp::GcHandle::Strong(TechDonor(tile, context));
  auto* donor    = source.Get<Selector>();
  auto* original = donor ? donor->gameObject() : nullptr;
  auto* object   = original ? unity::Instantiate(original, parent, false) : nullptr;
  auto  root     = il2cpp::GcHandle::Strong(object);
  if (!root)
    return nullptr;
  object->SetActive(false);
  if (!object->TryCall<"set_name", void>(il2cpp::NewString(kIcons[index]))) {
    unity::Destroy(object);
    return nullptr;
  }
  auto* selector = object->GetComponent<Selector>();
  auto* image    = selector ? selector->Target() : nullptr;
  if (!image || !ExcludeTechLayout(object)) {
    unity::Destroy(object);
    return nullptr;
  }
  image->SetRaycastTarget(false);
  image->SetPreserveAspect(true);
  return object;
}

unity::GameObject* TechBacking(unity::Transform* parent, size_t index)
{
  if (auto* child = TechChild(parent, kBackings[index]))
    return child->gameObject();
  il2cpp::GcHandle root;
  auto*            rect   = unity::CreateUiObject(kBackings[index], parent, root);
  auto*            object = rect ? rect->gameObject() : nullptr;
  auto*            image  = object ? object->AddComponent<unity::Image>() : nullptr;
  if (!image || !ExcludeTechLayout(object)) {
    if (object)
      unity::Destroy(object);
    return nullptr;
  }
  image->SetRaycastTarget(false);
  image->SetColor({0.01f, 0.02f, 0.03f, 0.46f});
  auto& rounded = ShipTech().rounded;
  if (rounded.Load(kRoundedBackground, sizeof(kRoundedBackground)))
    image->SetSprite(rounded.Sprite());
  return object;
}

void UpdateTechIndicator(ShipTileWidget* tile, ShipManagementViewContext* context, unity::Transform* parent,
                         size_t index, int64_t art)
{
  if (art <= 0)
    return;
  auto* object      = TechIcon(tile, context, parent, index);
  auto  object_root = il2cpp::GcHandle::Strong(object);
  auto* selector    = object ? object->GetComponent<Selector>() : nullptr;
  auto* rect        = TechRect(object ? object->transform() : nullptr);
  if (!object_root || !selector || !rect)
    return;
  const bool           selected = TechSelected(parent);
  const unity::Vector2 anchor   = index == 0 ? unity::Vector2{0, 1} : unity::Vector2{1, 1};
  const unity::Vector2 position =
      index == 0 ? unity::Vector2{selected ? 16.0f : 8.0f, -3} : unity::Vector2{selected ? -56.0f : -8.0f, -3};
  rect->Place(anchor, anchor, anchor, TechSize(parent, true), position);
  if (!selector->SetArt(art))
    return;
  if (Config::Get().ui.show_ship_tech_indicator_backgrounds) {
    auto* backing      = TechBacking(parent, index);
    auto* backing_rect = TechRect(backing ? backing->transform() : nullptr);
    if (backing_rect) {
      const unity::Vector2 backing_position{position.x + (anchor.x - 0.5f) * 8, position.y + 4};
      backing_rect->Place(anchor, anchor, anchor, TechSize(parent, false), backing_position);
      backing_rect->Call<"SetAsLastSibling">();
      backing->SetActive(true);
    }
  }
  rect->Call<"SetAsLastSibling">();
  object->SetActive(true);
}

void ShipTechFrame()
{
  auto&       state = ShipTech();
  const auto& ui    = Config::Get().ui;
  if (state.shown == ui.show_ship_tech_indicators && state.backgrounds == ui.show_ship_tech_indicator_backgrounds)
    return;
  state.shown       = ui.show_ship_tech_indicators;
  state.backgrounds = ui.show_ship_tech_indicator_backgrounds;
  std::vector<il2cpp::GcHandle> active;
  for (const auto& weak : state.tiles) {
    auto root = il2cpp::GcHandle::Strong(weak.Get());
    if (unity::Alive(root.Get()))
      active.push_back(std::move(root));
  }
  for (const auto& root : active)
    ship_tech_indicators::Bind(root.Get<ShipTileWidget>());
}

bool InstallShipTechIndicators()
{
  if (!FleetPlayerData::ShipIdentityAvailable()) {
    spdlog::warn("[ShipTechIndicators] native ship-instance identity unavailable; tile artwork unchanged");
    return false;
  }
  if (!ship_tech_binding::Available()) {
    spdlog::warn("[ShipTechIndicators] native equipment/art preflight unavailable (nullable-int64-slot={} "
                 "scalar-art-setter={}); tile artwork unchanged",
                 ship_tech_binding::SlotItemGetter() != nullptr,
                 static_cast<bool>(ship_tech_binding::ImageSelector::ArtSetter()));
    return false;
  }
  ShipTech().installed = EnsureShipTileBinding();
  return ShipTech().installed;
}
} // namespace

namespace ship_tech_indicators
{
bool Available()
{ return ShipTech().installed; }

void Bind(ShipTileWidget* tile)
{
  auto& state = ShipTech();
  if (!state.installed || std::exchange(state.binding, true))
    return;
  struct Release {
    bool& value;
    ~Release()
    { value = false; }
  } release{state.binding};
  auto  tile_root = il2cpp::GcHandle::Strong(tile);
  auto* parent    = tile_root ? tile->Transform() : nullptr;
  if (!parent)
    return;
  HideTech(parent);
  auto selection = il2cpp::GcHandle::Strong(tile->PinContext());
  if (!selection)
    return;
  std::erase_if(state.tiles, [](const auto& weak) { return !weak.Get(); });
  if (std::ranges::none_of(state.tiles, [tile](const auto& weak) { return weak.Get() == tile; })) {
    auto weak = il2cpp::GcHandle::Weak(tile);
    if (weak)
      state.tiles.push_back(std::move(weak));
  }
  if (!Config::Get().ui.show_ship_tech_indicators)
    return;
  const auto observed   = tile->TryCall<"get_Context", FleetPlayerData*>();
  auto       fleet_root = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  auto*      fleet      = fleet_root.Get<FleetPlayerData>();
  const auto ship       = fleet ? fleet->ShipInstanceId() : std::nullopt;
  if (!ship)
    return;
  const auto art = ship_tech_binding::EquippedArt(*ship);
  if (!art)
    return;
  for (size_t index = 0; index < 2; ++index) {
    const auto current = tile->TryCall<"get_Context", FleetPlayerData*>();
    if (!current || current.value != fleet || tile->PinContext() != selection.Get())
      return;
    UpdateTechIndicator(tile, selection.Get<ShipManagementViewContext>(), parent, index, art.value[index]);
  }
}
} // namespace ship_tech_indicators

const Feature kShipTechIndicators{"ShipTechIndicators", &Config::Patches::shiptechindicatorhooks,
                                  InstallShipTechIndicators, ShipTechFrame};
