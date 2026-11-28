// The configured mining-preview action cycles an occupied node through its
// foreign occupant and cargo, then returns to the same node.
#include "features/mining_preview.h"
#include "config.h"
#include "features/features.h"
#include "features/mining_preview_poll.h"

#include "game/mining_preview.h"

#include <spdlog/spdlog.h>
#include <unity/ui.h>

#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
using Clock       = std::chrono::steady_clock;
namespace binding = mining_preview_binding;
struct MiningPreviewSession {
  il2cpp::GcHandle            context;
  int64_t                     node = 0, fleet = 0;
  int32_t                     type        = 0;
  bool                        cargo_first = false, cargo_requested = false;
  mining_preview::PreviewPoll polling;
};
MiningPreviewSession& MiningSession()
{
  // Weak custody is released by Reset; do not invoke IL2CPP after its teardown.
  static auto* session = new MiningPreviewSession;
  return *session;
}
void MiningReset()
{ MiningSession() = {}; }

bool MiningSystemMap()
{ return Hub::CurrentSection() == SectionID::Navigation_System; }

// Presence checks retain source's shown/showing test. Full asset and navigation
// ownership readiness is required again before an input changes the selection.
std::optional<binding::Viewer> MiningShip(NavigationInteractionUIContext* context)
{
  const auto& a = binding::Native();
  for (auto* widget : unity::Instances<PreScanTargetWidget>()) {
    binding::Viewer observed;
    observed.source = il2cpp::GcHandle::Strong(widget);
    if (!observed.source || !observed.source.Get<PreScanTargetWidget>()->IsCardShown())
      continue;
    observed.context =
        il2cpp::GcHandle::Strong(binding::Read<NavigationInteractionUIContext*>(observed.source.Get(), a.context));
    if (!observed.context || observed.context.Get() != context)
      continue;
    auto       fleet = binding::Fleet(observed.source.Get<PreScanTargetWidget>());
    const auto id    = fleet ? planned_warp_binding::Scalar<"get_ID", int64_t>(fleet.Get<FleetDeployedData>())
                             : il2cpp::CallResult<int64_t>{};
    if (!id || id.value != MiningSession().fleet)
      continue;
    observed.poi = il2cpp::GcHandle::Strong(binding::Read<binding::Poi*>(observed.context.Get(), a.poi));
    if (observed.poi)
      return std::move(observed);
  }
  return {};
}

bool MiningValidateSession()
{
  auto& session = MiningSession();
  if (!session.context)
    return false;
  auto        context = il2cpp::GcHandle::Strong(session.context.Get());
  const auto& a       = binding::Native();
  const auto& control = Config::Get().control;
  auto        poi =
      context ? il2cpp::GcHandle::Strong(binding::Read<binding::Poi*>(context.Get(), a.poi)) : il2cpp::GcHandle{};
  if (!control.hotkeys_enabled || control.use_scopely_hotkeys || !MiningSystemMap() || !poi
      || binding::Read<int64_t>(poi.Get(), a.id) != session.fleet) {
    MiningReset();
    return false;
  }
  return true;
}

bool MiningBegin()
{
  const auto& a = binding::Native();
  if (!a.ready)
    return false;
  for (auto* widget : unity::Instances<MiningObjectViewerWidget>()) {
    auto observed = binding::Observe(widget);
    if (!observed)
      continue;
    const auto occupant = binding::ForeignOccupant(widget);
    if (!occupant)
      continue;
    MiningReset();
    auto& session   = MiningSession();
    session.context = il2cpp::GcHandle::Weak(observed.context.Get());
    if (!session.context)
      return false;
    session.node            = observed.id;
    session.type            = observed.type;
    session.fleet           = occupant.id;
    session.cargo_first     = Config::Get().ui.mining_preview_cargo_first;
    session.cargo_requested = session.cargo_first;
    session.polling.Start(Clock::now());
    // Native occupant ship POI code used by play; node type is retained exactly.
    if (binding::Select(observed.context.Get<NavigationInteractionUIContext>(), session.node, session.fleet, 5))
      return true;
    MiningReset();
    return false;
  }
  return false;
}

#if defined(STFC_DEVELOPMENT_PACKAGE) && STFC_DEVELOPMENT_PACKAGE
// --- Selected mining view OPC indicator (DEV opt-in) ---

constexpr unity::Color kMiningOpcAmber{1.0f, 0.66f, 0.12f, 1.0f};

struct MiningOpcIndicator {
  il2cpp::GcHandle  card, anchor, object, context, loader, poi;
  il2cpp::GcHandle  slot, occupant;
  bool              node  = false;
  int64_t           fleet = 0;
  Clock::time_point next{};
};

MiningOpcIndicator& MiningIndicator()
{
  // Keep the overlay wrapper alive so we can always hide it while its native parent exists.
  // The native viewer and its occupants remain weak. Do not release handles after IL2CPP teardown.
  static auto* indicator = new MiningOpcIndicator;
  return *indicator;
}

bool HideMiningIndicator()
{
  auto& indicator = MiningIndicator();
  if (indicator.fleet == 0)
    return true;
  auto*      object = indicator.object.Get<unity::GameObject>();
  const auto alive  = unity::TryAlive(object);
  // Keep custody until we have hidden the overlay or confirmed that Unity destroyed it.
  if (!alive || (alive.value && !object->TryCall<"SetActive", void(bool)>(false)))
    return false;
  if (!alive.value)
    indicator.object.Reset();

  indicator.context.Reset();
  indicator.loader.Reset();
  indicator.poi.Reset();
  indicator.slot.Reset();
  indicator.occupant.Reset();
  indicator.fleet = 0;
  return true;
}

bool SetMiningIndicatorRect(unity::RectTransform* rect, unity::Vector2 min, unity::Vector2 max,
                            unity::Vector2 offset_min, unity::Vector2 offset_max)
{
  return rect->TryCall<"set_anchorMin", void(unity::Vector2)>(min)
         && rect->TryCall<"set_anchorMax", void(unity::Vector2)>(max)
         && rect->TryCall<"set_offsetMin", void(unity::Vector2)>(offset_min)
         && rect->TryCall<"set_offsetMax", void(unity::Vector2)>(offset_max);
}

struct MiningOverlay {
  il2cpp::GcHandle      object;
  unity::RectTransform* rect  = nullptr;
  int32_t               layer = 0;
};

MiningOverlay CreateMiningOverlay(std::string_view name, unity::RectTransform* anchor)
{
  auto       parent = il2cpp::GcHandle::Strong(anchor->gameObject());
  const auto layer =
      parent ? parent.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!layer)
    return {};
  MiningOverlay overlay;
  overlay.rect = unity::CreateUiObject(name, anchor, overlay.object);
  auto* object = overlay.object.Get<unity::GameObject>();
  if (!object || !overlay.rect || !object->TryCall<"set_layer", void(int32_t)>(layer.value)) {
    if (object)
      unity::Destroy(object);
    return {};
  }
  overlay.layer = layer.value;
  return overlay;
}

il2cpp::GcHandle CreateMiningOutline(unity::RectTransform* anchor)
{
  auto overlay = CreateMiningOverlay("CommunityMod_MiningTargetOpc", anchor);
  if (!overlay.object)
    return {};
  auto* container = overlay.object.Get<unity::GameObject>();
  auto* rect      = overlay.rect;
  auto  layout    = il2cpp::GcHandle::Strong(container->AddComponent<unity::LayoutElement>());
  if (!layout || !layout.Get<unity::LayoutElement>()->ExcludeFromLayout()
      || !SetMiningIndicatorRect(rect, {0, 0}, {1, 1}, {0, 0}, {0, 0})) {
    unity::Destroy(container);
    return {};
  }
  struct Edge {
    unity::Vector2 min, max, offset_min, offset_max;
  };
  constexpr float               width = 2;
  constexpr std::array<Edge, 4> edges{{
      {{0, 0}, {0, 1}, {0, 0}, {width, 0}},
      {{1, 0}, {1, 1}, {-width, 0}, {0, 0}},
      {{0, 0}, {1, 0}, {0, 0}, {0, width}},
      {{0, 1}, {1, 1}, {0, -width}, {0, 0}},
  }};
  for (const auto& edge : edges) {
    il2cpp::GcHandle child;
    auto*            line   = unity::CreateUiObject("Edge", rect, child);
    auto*            object = child.Get<unity::GameObject>();
    auto             image  = il2cpp::GcHandle::Strong(object ? object->AddComponent<unity::Image>() : nullptr);
    if (!line || !image || !object->TryCall<"set_layer", void(int32_t)>(overlay.layer)
        || !SetMiningIndicatorRect(line, edge.min, edge.max, edge.offset_min, edge.offset_max)
        || !image.Get<unity::Image>()->TryCall<"set_raycastTarget", void(bool)>(false)
        || !image.Get<unity::Image>()->TryCall<"set_color", void(unity::Color)>(kMiningOpcAmber)
        || !object->TryCall<"SetActive", void(bool)>(true)) {
      unity::Destroy(container);
      return {};
    }
  }
  return std::move(overlay.object);
}

il2cpp::GcHandle CreateMiningNodeBadge(unity::RectTransform* anchor, il2cpp::Object* font)
{
  if (!font)
    return {};
  auto overlay = CreateMiningOverlay("CommunityMod_MiningNodeOpc", anchor);
  if (!overlay.object)
    return {};
  auto* object = overlay.object.Get<unity::GameObject>();
  auto  label  = il2cpp::GcHandle::Strong(object->AddComponent<unity::TextMeshProUGUI>());
  auto* badge  = label.Get<unity::TextMeshProUGUI>();
  if (!badge || !SetMiningIndicatorRect(overlay.rect, {1, 0.5f}, {1, 0.5f}, {6, -15}, {62, 15})
      || !badge->TryCall<"set_raycastTarget", void(bool)>(false)
      || !badge->TryCall<"set_alignment", void(unity::TMP_Text::Alignment)>(unity::TMP_Text::Alignment::Center)
      || !badge->TryCall<"set_fontSize", void(float)>(22.0f)
      || !badge->TryCall<"set_color", void(unity::Color)>(kMiningOpcAmber)
      || !badge->TryCall<"set_text", void(Il2CppString*)>(il2cpp::NewString("OPC"))) {
    unity::Destroy(object);
    return {};
  }
  badge->SetFont(font);
  return std::move(overlay.object);
}

bool MiningIndicatorSelected(ObjectViewerBaseWidget* card, NavigationInteractionUIContext* context,
                             binding::Loader* loader, binding::Poi* poi, bool node, binding::Slot* slot,
                             FleetDeployedData* occupant, int64_t id)
{
  if (!unity::Alive(card))
    return false;
  if (node) {
    if (!binding::ViewerVisible(card))
      return false;
    const auto current = binding::ForeignOccupant(static_cast<MiningObjectViewerWidget*>(card));
    if (!current || current.slot.Get() != slot || current.fleet.Get() != occupant || current.id != id)
      return false;
  } else if (!static_cast<PreScanTargetWidget*>(card)->IsCardShown()) {
    return false;
  }
  return binding::StillSelected(card, context, loader, poi);
}

void ShowMiningIndicator(const binding::Viewer& selected, const il2cpp::GcHandle& anchor,
                         const binding::Occupant& occupant = {}, il2cpp::Object* font = nullptr)
{
  auto&      indicator = MiningIndicator();
  auto*      card      = selected.source.Get<ObjectViewerBaseWidget>();
  const bool node      = static_cast<bool>(occupant);
  const auto id        = node ? occupant.id : selected.id;
  if (anchor && indicator.node == node && indicator.anchor.Get() == anchor.Get()
      && indicator.context.Get() == selected.context.Get() && indicator.card.Get() == card
      && indicator.loader.Get() == selected.loader.Get() && indicator.poi.Get() == selected.poi.Get()
      && indicator.fleet == id && indicator.slot.Get() == occupant.slot.Get()
      && indicator.occupant.Get() == occupant.fleet.Get() && unity::Alive(indicator.object.Get()))
    return;
  if (!HideMiningIndicator() || !anchor)
    return;
  const auto alive = unity::TryAlive(indicator.object.Get());
  if (!alive)
    return;
  auto object = std::move(indicator.object);
  if (!alive.value || indicator.anchor.Get() != anchor.Get() || indicator.node != node) {
    if (alive.value)
      unity::Destroy(object.Get());
    object = node ? CreateMiningNodeBadge(anchor.Get<unity::RectTransform>(), font)
                  : CreateMiningOutline(anchor.Get<unity::RectTransform>());
  }
  if (!object)
    return;

  // Native object creation and property calls can reenter card binding. Recheck before showing.
  auto card_ref    = il2cpp::GcHandle::Weak(card);
  auto anchor_ref  = il2cpp::GcHandle::Weak(anchor.Get());
  auto context_ref = il2cpp::GcHandle::Weak(selected.context.Get());
  auto loader_ref  = il2cpp::GcHandle::Weak(selected.loader.Get());
  auto poi_ref     = il2cpp::GcHandle::Weak(selected.poi.Get());
  auto slot_ref    = il2cpp::GcHandle::Weak(occupant.slot.Get());
  auto fleet_ref   = il2cpp::GcHandle::Weak(occupant.fleet.Get());
  if (!card_ref || !anchor_ref || !context_ref || !loader_ref || !poi_ref || (node && (!slot_ref || !fleet_ref))
      || !MiningIndicatorSelected(card, selected.context.Get<NavigationInteractionUIContext>(),
                                  selected.loader.Get<binding::Loader>(), selected.poi.Get<binding::Poi>(), node,
                                  occupant.slot.Get<binding::Slot>(), occupant.fleet.Get<FleetDeployedData>(), id)
      || !object.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(true)) {
    unity::Destroy(object.Get());
    return;
  }
  indicator.card     = std::move(card_ref);
  indicator.anchor   = std::move(anchor_ref);
  indicator.object   = std::move(object);
  indicator.context  = std::move(context_ref);
  indicator.loader   = std::move(loader_ref);
  indicator.poi      = std::move(poi_ref);
  indicator.slot     = std::move(slot_ref);
  indicator.occupant = std::move(fleet_ref);
  indicator.node     = node;
  indicator.fleet    = id;
}

void MiningTargetOpcFrame()
{
  auto& indicator = MiningIndicator();
  if (!Config::Get().ui.mining_target_opc || !MiningSystemMap()) {
    HideMiningIndicator();
    indicator.next = {};
    return;
  }

  if (indicator.fleet != 0) {
    auto card    = il2cpp::GcHandle::Strong(indicator.card.Get());
    auto context = il2cpp::GcHandle::Strong(indicator.context.Get());
    auto loader  = il2cpp::GcHandle::Strong(indicator.loader.Get());
    auto poi     = il2cpp::GcHandle::Strong(indicator.poi.Get());
    auto slot    = il2cpp::GcHandle::Strong(indicator.slot.Get());
    auto fleet   = il2cpp::GcHandle::Strong(indicator.occupant.Get());
    if (!MiningIndicatorSelected(card.Get<ObjectViewerBaseWidget>(), context.Get<NavigationInteractionUIContext>(),
                                 loader.Get<binding::Loader>(), poi.Get<binding::Poi>(), indicator.node,
                                 slot.Get<binding::Slot>(), fleet.Get<FleetDeployedData>(), indicator.fleet)
        && !HideMiningIndicator())
      return;
  }

  const auto now = Clock::now();
  if (now < indicator.next)
    return;
  indicator.next = now + std::chrono::milliseconds(250);
  for (auto* card : unity::Instances<MiningObjectViewerWidget>()) {
    const auto selected = binding::Observe(card);
    if (!selected)
      continue;
    const auto occupant = binding::ForeignOccupant(card);
    if (!occupant
        || !binding::TargetIsOpc(occupant.fleet.Get<FleetDeployedData>(), occupant.id,
                                 occupant.slot.Get<binding::Slot>()))
      continue;
    const auto status = binding::OccupiedStatus(card);
    ShowMiningIndicator(selected, status.anchor, occupant, status.font.Get<il2cpp::Object>());
    return;
  }

  for (auto* card : unity::Instances<PreScanTargetWidget>()) {
    auto selected = binding::Observe(card);
    if (!selected)
      continue;
    auto       fleet  = binding::Fleet(selected.source.Get<PreScanTargetWidget>());
    auto*      target = fleet.Get<FleetDeployedData>();
    const auto id     = target ? target->TryId() : il2cpp::CallResult<int64_t>{};
    if (id && id.value == selected.id && binding::TargetIsOpc(target, id.value)) {
      ShowMiningIndicator(selected, binding::CardAnchor(card));
      return;
    }
  }
  HideMiningIndicator();
}

bool InstallMiningTargetOpc()
{
  if (!Config::Get().patches.objecttracker) {
    spdlog::warn("[MiningTargetOpc] object tracking is disabled");
    return false;
  }
  if (binding::TargetOpcAvailable())
    return true;
  spdlog::warn("[MiningTargetOpc] native mining target cargo contract unavailable");
  return false;
}
#endif

bool InstallMiningPreview()
{
  if (!binding::Native().ready) {
    spdlog::warn("[MiningPreview] native node/occupant preview contract unavailable; native cargo controls retained");
    return false;
  }
  return true; // Frame/input composition only; installs no native detours.
}

void MiningPreviewFrame()
{ mining_preview::Update(); }
} // namespace

void mining_preview::Update()
{
  if (!MiningValidateSession())
    return;
  auto&      session = MiningSession();
  const auto now     = Clock::now();
  if (!session.polling.Due(now))
    return;
  auto       context = il2cpp::GcHandle::Strong(session.context.Get());
  const bool seen    = session.polling.Ready();
  auto       ship    = context ? MiningShip(context.Get<NavigationInteractionUIContext>()) : std::nullopt;
  const auto ready   = ship && !seen ? binding::Observe(ship->source.Get<PreScanTargetWidget>()) : binding::Viewer{};
  if (ship && (seen || (ready && ready.context.Get() == context.Get() && ready.id == session.fleet))) {
    if (!seen)
      session.polling.ObserveReady(now);
  } else if (seen || session.polling.Expired(now)) {
    MiningReset();
  }
}

bool mining_preview::Cycle()
{
  try {
    if (!MiningSystemMap() || !binding::Native().ready) {
      MiningReset();
      return false;
    }
    if (!MiningValidateSession())
      return MiningBegin();
    auto&      session = MiningSession();
    auto       context = il2cpp::GcHandle::Strong(session.context.Get());
    auto       present = context ? MiningShip(context.Get<NavigationInteractionUIContext>()) : std::nullopt;
    auto       ready   = present ? binding::Observe(present->source.Get<PreScanTargetWidget>()) : binding::Viewer{};
    const bool settled = ready && ready.context.Get() == context.Get() && ready.id == session.fleet;
    const auto now     = Clock::now();
    if (!settled) {
      if ((session.polling.Ready() && !present) || session.polling.Expired(now)) {
        MiningReset();
        return MiningBegin();
      }
      return true; // Keep input while this exact native popup finishes loading.
    }
    if (!session.polling.Ready())
      session.polling.ObserveReady(now);
    auto* widget        = ready.source.Get<PreScanTargetWidget>();
    auto  button        = il2cpp::GcHandle::Strong(widget->rewardsButtonWidget());
    auto* rewards_value = button ? button.Get<RewardsButtonWidget>()->rewardsController() : nullptr;
    auto  rewards       = il2cpp::GcHandle::Strong(rewards_value);
    if (!rewards)
      return true;
    const auto state = binding::Read<int32_t>(rewards.Get(), binding::Native().visibility_state);
    const bool visible =
        state == std::to_underlying(VisibilityState::Show) || state == std::to_underlying(VisibilityState::Visible);
    if (visible != session.cargo_first) {
      const bool selected =
          binding::Select(context.Get<NavigationInteractionUIContext>(), session.fleet, session.node, session.type);
      MiningReset();
      return selected;
    }
    session.cargo_requested = !visible;
    if (visible)
      rewards_value->Hide();
    else
      button.Get<RewardsButtonWidget>()->TryCall<"RewardsClicked", void>();
    return true;
  } catch (...) {
    MiningReset();
    return false;
  }
}

std::optional<bool> mining_preview::AutomaticCargo(RewardsButtonWidget* widget)
{
  auto& session = MiningSession();
  if (!session.context || !MiningSystemMap() || !widget)
    return {};
  auto navigation = il2cpp::GcHandle::Strong(session.context.Get());
  if (!navigation)
    return {};
  const auto  fleet_id  = session.fleet;
  const bool  requested = session.cargo_requested;
  const auto  context   = widget->TryCall<"get_Context", BattleTargetData*>();
  auto        data      = context ? il2cpp::GcHandle::Strong(context.value) : il2cpp::GcHandle{};
  const auto& a         = binding::Native();
  auto        fleet     = data ? il2cpp::GcHandle::Strong(binding::Read<FleetDeployedData*>(data.Get(), a.target_fleet))
                               : il2cpp::GcHandle{};
  const auto  id        = fleet ? planned_warp_binding::Scalar<"get_ID", int64_t>(fleet.Get<FleetDeployedData>())
                                : il2cpp::CallResult<int64_t>{};
  if (session.context.Get() != navigation.Get() || session.fleet != fleet_id || session.cargo_requested != requested)
    return {};
  return id && id.value == fleet_id ? std::optional(requested) : std::nullopt;
}

const Feature kMiningPreview{"MiningPreview", &Config::Patches::hotkeyhooks, InstallMiningPreview, MiningPreviewFrame};

#if defined(STFC_DEVELOPMENT_PACKAGE) && STFC_DEVELOPMENT_PACKAGE
const Feature kMiningTargetOpc{"MiningTargetOpc", &Config::Patches::opcindicatorhooks, InstallMiningTargetOpc,
                               MiningTargetOpcFrame};
#endif
