/**
 * @file fleet_runtime_sync.cc
 * @brief Change-driven fleet runtime sync snapshots.
 */
#include "patches/fleet_runtime_sync.h"

#include "config.h"
#include "patches/fleet_runtime_diagnostics.h"
#include "patches/live_debug_fleet_runtime_observers.h"
#include "patches/sidecar_local_ingest.h"
#include "patches/sync_scheduler.h"

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>

namespace
{
using json = nlohmann::json;

constexpr auto kFleetRuntimeSyncQuietDelay = std::chrono::milliseconds(2500);

std::mutex                            s_pending_sync_mutex;
bool                                  s_pending_sync = false;
GameplayDispatchContext               s_pending_sync_dispatch;
std::chrono::steady_clock::time_point s_pending_sync_requested_at;
uint64_t                              s_pending_sync_sequence = 0;
bool                                  s_pending_sync_delay_logged = false;

struct FleetSlotStateKey {
  bool        selected = false;
  bool        present = false;
  uint64_t    fleet_id = 0;
  int64_t     hull_spec_id = -1;
  int         current_state = -1;
  int         previous_state = -1;
  int         cargo_fill_percent = -1;
  std::string hull_name;
  std::optional<std::string> ship_identity_probe_id;

  auto operator<=>(const FleetSlotStateKey&) const = default;
};

struct FleetStateKey {
  bool tracked = false;
  int  selected_index = -1;

  bool        fleet_present = false;
  uint64_t    fleet_id = 0;
  int64_t     hull_spec_id = -1;
  int         current_state = -1;
  int         previous_state = -1;
  int         cargo_fill_percent = -1;
  std::string hull_name;
  std::optional<std::string> ship_identity_probe_id;

  std::array<FleetSlotStateKey, kFleetIndexMax> slots{};

  auto operator<=>(const FleetStateKey&) const = default;
};

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool fleet_runtime_sync_enabled()
{
  return Config::Get().installSyncPatches
         && (Config::Get().sync_options.fleet_runtime || sidecar_local_ingest::FleetRuntimeEnabled());
}

int cargo_fill_percent_bucket(int basis_points, int percent)
{
  if (percent >= 0) {
    return percent;
  }
  if (basis_points >= 0) {
    return basis_points / 100;
  }
  return -1;
}

FleetStateKey make_state_key(const FleetObservation& fleet,
                             const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  FleetStateKey key;
  key.tracked = fleet.tracked;
  key.selected_index = fleet.selectedIndex;
  key.fleet_present = fleet.hasFleet;
  key.fleet_id = fleet.fleetId;
  key.hull_spec_id = fleet.hullSpecId;
  key.current_state = fleet.currentState;
  key.previous_state = fleet.previousState;
  key.cargo_fill_percent = cargo_fill_percent_bucket(fleet.cargoFillBasisPoints, fleet.cargoFillPercent);
  key.hull_name = fleet.hullName;
  key.ship_identity_probe_id = fleet.shipIdentityProbeId;

  for (size_t index = 0; index < slots.size(); ++index) {
    const auto& slot = slots[index];
    auto&       state = key.slots[index];
    state.selected = slot.selected;
    state.present = slot.present;
    state.fleet_id = slot.fleetId;
    state.hull_spec_id = slot.hullSpecId;
    state.current_state = slot.currentState;
    state.previous_state = slot.previousState;
    state.cargo_fill_percent = cargo_fill_percent_bucket(slot.cargoFillBasisPoints, slot.cargoFillPercent);
    state.hull_name = slot.hullName;
    state.ship_identity_probe_id = slot.shipIdentityProbeId;
  }

  return key;
}

bool is_meaningful_state(const FleetStateKey& state)
{
  return state.tracked || state.fleet_present
      || std::ranges::any_of(state.slots, [](const auto& slot) { return slot.present; });
}

json fleet_to_json(const FleetObservation& fleet)
{
  json result = {{"present", fleet.hasFleet}};

  if (!fleet.hasFleet) {
    return result;
  }

  result["id"] = fleet.fleetId;
  result["currentState"] = fleet.currentState;
  result["currentStateName"] = fleet_state_name_from_value(fleet.currentState);
  result["previousState"] = fleet.previousState;
  result["previousStateName"] = fleet_state_name_from_value(fleet.previousState);
  result["cargoFillPercent"] = fleet.cargoFillPercent;
  result["cargoFillBasisPoints"] = fleet.cargoFillBasisPoints;
  if (fleet.hullSpecId >= 0) {
    result["hullSpecId"] = fleet.hullSpecId;
  }
  result["hullName"] = fleet.hullName;
  if (fleet.shipIdentityProbeId.has_value()) {
    result["shipIdentityProbe"] = {
        {"shipId", *fleet.shipIdentityProbeId},
        {"source", "FleetPlayerData.Ship.ID"},
    };
  }
  return result;
}

json fleet_slot_to_json(const FleetSlotObservation& slot)
{
  json result = {
      {"slotIndex", slot.slotIndex},
      {"selected", slot.selected},
      {"present", slot.present},
  };

  if (!slot.present) {
    return result;
  }

  result["fleetId"] = slot.fleetId;
  result["currentState"] = slot.currentState;
  result["currentStateName"] = fleet_state_name_from_value(slot.currentState);
  result["previousState"] = slot.previousState;
  result["previousStateName"] = fleet_state_name_from_value(slot.previousState);
  result["cargoFillPercent"] = slot.cargoFillPercent;
  result["cargoFillBasisPoints"] = slot.cargoFillBasisPoints;
  if (slot.hullSpecId >= 0) {
    result["hullSpecId"] = slot.hullSpecId;
  }
  result["hullName"] = slot.hullName;
  if (slot.shipIdentityProbeId.has_value()) {
    result["shipIdentityProbe"] = {
        {"shipId", *slot.shipIdentityProbeId},
        {"source", "FleetPlayerData.Ship.ID"},
    };
  }
  return result;
}

json slots_to_json(const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  auto result = json::array();
  for (const auto& slot : slots) {
    result.push_back(fleet_slot_to_json(slot));
  }
  return result;
}

json build_snapshot_payload(std::string_view source, int64_t observed_at_ms, const FleetObservation& fleet,
                            const std::array<FleetSlotObservation, kFleetIndexMax>& slots)
{
  return json{
      {"type", "fleet.runtime"},
      {"schemaVersion", "stfc.fleet.runtime_snapshot.v1"},
      {"source", source},
      {"observedAtMs", observed_at_ms},
      {"fleetBarTracked", fleet.tracked},
      {"selectedIndex", fleet.selectedIndex},
      {"fleet", fleet_to_json(fleet)},
      {"slots", slots_to_json(slots)},
  };
}
} // namespace

void fleet_runtime_sync_trigger(const GameplayDispatchContext& dispatch)
{
  if (!fleet_runtime_sync_enabled()) {
    return;
  }

  std::lock_guard lock(s_pending_sync_mutex);
  s_pending_sync          = true;
  s_pending_sync_dispatch = dispatch;
  s_pending_sync_requested_at = std::chrono::steady_clock::now();
  ++s_pending_sync_sequence;
  s_pending_sync_delay_logged = false;
  spdlog::debug("[FleetRuntimeSync] stage=request source={} owner={} seam={} reason={} effect={} decision=deferred "
                "quietDelayMs={} seq={}",
                dispatch.source,
                dispatch.owner,
                dispatch.seam,
                dispatch.reason,
                dispatch.effect,
                kFleetRuntimeSyncQuietDelay.count(),
                s_pending_sync_sequence);
}

bool fleet_runtime_sync_frame_subscriber_enabled()
{ return fleet_runtime_sync_enabled(); }

void fleet_runtime_sync_process_pending()
{
  GameplayDispatchContext dispatch;
  uint64_t                sequence = 0;
  {
    std::lock_guard lock(s_pending_sync_mutex);
    if (!s_pending_sync) {
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_pending_sync_requested_at);
    if (age < kFleetRuntimeSyncQuietDelay) {
      if (!s_pending_sync_delay_logged) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            kFleetRuntimeSyncQuietDelay - age);
        spdlog::debug("[FleetRuntimeSync] stage=flush source={} owner={} seam={} reason={} effect={} "
                      "decision=deferred deferReason=quiet-window ageMs={} remainingMs={} seq={}",
                      s_pending_sync_dispatch.source,
                      s_pending_sync_dispatch.owner,
                      s_pending_sync_dispatch.seam,
                      s_pending_sync_dispatch.reason,
                      s_pending_sync_dispatch.effect,
                      age.count(),
                      remaining.count(),
                      s_pending_sync_sequence);
        s_pending_sync_delay_logged = true;
      }
      return;
    }

    dispatch = s_pending_sync_dispatch;
    sequence = s_pending_sync_sequence;
    s_pending_sync = false;
    s_pending_sync_dispatch = {};
    s_pending_sync_delay_logged = false;
  }

  if (!fleet_runtime_sync_enabled()) {
    spdlog::warn("[FleetRuntimeSync] stage=flush source={} owner={} seam={} reason={} effect={} decision=skipped "
                 "skipReason=disabled",
                 dispatch.source,
                 dispatch.owner,
                 dispatch.seam,
                 dispatch.reason,
                 dispatch.effect);
    return;
  }

  spdlog::info("[FleetRuntimeSync] stage=flush source={} owner={} seam={} reason={} effect={} decision=executed "
               "flushReason=quiet-window seq={}",
               dispatch.source,
               dispatch.owner,
               dispatch.seam,
               dispatch.reason,
               dispatch.effect,
               sequence);
  fleet_runtime_diagnostics_trigger(dispatch);

  if (sidecar_local_ingest::FleetRuntimeRequestOnlyMode()) {
    spdlog::info("[FleetRuntimeSync] stage=capture source={} owner={} seam={} reason={} effect={} decision=skipped "
                 "skipReason=request-only mode={} classification=quarantined-diagnostic-mode",
                 dispatch.source,
                 dispatch.owner,
                 dispatch.seam,
                 dispatch.reason,
                 dispatch.effect,
                 sidecar_local_ingest::FleetRuntimeMode());
    return;
  }

  try {
    fleet_runtime_sync_capture(dispatch);
  } catch (const std::exception& ex) {
    spdlog::error("[FleetRuntimeSync] source={} owner={} seam={} reason={} effect={} status=failed error='{}'",
                  dispatch.source,
                  dispatch.owner,
                  dispatch.seam,
                  dispatch.reason,
                  dispatch.effect,
                  ex.what());
  } catch (...) {
    spdlog::error("[FleetRuntimeSync] source={} owner={} seam={} reason={} effect={} status=failed error='unknown "
                  "exception'",
                  dispatch.source,
                  dispatch.owner,
                  dispatch.seam,
                  dispatch.reason,
                  dispatch.effect);
  }
}

void fleet_runtime_sync_capture(const GameplayDispatchContext& dispatch)
{
  static std::optional<FleetStateKey> last_state;

  const auto capture_started_at = std::chrono::steady_clock::now();
  const auto snapshot = observe_fleet_runtime_snapshot();
  const auto state = make_state_key(snapshot.fleet, snapshot.slots);
  const auto observed_at_ms = current_time_millis_utc();
  const auto capture_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - capture_started_at)
                                       .count();

  fleet_runtime_diagnostics_capture_attempt(dispatch, capture_duration_ms);

  if (!is_meaningful_state(state) && !last_state.has_value()) {
    fleet_runtime_diagnostics_suppressed_non_meaningful(dispatch, capture_duration_ms);
    return;
  }

  if (last_state.has_value() && *last_state == state) {
    fleet_runtime_diagnostics_suppressed_unchanged(dispatch, capture_duration_ms);
    return;
  }

  last_state = state;
  const auto trace = fleet_runtime_diagnostics_make_trace(dispatch, snapshot.fleet, snapshot.slots, observed_at_ms,
                                                          capture_duration_ms);
  const auto payload = build_snapshot_payload(dispatch.source, observed_at_ms, snapshot.fleet, snapshot.slots);
  if (Config::Get().sync_options.fleet_runtime) {
    queue_data(SyncConfig::Type::FleetRuntime, payload, false, trace);
  }
  if (sidecar_local_ingest::FleetRuntimeEnabled()) {
    if (sidecar_local_ingest::FleetRuntimeSnapshotOnlyMode()) {
      spdlog::info("[FleetRuntimeSync] stage=sidecar-publish source={} owner={} seam={} reason={} effect={} "
                   "decision=skipped skipReason=snapshot-only captureDurationMs={} mode={} "
                   "classification=quarantined-diagnostic-mode",
                   dispatch.source,
                   dispatch.owner,
                   dispatch.seam,
                   dispatch.reason,
                   dispatch.effect,
                   capture_duration_ms,
                   sidecar_local_ingest::FleetRuntimeMode());
      return;
    }

    const auto sidecar_context = sidecar_local_dispatch_context(
        dispatch,
        "stfc.fleet.runtime_snapshot.v1",
        "runtime-evidence",
        "sidecar-local.fleet-runtime",
        "game thread copied fleet runtime snapshot before async sidecar publish");
    const auto enqueue_started_at = std::chrono::steady_clock::now();
    const auto enqueue = sidecar_local_ingest::EnqueueFleetRuntimeSnapshot(payload, sidecar_context);
    const auto enqueue_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::steady_clock::now() - enqueue_started_at)
                                         .count();
    spdlog::info("[FleetRuntimeSync] stage=sidecar-enqueue source={} owner={} seam={} reason={} effect={} "
                 "accepted={} coalesced={} depth={} enqueued={} dropped={} coalescedTotal={} durationUs={} mode={}",
                 dispatch.source,
                 dispatch.owner,
                 dispatch.seam,
                 dispatch.reason,
                 dispatch.effect,
                 enqueue.accepted,
                 enqueue.coalesced,
                 enqueue.depth,
                 enqueue.enqueued,
                 enqueue.dropped,
                 enqueue.coalesced_total,
                 enqueue_duration_us,
                 sidecar_local_ingest::FleetRuntimeMode());
  }
}
