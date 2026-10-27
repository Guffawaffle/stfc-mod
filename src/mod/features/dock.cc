// The ship management dock:
// - saved ship-instance pins lead in their acknowledged order; ui.pinned_ships remains a legacy fallback;
//   double-clicking a pin badge toggles it, and a held badge drag changes the saved order;
// - [ui] double_click_to_assign_ship presses Assign on a double click on a ship tile, or on Enter;
// - [ui] dock_syncmanagement selects the fleet bar slot of the ship open in ship management.
//
// Only ShipManagementViewContext.Sort is hooked, not ISortedFleetsContext, so the hailing frequencies inventory keeps
// the game's order.

#include "checked_ship_order.h"
#include "config.h"
#include "features/dock_assignment_input.h"
#include "features/features.h"
#include "features/ship_pin_input.h"
#include "frame.h"
#include "mod_state.h"
#include "pending_pin_order.h"
#include "pinned_ship_order.h"
#include "pinned_ship_state.h"
#include "text.h"

#include "game/app.h"
#include "game/fleet.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>
#include <unity/tracker.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
// The ship management screen when it is the one showing. The dock is hosted under more than one game section, so
// this follows the screen rather than SectionID.
ShipManagementViewController* VisibleShipManagement()
{
  for (auto* controller : unity::Instances<ShipManagementViewController>())
    if (CanvasController::OnScreen(controller))
      return controller;
  return nullptr;
}

std::string ShipNameText(const std::vector<std::string>& words)
{ return fmt::format("{}", fmt::join(words, " ")); }

// --- Stable ship-instance pins ---

struct PinStateData {
  std::vector<int64_t>               ids, legacy_ids;
  std::vector<std::string>           legacy_names;
  pinned_ship_state::PendingPinOrder pending;
  il2cpp::GcHandle                   save_context, refresh_context;
  int                                refresh_frames = 0;
  bool                               has_id_order = false, loaded = false, worker_ready = false;
  bool                               sort_installed = false, input_installed = false, receipt_registered = false;
  bool                               receipt_healthy = true;
  bool                               writes_safe = true, snapshot_warned = false;
};
PinStateData& PinState()
{
  // Explicit process lifetime: no native roots are released after IL2CPP teardown.
  static auto* state = new PinStateData;
  return *state;
}
std::vector<std::string> PinConfiguredLegacyNames()
{
  std::vector<std::string> names;
  for (const auto& words : Config::Get().ui.pinned_ships)
    names.push_back(ShipNameText(words));
  return names;
}
std::vector<std::string> PinAppliedLegacyNames()
{ return PinState().has_id_order ? PinState().legacy_names : PinConfiguredLegacyNames(); }
const std::vector<int64_t>& PinDesiredIds()
{ return PinState().pending.Desired() ? PinState().pending.Desired()->ids : PinState().ids; }
std::vector<std::string> PinDesiredLegacyNames()
{ return PinState().pending.Desired() ? PinState().pending.Desired()->legacy_names : PinAppliedLegacyNames(); }

struct PinRow {
  FleetPlayerData*         ship = nullptr;
  std::optional<int64_t>   id;
  std::vector<std::string> words;
  int64_t                  level    = 0;
  bool                     non_ship = true;
};
using PinList = il2cpp::List<FleetPlayerData*>;
struct PinSnapshot {
  il2cpp::GcHandle              context, list;
  std::vector<il2cpp::GcHandle> rows_owned;
  std::vector<PinRow>           rows;
};
std::optional<PinSnapshot> PinReadSnapshot(ShipManagementViewContext* context, bool names)
{
  if (!context)
    return std::nullopt;
  PinSnapshot result;
  result.context = il2cpp::GcHandle::Strong(context);
  auto* list     = result.context ? context->SortedIdleShips() : nullptr;
  result.list    = il2cpp::GcHandle::Strong(list);
  if (!result.list || !list->HasMethod<int32_t()>("get_Count")
      || !list->HasMethod<FleetPlayerData*(int32_t)>("get_Item")
      || !list->HasMethod<void(int32_t, FleetPlayerData*)>("set_Item"))
    return std::nullopt;
  const auto count = list->TryCall<"get_Count", int32_t>();
  if (!count || count.value < 0)
    return std::nullopt;
  result.rows.reserve(static_cast<size_t>(count.value));
  result.rows_owned.reserve(static_cast<size_t>(count.value));
  for (int32_t index = 0; index < count.value; ++index) {
    const auto item = list->TryCall<"get_Item", FleetPlayerData*(int32_t)>(index);
    if (!item)
      return std::nullopt;
    PinRow row{.ship = item.value};
    if (row.ship) {
      if (!row.ship->Is<FleetPlayerData>())
        return std::nullopt;
      auto root = il2cpp::GcHandle::Strong(row.ship);
      if (!root)
        return std::nullopt;
      result.rows_owned.push_back(std::move(root));
      const auto has_ship = row.ship->TryCall<"get_HasShip", bool>();
      if (!has_ship)
        return std::nullopt;
      row.non_ship = !has_ship.value;
      if (!row.non_ship) {
        row.id = row.ship->ShipInstanceId();
        if (names) {
          const auto level = row.ship->TryCall<"get_Level", int64_t>();
          if (!level)
            return std::nullopt;
          row.level = level.value;
          row.words = ShipNameWords(row.ship->DisplayName());
        }
      }
    }
    result.rows.push_back(std::move(row));
  }
  return result;
}

// This is the old45d legacy rule: highest level, native order on ties, one row
// per name occurrence. Explicit IDs and placeholders are never claimed by names.
std::vector<bool> PinLegacyMask(const std::vector<PinRow>& rows, const std::vector<std::string>& names,
                                const std::vector<int64_t>& explicit_ids)
{
  std::vector<bool> matched(rows.size(), false);
  for (const auto& name : names) {
    const auto            words = StrSplit(name, ' ');
    std::optional<size_t> best;
    for (size_t i = 0; i < rows.size(); ++i) {
      const auto& row = rows[i];
      if (row.non_ship || matched[i] || (row.id && pinned_ship_order::Contains(explicit_ids, *row.id))
          || !ShipNameMatches(row.words, words))
        continue;
      if (!best || row.level > rows[*best].level)
        best = i;
    }
    if (best)
      matched[*best] = true;
  }
  return matched;
}
void PinShips(ShipManagementViewContext* context)
{
  auto&      state = PinState();
  const auto names = PinAppliedLegacyNames();
  state.legacy_ids.clear();
  if (!state.writes_safe || (state.ids.empty() && names.empty()))
    return;
  auto snapshot = PinReadSnapshot(context, !names.empty());
  if (!snapshot) {
    if (!std::exchange(state.snapshot_warned, true))
      spdlog::warn("[PinnedShips] ship list is unavailable; keeping the native order");
    return;
  }
  auto*                               list   = snapshot->list.Get<PinList>();
  const auto                          legacy = PinLegacyMask(snapshot->rows, names, state.ids);
  std::vector<std::optional<int64_t>> ids;
  std::vector<bool>                   non_ships;
  std::vector<FleetPlayerData*>       original;
  for (size_t i = 0; i < snapshot->rows.size(); ++i) {
    const auto& row = snapshot->rows[i];
    ids.push_back(row.id);
    non_ships.push_back(row.non_ship);
    original.push_back(row.ship);
    if (legacy[i] && row.id)
      state.legacy_ids.push_back(*row.id);
  }
  const auto order  = pinned_ship_order::SortedIndices(ids, state.ids, legacy, non_ships);
  const auto result = ship_order::Apply<FleetPlayerData*>(
      original, order,
      [list]() -> std::optional<int32_t> {
        const auto result = list->TryCall<"get_Count", int32_t>();
        return result ? std::optional{result.value} : std::nullopt;
      },
      [list](int32_t index) -> std::optional<FleetPlayerData*> {
        const auto result = list->TryCall<"get_Item", FleetPlayerData*(int32_t)>(index);
        return result ? std::optional{result.value} : std::nullopt;
      },
      [list](int32_t index, FleetPlayerData* ship) {
        return list->TryCall<"set_Item", void(int32_t, FleetPlayerData*)>(index, ship).succeeded;
      });
  if (result == ship_order::Result::Restored || result == ship_order::Result::Unrestored) {
    state.writes_safe = false;
    spdlog::error("[PinnedShips] managed reorder failed; original order {}. Further pin edits are disabled",
                  result == ship_order::Result::Restored ? "restored and verified" : "could not be verified");
    ship_pin_input::RefreshVisible();
  }
}
void ShipManagementViewContext_Sort_Hook(auto original, ShipManagementViewContext* _this)
{
  original(_this);
  try {
    PinShips(_this);
  } catch (const std::exception& error) {
    spdlog::warn("[PinnedShips] could not prepare ship order: {}", error.what());
  }
}

std::vector<std::optional<int64_t>> PinLegacyAssignments(const PinSnapshot&              snapshot,
                                                         const std::vector<std::string>& names,
                                                         const std::vector<int64_t>&     explicit_ids)
{
  std::vector<std::optional<int64_t>> assignments;
  std::vector<bool>                   used(snapshot.rows.size(), false);
  auto                                claimed = explicit_ids;
  for (const auto& name : names) {
    const auto            words = StrSplit(name, ' ');
    std::optional<size_t> best;
    for (size_t i = 0; i < snapshot.rows.size(); ++i) {
      const auto& row = snapshot.rows[i];
      if (row.non_ship || used[i] || (row.id && pinned_ship_order::Contains(claimed, *row.id))
          || !ShipNameMatches(row.words, words))
        continue;
      if (!best || row.level > snapshot.rows[*best].level)
        best = i;
    }
    assignments.push_back(best ? snapshot.rows[*best].id : std::nullopt);
    if (best) {
      used[*best] = true;
      if (snapshot.rows[*best].id)
        claimed.push_back(*snapshot.rows[*best].id);
    }
  }
  return assignments;
}
bool PinQueue(pinned_ship_state::PinOrder desired, ShipManagementViewContext* context)
{
  auto&      state    = PinState();
  auto       weak     = il2cpp::GcHandle::Weak(context);
  const auto revision = mod_state::QueueShipPins(desired.ids, desired.legacy_names);
  if (!revision) {
    spdlog::warn("[PinnedShips] pin edit was not admitted; the applied order is unchanged");
    return false;
  }
  state.pending.Queue(std::move(desired), revision);
  state.save_context = std::move(weak);
  return true;
}
void PinReceiptWork()
{
  auto& state = PinState();
  if (state.pending.Desired()) {
    const auto completion = state.pending.Observe(mod_state::Status(mod_state::Group::ShipPins));
    if (completion != pinned_ship_state::PendingPinOrder::Completion::Waiting) {
      auto desired = state.pending.Take();
      auto context = std::move(state.save_context);
      if (completion == pinned_ship_state::PendingPinOrder::Completion::Saved) {
        state.ids          = std::move(desired.ids);
        state.legacy_names = std::move(desired.legacy_names);
        state.legacy_ids.clear();
        state.has_id_order    = true;
        state.refresh_context = std::move(context);
        state.refresh_frames  = 0;
      } else {
        spdlog::warn("[PinnedShips] pin edit was not saved; the previously applied order is retained");
      }
    }
  }
  if (state.refresh_context && ++state.refresh_frames >= 2) {
    auto  weak    = std::move(state.refresh_context);
    auto  strong  = il2cpp::GcHandle::Strong(weak.Get());
    auto* context = strong.Get<ShipManagementViewContext>();
    if (context) {
      for (auto* widget : unity::Instances<AssignShipsWidget>()) {
        if (!widget->isActiveAndEnabled() || widget->Context() != context)
          continue;
        const auto sorted = context->TryCall<"Sort", void()>();
        if (sorted && widget->isActiveAndEnabled() && widget->Context() == context)
          (void)widget->SortingChanged();
        break;
      }
    }
    ship_pin_input::RefreshVisible();
  }
  ship_pin_input::Tick();
}
void PinReceiptFrame()
{
  auto& state = PinState();
  if (!state.receipt_healthy)
    return;
  try {
    PinReceiptWork();
  } catch (...) {
    // Durable admission is independent of native publication. An unavailable
    // receipt cannot be claimed as failed/saved or applied to another view.
    state.receipt_healthy = false;
    try {
      ship_pin_input::RefreshVisible();
    } catch (...) {
    }
    try {
      spdlog::warn("[PinnedShips] receipt processing unavailable; local pin edits disabled");
    } catch (...) {
    }
  }
}

// --- Double click and Enter to assign ---

constexpr auto kDoubleClickWindow = std::chrono::milliseconds(400);

// Shared tile callback installation is terminal for the process.
bool double_click_installed = false;
// Compared only, never dereferenced.
FleetPlayerData*                      last_clicked_ship = nullptr;
std::chrono::steady_clock::time_point last_click_time{};

void PressAssignButton()
{
  for (auto* widget : unity::Instances<AssignShipsWidget>()) {
    // The tracker snapshot is borrowed. Root before managed calls and check
    // Unity lifetime independently; no custody or cached dock survives this call.
    auto owner = il2cpp::GcHandle::Strong(widget);
    if (!owner || !unity::Alive(widget) || !CanvasController::OnScreen(widget))
      continue;
    auto* wrapper = widget->assignButton();
    auto* inner   = wrapper ? wrapper->Widget() : nullptr;
    // Only the first shown dock is pressed, even when its button cannot be read.
    if (auto* button = inner ? inner->UnityButton() : nullptr)
      button->Press();
    return;
  }
}

void AssignOnDoubleClick(FleetPlayerData* ship)
{
  if (!double_click_installed || !Config::Get().ui.double_click_to_assign_ship)
    return;
  const auto now          = std::chrono::steady_clock::now();
  const bool double_click = ship == last_clicked_ship && now - last_click_time <= kDoubleClickWindow;
  last_clicked_ship       = ship;
  last_click_time         = now;
  if (!double_click)
    return;
  // Consumed, so a third click does not count as another double click.
  last_clicked_ship = nullptr;
  PressAssignButton();
}

void AssignOnEnterFrame()
{
  // Down, not Pressed: a held Enter must press Assign once, not every frame.
  if (dock_assignment::EnterRequested(double_click_installed))
    PressAssignButton();
}

// --- Ship tile click ---

// Pin badges and ordinary double click share one tile callback owner.
void ShipTileWidget_HandleOnClick_Hook(auto original, ShipTileWidget* _this)
{
  if (ship_pin_input::ConsumeTileClick(_this)) {
    last_clicked_ship = nullptr;
    return;
  }
  original(_this);
  // The click arrives through the UI event system, which can run before the frame tick has polled this frame.
  unity::Key::Poll();
  if (auto* ship = _this->Context()) {
    AssignOnDoubleClick(ship);
  }
}

// --- Dock selection ---

// The fleet open in ship management, when it holds a ship and has a dock slot.
FleetPlayerData* ManagedDockFleet()
{
  auto* sections = Hub::Sections();
  if (!Config::Get().ui.dock_syncmanagement || !sections
      || sections->CurrentSection() != SectionID::ShipManagement_Details)
    return nullptr;
  auto* controller = VisibleShipManagement();
  auto* context    = controller ? controller->Context() : nullptr;
  auto* fleet      = context ? context->CurrentFleet() : nullptr;
  auto* manager    = FleetsManager::Instance();
  if (!fleet || !fleet->HasShip() || !manager || !manager->HasFleetService())
    return nullptr;
  // Unassigned and synthetic fleets have no dock slot; the management list order is not the dock order.
  const auto index      = fleet->Index();
  auto*      dock_fleet = index >= 0 ? manager->GetFleetPlayerData(index) : nullptr;
  return dock_fleet && dock_fleet->Id() == fleet->Id() ? fleet : nullptr;
}

void DockSelectionFrame()
{
  // Ten times a second; selecting a slot is not urgent.
  static Throttle throttle(std::chrono::milliseconds(100));
  static int64_t  last_fleet_id = 0;
  if (!throttle.Due())
    return;
  auto* fleet = ManagedDockFleet();
  if (!fleet) {
    last_fleet_id = 0;
    return;
  }
  const auto id  = fleet->Id();
  auto*      bar = unity::Newest<FleetBarViewController>();
  if (id == last_fleet_id || !bar)
    return;
  if (const auto index = fleet->Index(); !bar->IsIndexSelected(index)) {
    // The bar's own selection flow updates both the highlight and the fleet panel.
    bar->RequestSelect(index);
    // Blocked by the game: try again next time.
    if (!bar->IsIndexSelected(index))
      return;
    spdlog::debug("[DockSelection] selected dock slot {} for fleet {}", index + 1, id);
  }
  last_fleet_id = id;
}

// --- Installation ---

bool InstallPinnedShips()
{
  auto& state = PinState();
  if (!state.loaded) {
    state.loaded       = true;
    state.worker_ready = mod_state::PrepareAsync();
    if (const auto stored = mod_state::Read()) {
      if (const auto order = pinned_ship_state::ReadOrder(*stored)) {
        state.ids          = *order;
        state.legacy_names = pinned_ship_state::ReadLegacyNames(*stored);
        state.has_id_order = true;
      }
    }
  }
  if (!ShipManagementViewContext::Class::HasField<"SortedIdleShips", PinList*>()
      || !FleetPlayerData::Class::HasProperty<"HasShip", bool>())
    return false;
  il2cpp::HookSet hooks("PinnedShips");
  IL2CPP_STAGE(hooks, ShipManagementViewContext::Class::Method("Sort", 0), ShipManagementViewContext_Sort_Hook);
  state.sort_installed = hooks.Commit();
  return state.sort_installed;
}

// Shared by pin-badge input and double-click assignment, whichever of them install, so it is its own set,
// installed on the first call.
bool InstallShipTileClick()
{
  static const bool tile_click_installed = [] {
    il2cpp::HookSet hooks("ShipTileClick");
    IL2CPP_STAGE(hooks, ShipTileWidget::Class::Method("HandleOnClick", 0), ShipTileWidget_HandleOnClick_Hook);
    return hooks.Commit();
  }();
  return tile_click_installed;
}

bool InstallPinnedShipClick()
{
  auto& state = PinState();
  if (!state.sort_installed || !state.worker_ready || !FleetPlayerData::ShipIdentityAvailable())
    return false;
  const auto frame         = EnsureFrameTick();
  state.receipt_registered = frame.Installed() && RegisterFrameCallback(PinReceiptFrame, "ShipPinReceipts");
  state.input_installed    = state.receipt_registered && InstallShipTileClick() && ship_pin_input::Install();
  return state.input_installed;
}

bool InstallDoubleClickAssign()
{
  double_click_installed = InstallShipTileClick();
  return double_click_installed;
}
} // namespace

namespace ship_pins
{
bool InputReady()
{
  const auto& state = PinState();
  return state.input_installed && state.receipt_registered && state.receipt_healthy && state.writes_safe
         && ship_pin_input::Available() && FrameDeliveryAvailable();
}
bool IsPinned(FleetPlayerData* ship)
{
  const auto id = ship ? ship->ShipInstanceId() : std::nullopt;
  return id
         && (pinned_ship_order::Contains(PinState().ids, *id)
             || pinned_ship_order::Contains(PinState().legacy_ids, *id));
}
bool Edit(FleetPlayerData* ship, ShipManagementViewContext* context, bool move, std::optional<int64_t> target)
{
  if (!InputReady() || !ship || !context)
    return false;
  const auto id = ship->ShipInstanceId();
  if (!id)
    return false;
  auto snapshot = PinReadSnapshot(context, true);
  if (!snapshot || std::ranges::none_of(snapshot->rows, [&](const PinRow& row) { return row.id == id; }))
    return false;
  auto       ids          = PinDesiredIds();
  auto       names        = PinDesiredLegacyNames();
  const auto assignments  = PinLegacyAssignments(*snapshot, names, ids);
  auto       migration    = pinned_ship_order::MigrateLegacyNames(names, assignments, *id);
  const bool explicit_pin = pinned_ship_order::Contains(ids, *id);
  if (!move && pinned_ship_order::AmbiguousUnpin(migration, explicit_pin, [&](const std::string& name) {
        const auto words = ShipNameWords(ship->DisplayName());
        return words.empty() || ShipNameMatches(words, StrSplit(name, ' '));
      })) {
    spdlog::warn("[PinnedShips] unresolved legacy pins could reclaim the selected ship; edit ignored");
    return false;
  }
  ids.insert(ids.end(), migration.pins.begin(), migration.pins.end());
  if (move) {
    if (!(target ? pinned_ship_order::MoveToTarget(ids, *id, *target) : pinned_ship_order::MoveToEnd(ids, *id)))
      return false;
  } else if (!migration.selected_was_legacy) {
    pinned_ship_order::Toggle(ids, *id);
  }
  return PinQueue({std::move(ids), std::move(migration.unresolved_names)}, context);
}
bool Toggle(FleetPlayerData* ship, ShipManagementViewContext* context)
{
  try {
    return Edit(ship, context, false, {});
  } catch (const std::exception& error) {
    spdlog::warn("[PinnedShips] pin edit unavailable: {}", error.what());
    return false;
  }
}
bool Place(FleetPlayerData* ship, std::optional<int64_t> target, ShipManagementViewContext* context)
{
  try {
    return Edit(ship, context, true, target);
  } catch (const std::exception& error) {
    spdlog::warn("[PinnedShips] pin move unavailable: {}", error.what());
    return false;
  }
}
} // namespace ship_pins

const Feature kPinnedShips{"PinnedShips", &Config::Patches::pinnedshiphooks, InstallPinnedShips};
// The old downstream parsed pinnedshipclicktoggle but did not consume it. Pin
// badge input follows the actual pinnedshiphooks owner; Shift-click is not a substitute.
const Feature kPinnedShipClick{"PinnedShipClick", &Config::Patches::pinnedshiphooks, InstallPinnedShipClick};
const Feature kDoubleClickAssign{"DoubleClickAssign", &Config::Patches::doubleclickassignshiphooks,
                                 InstallDoubleClickAssign};
// Enter was driven by the hotkey patch in PLAY, independently of ordinary shortcut mode.
const Feature kAssignOnEnter{"AssignOnEnter", &Config::Patches::hotkeyhooks, nullptr, AssignOnEnterFrame};
// Frame work only, installed with the hotkeys it was introduced with.
const Feature kDockSelection{"DockSelection", &Config::Patches::hotkeyhooks, nullptr, DockSelectionFrame};
