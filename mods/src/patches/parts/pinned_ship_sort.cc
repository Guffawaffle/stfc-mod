#include "config.h"
#include "errormsg.h"
#include "mod_state.h"
#include "patches/pinned_ship_order.h"
#include "patches/pinned_ship_sort.h"
#include "patches/pinned_ship_state.h"
#include "patches/screen_update_hook.h"
#include "patches/ship_identity.h"
#include "patches/swap_ship_pin_input.h"
#include "ship_name_match.h"

#include <prime/AssignShipsWidget.h>
#include <prime/FleetPlayerData.h>
#include <prime/HullSpec.h>

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

// Sort only the ship-management context. Explicit ship-ID pins lead in saved
// order; legacy name pins and unpinned ships retain the game's active sort.

namespace {

struct PinnedShipSortState {
  IL2CppClassHelper* shipManagementViewContext = nullptr;
  IL2CppFieldHelper* sortedIdleShipsField      = nullptr;
  std::vector<int64_t> pinned_ids;
  std::vector<std::string> legacy_names;
  bool               has_id_order              = false;
  bool               valid                     = false;
  bool               input_available           = false;
};

PinnedShipSortState& State()
{
  static PinnedShipSortState s;
  return s;
}

Il2CppGCHandle pending_refresh = nullptr;
int pending_refresh_ticks = 0;

void FlushPendingRefresh()
{
  if (!pending_refresh)
    return;
  if (++pending_refresh_ticks < 2)
    return;
  auto handle = pending_refresh;
  pending_refresh = nullptr;
  pending_refresh_ticks = 0;
  auto* context = il2cpp_gchandle_get_target(handle);
  static auto widget = AssignShipsWidget::get_class_helper();
  static auto parent = widget.GetParent("Widget`1");
  static auto* bound_context = parent.GetMethodInfo("get_Context", 0);
  static auto* refresh = widget.GetMethodInfo("HandleSortingChanged", 1);
  bool refreshed = false;
  if (context && bound_context && refresh && refresh->parameters_count == 1
      && refresh->parameters[0]->type == IL2CPP_TYPE_I4) {
    for (auto* item : ObjectFinder<AssignShipsWidget>::GetAll()) {
      if (!item || !item->isActiveAndEnabled)
        continue;
      Il2CppException* exception = nullptr;
      auto* bound = il2cpp_runtime_invoke(bound_context, item, nullptr, &exception);
      if (exception || bound != context)
        continue;
      int dirty = 0;
      void* args[]{&dirty};
      exception = nullptr;
      il2cpp_runtime_invoke(refresh, item, args, &exception);
      refreshed = !exception;
      if (!refreshed)
        spdlog::warn("[PinnedShipSort] Assign Ships list redraw failed");
      break;
    }
  }
  if (refreshed)
    spdlog::debug("[PinnedShipSort] refreshed active Assign Ships list");
  else
    spdlog::warn("[PinnedShipSort] saved pins but no active Assign Ships list was refreshed");
  il2cpp_gchandle_free(handle);
}

bool InitializeState()
{
  auto& s = State();

  s.shipManagementViewContext = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementViewContext"));
  if (!s.shipManagementViewContext->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Prime.Ships", "ShipManagementViewContext");
    return false;
  }

  s.sortedIdleShipsField = new IL2CppFieldHelper(s.shipManagementViewContext->GetField("SortedIdleShips"));
  if (!s.sortedIdleShipsField->isValidHelper()) {
    spdlog::error("[PinnedShipSort] ShipManagementViewContext field layout changed");
    return false;
  }

  s.valid = true;
  return true;
}

struct ShipEntry {
  void*         item;
  std::optional<int64_t> id;
  std::vector<std::string> match_words; // words from the game's own display name
  int64_t       level;
  bool          non_ship;
};

ShipEntry BuildShipEntry(void* item, bool legacy_names)
{
  ShipEntry entry{item, {}, {}, 0, !item};

  auto* ship = reinterpret_cast<FleetPlayerData*>(item);
  if (!ship) return entry;
  if (!ship->HasShip) {
    entry.non_ship = true;
    return entry;
  }

  entry.id = ship_identity::InstanceId(ship);
  if (legacy_names) {
    entry.level       = ship->Level;
    entry.match_words = ShipNameMatch::DisplayWords(ship);
  }
  return entry;
}

void ReorderPinnedShips(void* list)
{
  if (!list) return;

  const auto& cfg = Config::Get();
  auto&       state = State();
  const auto& legacy_names = state.has_id_order ? state.legacy_names : cfg.pinned_ships;
  if (state.pinned_ids.empty() && legacy_names.empty()) return;

  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(list));
  if (!listClass) return;

  auto* getCount = il2cpp_class_get_method_from_name(listClass, "get_Count", 0);
  auto* getItem  = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  auto* clear    = il2cpp_class_get_method_from_name(listClass, "Clear", 0);
  auto* add      = il2cpp_class_get_method_from_name(listClass, "Add", 1);
  if (!getCount || !getItem || !clear || !add) {
    spdlog::warn("[PinnedShipSort] SortedIdleShips list is missing expected List<T> methods");
    return;
  }

  Il2CppException* exc = nullptr;
  auto* countObj = il2cpp_runtime_invoke(getCount, list, nullptr, &exc);
  if (exc || !countObj) return;
  const auto count = *reinterpret_cast<int32_t*>(il2cpp_object_unbox(countObj));
  if (count <= 1) return;
  if (count > 2000) {
    spdlog::warn("[PinnedShipSort] SortedIdleShips reported an implausible count ({}); skipping", count);
    return;
  }

  std::vector<ShipEntry> ships;
  ships.reserve(count);
  for (int32_t i = 0; i < count; ++i) {
    void* idxArgs[1] = {&i};
    exc              = nullptr;
    auto* item       = il2cpp_runtime_invoke(getItem, list, idxArgs, &exc);
    if (exc) item = nullptr;

    auto entry = BuildShipEntry(item, !legacy_names.empty());
    ships.push_back(std::move(entry));
  }

  std::vector<std::optional<int64_t>> ids;
  std::vector<bool> non_ships;
  ids.reserve(ships.size());
  non_ships.reserve(ships.size());
  for (const auto& entry : ships) {
    ids.push_back(entry.id);
    non_ships.push_back(entry.non_ship);
  }

  std::vector<bool> legacy_pins(count, false);
  for (const auto& name : legacy_names) {
    const auto words = ShipNameMatch::SplitWords(name);
    if (words.empty()) continue;
    int     best_index = -1;
    int64_t best_level = -1;
    for (int32_t i = 0; i < count; ++i) {
      if (non_ships[i] || legacy_pins[i] || (ids[i] && pinned_ship_order::Contains(state.pinned_ids, *ids[i]))) continue;
      if (!ShipNameMatch::MatchesDisplay(ships[i].match_words, words)) continue;
      if (ships[i].level > best_level) {
        best_level = ships[i].level;
        best_index = i;
      }
    }
    if (best_index >= 0)
      legacy_pins[best_index] = true;
    else
      spdlog::warn("[PinnedShipSort] legacy pin '{}' matched no idle ship", name);
  }
  const auto order = pinned_ship_order::SortedIndices(ids, state.pinned_ids, legacy_pins, non_ships);

  std::vector<void*> items;
  items.reserve(count);
  for (const auto& entry : ships) items.push_back(entry.item);

  bool changed = false;
  for (int32_t i = 0; i < count; ++i) {
    if (order[i] != i) {
      changed = true;
      break;
    }
  }
  if (!changed) return;

  exc = nullptr;
  il2cpp_runtime_invoke(clear, list, nullptr, &exc);
  if (exc) {
    spdlog::warn("[PinnedShipSort] list.Clear raised an exception; aborting reorder");
    return;
  }

  for (int32_t i = 0; i < count; ++i) {
    void* addArgs[1] = {items[order[i]]};
    exc               = nullptr;
    il2cpp_runtime_invoke(add, list, addArgs, &exc);
    if (exc) {
      spdlog::warn("[PinnedShipSort] list.Add raised an exception while re-inserting an item");
    }
  }
}

void ShipManagementViewContext_Sort_Hook(auto original, void* _this)
{
  original(_this);

  auto& s = State();
  if (!s.valid) return;

  auto* list = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.sortedIdleShipsField->offset());
  ReorderPinnedShips(list);
}

std::optional<std::vector<std::optional<int64_t>>> LegacyAssignments(Il2CppObject* context,
                                                                       const std::vector<std::string>& names)
{
  auto& state = State();
  if (names.empty())
    return std::vector<std::optional<int64_t>>{};
  if (!context)
    return std::nullopt;
  auto* list = *reinterpret_cast<void**>(reinterpret_cast<char*>(context) + state.sortedIdleShipsField->offset());
  if (!list)
    return std::nullopt;
  auto* cls = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(list));
  auto* count_method = cls ? il2cpp_class_get_method_from_name(cls, "get_Count", 0) : nullptr;
  auto* item_method = cls ? il2cpp_class_get_method_from_name(cls, "get_Item", 1) : nullptr;
  if (!count_method || !item_method)
    return std::nullopt;
  Il2CppException* exception = nullptr;
  auto* boxed_count = il2cpp_runtime_invoke(count_method, list, nullptr, &exception);
  if (exception || !boxed_count)
    return std::nullopt;
  const int count = *static_cast<int32_t*>(il2cpp_object_unbox(boxed_count));
  if (count < 0 || count > 2000)
    return std::nullopt;
  std::vector<ShipEntry> ships;
  ships.reserve(count);
  for (int index = 0; index < count; ++index) {
    void* args[]{&index};
    exception = nullptr;
    auto* item = il2cpp_runtime_invoke(item_method, list, args, &exception);
    if (exception)
      return std::nullopt;
    ships.push_back(BuildShipEntry(item, true));
  }
  std::vector<bool> used(ships.size(), false);
  std::vector<std::optional<int64_t>> assignments;
  assignments.reserve(names.size());
  auto claimed_ids = state.pinned_ids;
  for (const auto& name : names) {
    const auto words = ShipNameMatch::SplitWords(name);
    int64_t best_level = -1;
    std::optional<std::size_t> best_index;
    for (std::size_t index = 0; index < ships.size(); ++index) {
      const auto& ship = ships[index];
      if (ship.non_ship || used[index] || ship.level <= best_level || words.empty()
          || (ship.id && pinned_ship_order::Contains(claimed_ids, *ship.id))
          || !ShipNameMatch::MatchesDisplay(ship.match_words, words))
        continue;
      best_level = ship.level;
      best_index = index;
    }
    if (best_index) {
      used[*best_index] = true;
      assignments.push_back(ships[*best_index].id);
      if (ships[*best_index].id)
        claimed_ids.push_back(*ships[*best_index].id);
    } else {
      assignments.push_back(std::nullopt);
    }
  }
  return assignments;
}

bool SaveAndRefresh(std::vector<int64_t> updated, std::vector<std::string> legacy,
                    Il2CppObject* selection_context)
{
  auto& state = State();
  if (!mod_state::TryUpdate([&](nlohmann::json& data) {
        pinned_ship_state::WriteOrder(data, updated);
        pinned_ship_state::WriteLegacyNames(data, legacy);
      })) {
    spdlog::warn("[PinnedShipSort] could not save ship-ID pin order; action ignored");
    return false;
  }
  state.pinned_ids   = std::move(updated);
  state.legacy_names = std::move(legacy);
  state.has_id_order = true;

  Il2CppException* exception = nullptr;
  auto* sort = state.shipManagementViewContext->GetMethodInfo("Sort", 0);
  if (sort)
    il2cpp_runtime_invoke(sort, selection_context, nullptr, &exception);
  if (exception)
    spdlog::warn("[PinnedShipSort] saved pins but could not refresh the current ship sort");
  if (pending_refresh)
    il2cpp_gchandle_free(pending_refresh);
  pending_refresh = il2cpp_gchandle_new(selection_context, false);
  pending_refresh_ticks = 0;
  if (!pending_refresh)
    spdlog::warn("[PinnedShipSort] saved pins but could not queue a ship-bar redraw");
  swap_ship_pin_input::RefreshVisibleBadges();
  return true;
}

} // namespace

namespace pinned_ship_sort
{
bool Available()
{ return State().valid && State().input_available; }

std::optional<std::size_t> Rank(FleetPlayerData* ship)
{
  if (!ship || !ship->HasShip)
    return std::nullopt;
  const auto id = ship_identity::InstanceId(ship);
  if (!id)
    return std::nullopt;
  const auto& pins = State().pinned_ids;
  const auto  found = std::find(pins.begin(), pins.end(), *id);
  return found == pins.end() ? std::nullopt : std::optional<std::size_t>{found - pins.begin() + 1};
}

bool HandleCardAction(FleetPlayerData* ship, Il2CppObject* selection_context)
{
  auto& state = State();
  if (!state.valid || !ship || !ship->HasShip || !selection_context)
    return false;
  const auto id = ship_identity::InstanceId(ship);
  if (!id)
    return false;

  auto updated = state.pinned_ids;
  auto legacy  = state.has_id_order ? state.legacy_names : Config::Get().pinned_ships;
  const auto assignments = LegacyAssignments(selection_context, legacy);
  if (!assignments) {
    spdlog::warn("[PinnedShipSort] could not resolve legacy pins; action ignored");
    return true;
  }
  auto migrated = pinned_ship_order::MigrateLegacyNames(legacy, *assignments, *id);
  updated.insert(updated.end(), migrated.pins.begin(), migrated.pins.end());
  if (!migrated.selected_was_legacy)
    pinned_ship_order::Toggle(updated, *id);

  if (SaveAndRefresh(std::move(updated), std::move(migrated.unresolved_names), selection_context))
    spdlog::info("[PinnedShipSort] toggled ship={} ({} pins)", *id, state.pinned_ids.size());
  return true;
}

bool PlacePinnedShip(FleetPlayerData* source, std::optional<int64_t> target_id, Il2CppObject* selection_context)
{
  auto& state = State();
  if (!state.valid || !source || !source->HasShip || !selection_context)
    return false;
  const auto id = ship_identity::InstanceId(source);
  if (!id)
    return false;
  auto updated = state.pinned_ids;
  auto legacy = state.has_id_order ? state.legacy_names : Config::Get().pinned_ships;
  const auto assignments = LegacyAssignments(selection_context, legacy);
  if (!assignments) {
    spdlog::warn("[PinnedShipSort] could not resolve legacy pins; drag ignored");
    return false;
  }
  auto migrated = pinned_ship_order::MigrateLegacyNames(legacy, *assignments, *id);
  updated.insert(updated.end(), migrated.pins.begin(), migrated.pins.end());
  if (!(target_id ? pinned_ship_order::MoveToTarget(updated, *id, *target_id)
                  : pinned_ship_order::MoveToEnd(updated, *id)))
    return false;
  if (!SaveAndRefresh(std::move(updated), std::move(migrated.unresolved_names), selection_context))
    return false;
  if (target_id)
    spdlog::info("[PinnedShipSort] dragged ship={} to ship={} ({} pins)", *id, *target_id,
                 state.pinned_ids.size());
  else
    spdlog::info("[PinnedShipSort] dragged ship={} to end ({} pins)", *id, state.pinned_ids.size());
  return true;
}
} // namespace pinned_ship_sort

void InstallPinnedShipSortHooks()
{
  if (const auto persisted = mod_state::Read()) {
    if (const auto order = pinned_ship_state::ReadOrder(*persisted)) {
      State().pinned_ids   = *order;
      State().legacy_names = pinned_ship_state::ReadLegacyNames(*persisted);
      State().has_id_order = true;
      spdlog::info("[PinnedShipSort] loaded {} ship-ID pins", order->size());
    }
  }
  if (!InitializeState()) {
    spdlog::error("[PinnedShipSort] initialization failed; hooks not installed");
    return;
  }

  auto& s = State();

  auto sortMethod = s.shipManagementViewContext->GetMethod("Sort", 0);
  if (!sortMethod) {
    s.valid = false;
    ErrorMsg::MissingMethod("ShipManagementViewContext", "Sort");
    return;
  }

  if (!SPUD_STATIC_DETOUR(sortMethod, ShipManagementViewContext_Sort_Hook)) {
    s.valid = false;
    spdlog::error("[PinnedShipSort] failed to install ShipManagementViewContext.Sort detour");
    return;
  }
  s.input_available = swap_ship_pin_input::Install();
  if (!s.input_available)
    spdlog::warn("[PinnedShipSort] pointer polling unavailable; pin badge input disabled");
  if (!install_screen_manager_update_hook() || !register_screen_manager_update_callback(FlushPendingRefresh))
    spdlog::warn("[PinnedShipSort] ship-bar redraw polling unavailable");
  spdlog::info("Pinned ship sort: hooks installed");
}
