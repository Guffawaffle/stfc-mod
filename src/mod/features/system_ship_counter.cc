// [ui].system_player_ship_counter: client-known player ships in the viewed system.
// Native receipt/removal boundaries maintain copied state; camera culling does not.
#include "game/system_ship_counter.h"
#include "config.h"
#include "features/features.h"
#include "features/system_ship_counter_hud.h"
#include "features/system_ship_counter_state.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
namespace ship_binding = system_ship_counter_binding;
using ShipCounterClock = std::chrono::steady_clock;
struct SystemShipCounterState {
  system_ship_counter::Counter               model;
  system_ship_counter_hud::Hud               hud;
  il2cpp::GcHandle                           service;
  std::thread::id                            thread;
  std::atomic<bool>                          wrong_thread{false};
  ShipCounterClock::time_point               next_frame{}, next_seed{}, next_log{};
  std::optional<system_ship_counter::Totals> last_log;
  std::unordered_set<int64_t>                changed;
  bool installed = false, needs_seed = true, read_warned = false, thread_warned = false;
};
SystemShipCounterState& ShipCounterState()
{
  static auto* state = new SystemShipCounterState;
  return *state;
}
bool ShipCounterEnabled()
{ return Config::Get().ui.system_player_ship_counter; }
bool ShipCounterThread()
{
  auto& state = ShipCounterState();
  if (!state.installed)
    return false;
  if (state.thread == std::this_thread::get_id())
    return true;
  state.wrong_thread.store(true, std::memory_order_relaxed);
  return false;
}
void FollowShipCounterService(ship_binding::Service* service)
{
  auto& state = ShipCounterState();
  if (state.service.Get() == service)
    return;
  state.service = il2cpp::GcHandle::Weak(service);
  state.model.Reset();
  state.changed.clear();
  state.needs_seed = true;
  state.next_seed  = {};
  state.last_log.reset();
}
void ShipCounterUnavailable()
{
  auto& state = ShipCounterState();
  state.model.Unavailable();
  state.needs_seed = true;
  if (!std::exchange(state.read_warned, true))
    spdlog::warn("[SystemShipCounter] A fleet observation was unavailable; deferring the displayed count.");
}
std::optional<system_ship_counter::Observation> ObserveCounterFleet(FleetDeployedData* fleet)
{
  if (!fleet)
    return std::nullopt;
  const auto type = fleet->TryFleetType();
  const auto id   = fleet->TryId();
  if (!type || !id || id.value <= 0)
    return std::nullopt;
  if (type.value != DeployedFleetType::Player) {
    system_ship_counter::Observation other;
    other.id   = id.value;
    other.type = static_cast<int32_t>(type.value);
    return other;
  }
  return ship_binding::ReadFleet<system_ship_counter::Observation>(fleet);
}
void ApplyObservedCounterFleet(FleetDeployedData* fleet)
{
  const auto observation = ObserveCounterFleet(fleet);
  if (!observation) {
    ShipCounterUnavailable();
    return;
  }
  ShipCounterState().model.Apply(*observation);
}
void ApplyCurrentCounterFleet(ship_binding::Service* service, int64_t id)
{
  const auto fleet = ship_binding::TryFleet(service, id);
  auto       root  = il2cpp::GcHandle::Strong(fleet ? fleet.value : nullptr);
  if (!fleet) {
    ShipCounterUnavailable();
    return;
  }
  if (!fleet.value) {
    ShipCounterState().model.Remove(id);
    return;
  }
  if (!root) {
    ShipCounterUnavailable();
    return;
  }
  ApplyObservedCounterFleet(root.Get<FleetDeployedData>());
}
void RecordCounterRemoval(ship_binding::Service* service, int64_t id)
{
  const auto fleet = ship_binding::TryFleet(service, id);
  auto       root  = il2cpp::GcHandle::Strong(fleet ? fleet.value : nullptr);
  if (root) {
    const auto type = root.Get<FleetDeployedData>()->TryFleetType();
    if (type && type.value == DeployedFleetType::Player)
      ShipCounterState().model.Remove(id);
    else if (!type)
      ShipCounterUnavailable();
  } else if (!fleet || fleet.value)
    ShipCounterUnavailable();
}
void DeploymentService_AddDeployedFleet_CounterHook(auto original, ship_binding::Service* _this, ship_binding::Set* set,
                                                    int64_t id, ship_binding::Model* received)
{
  const bool tracking = ShipCounterThread();
  bool       accepted = false;
  if (tracking) {
    FollowShipCounterService(_this);
    // Add can ignore an untrackable receipt while leaving an old queued contact
    // cached. Only the native acceptance rule can release its removal marker.
    const auto trackable = ship_binding::ShouldTrack(_this, set, received);
    if (!trackable)
      ShipCounterUnavailable();
    else if ((accepted = trackable.value))
      ShipCounterState().model.Receipt(id);
    else
      RecordCounterRemoval(_this, id);
  }
  original(_this, set, id, received);
  if (tracking && accepted && ShipCounterEnabled())
    ApplyCurrentCounterFleet(_this, id);
}
void DeploymentService_UpdateDeployedFleet_CounterHook(auto original, ship_binding::Service* _this,
                                                       ship_binding::Set* set, int64_t id,
                                                       ship_binding::Model* received)
{
  const bool tracking = ShipCounterThread();
  if (tracking) {
    FollowShipCounterService(_this);
    // Release before the receipt; any nested native removal must win.
    ShipCounterState().model.Receipt(id);
  }
  original(_this, set, id, received);
  if (tracking && ShipCounterEnabled())
    ApplyCurrentCounterFleet(_this, id);
}
void DeploymentService_RemoveDeployedFleet_CounterHook(auto original, ship_binding::Service* _this,
                                                       ship_binding::Set* set, int64_t id, FleetRemovalReason reason,
                                                       bool clear)
{
  if (ShipCounterThread()) {
    FollowShipCounterService(_this);
    RecordCounterRemoval(_this, id);
  }
  original(_this, set, id, reason, clear);
}
void DeploymentService_ActiveSystem_CounterHook(auto original, ship_binding::Service* _this, int64_t system)
{
  original(_this, system);
  if (ShipCounterThread()) {
    FollowShipCounterService(_this);
    ShipCounterState().needs_seed = true;
    ShipCounterState().next_seed  = {};
  }
}
void DeploymentService_Reset_CounterHook(auto original, ship_binding::Service* _this, ship_binding::ServerContext* ctx)
{
  original(_this, ctx);
  if (ShipCounterThread()) {
    FollowShipCounterService(_this);
    ShipCounterState().model.Reset();
    ShipCounterState().changed.clear();
    ShipCounterState().needs_seed = true;
    ShipCounterState().next_seed  = {};
  }
}
void QueueCounterBatch(ship_binding::FleetList* fleets)
{
  if (!ShipCounterThread() || !ShipCounterEnabled())
    return;
  // Course ticks can move a ship to galaxy scope before its next server
  // receipt. Coalesce changed player IDs until the next display refresh;
  // neither a repeated movement event nor a render tick releases a marker.
  const bool read = ship_binding::VisitBatch(fleets, [](FleetDeployedData* fleet) {
    const auto type = fleet->TryFleetType();
    if (!type)
      return false;
    if (type.value != DeployedFleetType::Player)
      return true;
    const auto id = fleet->TryId();
    if (!id)
      return false;
    ShipCounterState().changed.insert(id.value);
    return true;
  });
  if (!read)
    ShipCounterUnavailable();
}
void DeploymentEvents_Updated_CounterHook(auto original, ship_binding::FleetList* fleets)
{
  QueueCounterBatch(fleets);
  original(fleets);
}
void DeploymentEvents_State_CounterHook(auto original, ship_binding::FleetList* fleets)
{
  QueueCounterBatch(fleets);
  original(fleets);
}
void DeploymentEvents_Exit_CounterHook(auto original, NodeAddress* address, ship_binding::FleetList* fleets)
{
  QueueCounterBatch(fleets);
  original(address, fleets);
}
void SystemShipCounterFrame()
{
  if (!ShipCounterThread())
    return;
  auto&      state = ShipCounterState();
  const auto now   = ShipCounterClock::now();
  if (now < state.next_frame)
    return;
  state.next_frame = now + std::chrono::milliseconds(250);
  if (!ShipCounterEnabled()) {
    state.hud.Clear();
    // Preserve removal intent while the display is disabled. Re-enabling must
    // not resurrect a departed ship retained by the native action queue.
    state.model.Unavailable();
    state.changed.clear();
    state.needs_seed = true;
    return;
  }
  const auto scope = state.hud.Poll();
  if (!scope)
    return;
  auto  owner   = il2cpp::GcHandle::Strong(state.service.Get());
  auto* service = owner.Get<ship_binding::Service>();
  if (!service || ship_binding::ActiveSystem(service) != scope->system) {
    state.hud.Show("Known player ships: ...");
    return;
  }
  if (state.wrong_thread.load(std::memory_order_relaxed)) {
    if (!std::exchange(state.thread_warned, true))
      spdlog::warn("[SystemShipCounter] Fleet callbacks crossed threads; suspending the scratch counter.");
    state.hud.Show("Known player ships: --");
    return;
  }
  if (state.model.Focus(*scope)) {
    state.needs_seed = true;
    state.next_seed  = {};
    state.last_log.reset();
  }
  if (state.needs_seed && now >= state.next_seed) {
    state.next_seed                                        = now + std::chrono::seconds(5);
    const auto                                    snapshot = ship_binding::Snapshot(service);
    std::vector<system_ship_counter::Observation> observations;
    bool                                          complete = snapshot.has_value();
    if (snapshot) {
      observations.reserve(snapshot->size());
      for (const auto& fleet : *snapshot) {
        const auto observation = ObserveCounterFleet(fleet.Get<FleetDeployedData>());
        if (!observation) {
          complete = false;
          break;
        }
        observations.push_back(*observation);
      }
    }
    if (complete) {
      state.model.Seed(observations);
      state.needs_seed  = false;
      state.read_warned = false;
    } else
      ShipCounterUnavailable();
  }
  // Always read the selected service's current object, never an old event
  // reference. Multiple event batches for one ship cost one refresh here.
  const auto pending = std::exchange(state.changed, {});
  for (int64_t id : pending) {
    const auto current = ship_binding::TryFleet(service, id);
    auto       root    = il2cpp::GcHandle::Strong(current ? current.value : nullptr);
    if (!current)
      ShipCounterUnavailable();
    else if (!current.value)
      state.model.Remove(id);
    else if (!root)
      ShipCounterUnavailable();
    else
      ApplyObservedCounterFleet(root.Get<FleetDeployedData>()); // Apply only; no Receipt.
  }
  const auto totals = state.model.Count();
  if (!totals) {
    state.hud.Show("Known player ships: ...");
    return;
  }
  state.hud.Show(fmt::format("Known player ships: {}", totals->ships));
  if (now >= state.next_log && state.last_log != totals) {
    state.next_log = now + std::chrono::seconds(2);
    state.last_log = totals;
    spdlog::debug("[SystemShipCounter] system={} instance={} known={} own={} observed_cloaked={} "
                  "observed_undetected_cloaked={} cloak_unknown={}",
                  scope->system, scope->instance, totals->ships, totals->own, totals->cloaked,
                  totals->undetected_cloaked, totals->cloak_unknown);
  }
}
bool InstallSystemShipCounter()
{
  il2cpp::HookSet hooks("SystemShipCounter");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::AddMethod()),
                  DeploymentService_AddDeployedFleet_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::UpdateMethod()),
                  DeploymentService_UpdateDeployedFleet_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::RemoveMethod()),
                  DeploymentService_RemoveDeployedFleet_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::ActiveSystemChangeMethod()),
                  DeploymentService_ActiveSystem_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::ResetMethod()), DeploymentService_Reset_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::UpdatedEventMethod()),
                  DeploymentEvents_Updated_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::StateEventMethod()), DeploymentEvents_State_CounterHook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ship_binding::ExitEventMethod()), DeploymentEvents_Exit_CounterHook);
  if (!hooks.Commit())
    return false;
  auto& state     = ShipCounterState();
  state.thread    = std::this_thread::get_id();
  state.installed = true;
  return true;
}
} // namespace

const Feature kSystemShipCounter{"SystemShipCounter", &Config::Patches::systemshipcounterhooks,
                                 InstallSystemShipCounter, SystemShipCounterFrame};
