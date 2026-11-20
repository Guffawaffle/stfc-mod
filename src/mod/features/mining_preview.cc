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
    if (!observed
        || binding::Read<int32_t>(observed.source.Get(), a.occupied)
               != std::to_underlying(OccupiedState::OtherPlayerOccupied))
      continue;
    auto slot  = il2cpp::GcHandle::Strong(binding::Read<binding::Slot*>(observed.source.Get(), a.node_slot));
    auto fleet = slot ? il2cpp::GcHandle::Strong(binding::Read<FleetDeployedData*>(slot.Get(), a.slot_fleet))
                      : il2cpp::GcHandle{};
    if (!fleet || binding::Read<bool>(fleet.Get(), a.local_fleet))
      continue; // The player's own miners retain native fleet controls.
    const auto id = planned_warp_binding::Scalar<"get_ID", int64_t>(fleet.Get<FleetDeployedData>());
    if (!id || id.value == 0 || id.value == -1)
      continue;
    MiningReset();
    auto& session   = MiningSession();
    session.context = il2cpp::GcHandle::Weak(observed.context.Get());
    if (!session.context)
      return false;
    session.node            = observed.id;
    session.type            = observed.type;
    session.fleet           = id.value;
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

// --- Selected ship OPC outline ---

struct MiningOpcOutline {
  il2cpp::GcHandle  card, anchor, object, context, loader, poi;
  int64_t           fleet = 0;
  Clock::time_point next{};
};

MiningOpcOutline& MiningOutline()
{
  // The native card owns its overlay. Retain only weak handles between frames.
  static auto* outline = new MiningOpcOutline;
  return *outline;
}

void HideMiningOutline()
{
  auto& outline = MiningOutline();
  if (outline.fleet == 0)
    return;
  outline.context.Reset();
  outline.loader.Reset();
  outline.poi.Reset();
  outline.fleet = 0;
  auto object   = il2cpp::GcHandle::Strong(outline.object.Get());
  if (unity::Alive(object.Get()) && !object.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(false)) {
    unity::Destroy(object.Get());
    outline.object.Reset();
  }
}

bool SetMiningOutlineRect(unity::RectTransform* rect, unity::Vector2 min, unity::Vector2 max, unity::Vector2 offset_min,
                          unity::Vector2 offset_max)
{
  return rect->TryCall<"set_anchorMin", void(unity::Vector2)>(min)
         && rect->TryCall<"set_anchorMax", void(unity::Vector2)>(max)
         && rect->TryCall<"set_offsetMin", void(unity::Vector2)>(offset_min)
         && rect->TryCall<"set_offsetMax", void(unity::Vector2)>(offset_max);
}

il2cpp::GcHandle CreateMiningOutline(unity::RectTransform* anchor)
{
  auto       parent = il2cpp::GcHandle::Strong(anchor->gameObject());
  const auto layer =
      parent ? parent.Get<unity::GameObject>()->TryCall<"get_layer", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!layer)
    return {};
  il2cpp::GcHandle root;
  auto*            rect      = unity::CreateUiObject("CommunityMod_MiningTargetOpc", anchor, root);
  auto*            container = root.Get<unity::GameObject>();
  auto layout = il2cpp::GcHandle::Strong(container ? container->AddComponent<unity::LayoutElement>() : nullptr);
  if (!rect || !layout || !container->TryCall<"set_layer", void(int32_t)>(layer.value)
      || !layout.Get<unity::LayoutElement>()->TryCall<"set_ignoreLayout", void(bool)>(true)
      || !SetMiningOutlineRect(rect, {0, 0}, {1, 1}, {0, 0}, {0, 0})) {
    if (container)
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
    if (!line || !image || !object->TryCall<"set_layer", void(int32_t)>(layer.value)
        || !SetMiningOutlineRect(line, edge.min, edge.max, edge.offset_min, edge.offset_max)
        || !image.Get<unity::Image>()->TryCall<"set_raycastTarget", void(bool)>(false)
        || !image.Get<unity::Image>()->TryCall<"set_color", void(unity::Color)>(unity::Color{1.0f, 0.66f, 0.12f, 1.0f})
        || !object->TryCall<"SetActive", void(bool)>(true)) {
      unity::Destroy(container);
      return {};
    }
  }
  return root;
}

void ShowMiningOutline(const binding::Viewer& selected)
{
  auto& outline = MiningOutline();
  auto* card    = selected.source.Get<PreScanTargetWidget>();
  auto  anchor  = binding::CardAnchor(card);
  if (anchor && outline.anchor.Get() == anchor.Get() && outline.context.Get() == selected.context.Get()
      && outline.card.Get() == selected.source.Get() && outline.loader.Get() == selected.loader.Get()
      && outline.poi.Get() == selected.poi.Get() && outline.fleet == selected.id && unity::Alive(outline.object.Get()))
    return;
  HideMiningOutline();
  if (!anchor)
    return;
  auto object = il2cpp::GcHandle::Strong(outline.object.Get());
  if (!unity::Alive(object.Get()) || outline.anchor.Get() != anchor.Get()) {
    if (unity::Alive(object.Get()))
      unity::Destroy(object.Get());
    object = CreateMiningOutline(anchor.Get<unity::RectTransform>());
  }
  if (!object)
    return;

  // Native object creation and property calls can reenter card binding. Recheck before showing.
  auto card_ref    = il2cpp::GcHandle::Weak(card);
  auto anchor_ref  = il2cpp::GcHandle::Weak(anchor.Get());
  auto object_ref  = il2cpp::GcHandle::Weak(object.Get());
  auto context_ref = il2cpp::GcHandle::Weak(selected.context.Get());
  auto loader_ref  = il2cpp::GcHandle::Weak(selected.loader.Get());
  auto poi_ref     = il2cpp::GcHandle::Weak(selected.poi.Get());
  if (!card_ref || !anchor_ref || !object_ref || !context_ref || !loader_ref || !poi_ref
      || !binding::StillSelected(card, selected.context.Get<NavigationInteractionUIContext>(),
                                 selected.loader.Get<binding::Loader>(), selected.poi.Get<binding::Poi>())
      || !object.Get<unity::GameObject>()->TryCall<"SetActive", void(bool)>(true)) {
    unity::Destroy(object.Get());
    return;
  }
  outline.card    = std::move(card_ref);
  outline.anchor  = std::move(anchor_ref);
  outline.object  = std::move(object_ref);
  outline.context = std::move(context_ref);
  outline.loader  = std::move(loader_ref);
  outline.poi     = std::move(poi_ref);
  outline.fleet   = selected.id;
}

void MiningTargetOpcFrame()
{
  auto& outline = MiningOutline();
  if (!Config::Get().ui.highlight_opc_fleets || !MiningSystemMap()) {
    HideMiningOutline();
    outline.next = {};
    return;
  }

  if (outline.fleet != 0) {
    auto card    = il2cpp::GcHandle::Strong(outline.card.Get());
    auto context = il2cpp::GcHandle::Strong(outline.context.Get());
    auto loader  = il2cpp::GcHandle::Strong(outline.loader.Get());
    auto poi     = il2cpp::GcHandle::Strong(outline.poi.Get());
    if (!unity::Alive(card.Get())
        || !binding::StillSelected(card.Get<PreScanTargetWidget>(), context.Get<NavigationInteractionUIContext>(),
                                   loader.Get<binding::Loader>(), poi.Get<binding::Poi>()))
      HideMiningOutline();
  }

  const auto now = Clock::now();
  if (now < outline.next)
    return;
  outline.next = now + std::chrono::milliseconds(250);
  for (auto* card : unity::Instances<PreScanTargetWidget>()) {
    auto selected = binding::Observe(card);
    if (!selected)
      continue;
    auto       fleet  = binding::Fleet(selected.source.Get<PreScanTargetWidget>());
    auto*      target = fleet.Get<FleetDeployedData>();
    const auto id     = target ? target->TryId() : il2cpp::CallResult<int64_t>{};
    if (id && id.value == selected.id && binding::TargetIsOpc(target, id.value)) {
      ShowMiningOutline(selected);
      return;
    }
  }
  HideMiningOutline();
}

bool InstallMiningTargetOpc()
{
  if (binding::TargetOpcAvailable())
    return true;
  spdlog::warn("[MiningTargetOpc] native mining target cargo contract unavailable");
  return false;
}

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

const Feature kMiningTargetOpc{"MiningTargetOpc", &Config::Patches::opcindicatorhooks, InstallMiningTargetOpc,
                               MiningTargetOpcFrame};
