// ActionView cycles an occupied mining node through its foreign occupant and
// cargo, then returns to the same node. It owns no independent shortcut path.
#include "features/mining_preview.h"
#include "config.h"
#include "features/features.h"
#include "features/mining_preview_poll.h"

#include "game/mining_preview.h"

#include <chrono>
#include <optional>
#include <spdlog/spdlog.h>
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
    if (!observed.source)
      continue;
    auto visibility =
        il2cpp::GcHandle::Strong(binding::Read<VisibilityController*>(observed.source.Get(), a.visibility));
    if (!visibility)
      continue;
    const auto state = binding::Read<int32_t>(visibility.Get(), a.visibility_state);
    if (state != std::to_underlying(VisibilityState::Visible) && state != std::to_underlying(VisibilityState::Show))
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
    session.cargo_first     = Config::Get().ui.show_cargo_default && Config::Get().ui.show_player_cargo;
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

bool mining_preview::SuppressAutomaticCargo(RewardsButtonWidget* widget)
{
  auto& session = MiningSession();
  if (!session.context || !session.context.Get() || !MiningSystemMap() || session.cargo_requested || !widget)
    return false;
  const auto  context = widget->TryCall<"get_Context", BattleTargetData*>();
  auto        data    = context ? il2cpp::GcHandle::Strong(context.value) : il2cpp::GcHandle{};
  const auto& a       = binding::Native();
  auto        fleet   = data ? il2cpp::GcHandle::Strong(binding::Read<FleetDeployedData*>(data.Get(), a.target_fleet))
                             : il2cpp::GcHandle{};
  const auto  id      = fleet ? planned_warp_binding::Scalar<"get_ID", int64_t>(fleet.Get<FleetDeployedData>())
                              : il2cpp::CallResult<int64_t>{};
  return id && id.value == session.fleet;
}

const Feature kMiningPreview{"MiningPreview", &Config::Patches::hotkeyhooks, InstallMiningPreview, MiningPreviewFrame};
