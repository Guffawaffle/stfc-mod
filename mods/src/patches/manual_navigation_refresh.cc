#include "patches/manual_navigation_refresh.h"

#include "config.h"
#include "prime/Hub.h"
#include "prime/IList.h"
#include "prime/NavigationSectionManager.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
constexpr size_t  kMaxTrackedSystems                 = 64;
constexpr int64_t kManualNavigationRefreshCooldownMs = 5000;
constexpr int     kManualNavigationRefreshMinDrainTicks = 2;
constexpr int     kManualNavigationRefreshDrainTimeoutTicks = 120;
constexpr int     kManualNavigationRefreshRepopulateTimeoutTicks = 240;
constexpr int     kManualNavigationRefreshStableTicks = 24;
constexpr int     kSolarSystemNodeDepth = 2;

enum class ManualNavigationRefreshStage {
  Idle,
  WaitingForDrain,
  WaitingForRepopulate,
};

struct NavigationManagerState {
  int64_t solarSystemNodeId            = 0;
  int     switchCount                  = 0;
  bool    resourcesLoaded              = false;
  bool    loadSolarSystemError         = false;
  bool    movingBackToGalaxyDueToError = false;
  void*   viewData                     = nullptr;
  void*   solarSystem                  = nullptr;
  void*   galaxyManagers               = nullptr;
  void*   systemManagers               = nullptr;
};

struct RuntimeLayerTotals {
  int scannedCollections   = 0;
  int scannedItems         = 0;
  int scannedFleetEntities = 0;
  int movementListCount    = -1;
};

std::vector<void*> g_fleet_entity_movement_systems;
std::vector<void*> g_system_view_vfx_systems;

bool                         g_system_view_active                        = false;
bool                         g_manual_navigation_refresh_pending         = false;
bool                         g_manual_navigation_refresh_running         = false;
ManualNavigationRefreshStage g_manual_navigation_refresh_stage           = ManualNavigationRefreshStage::Idle;
int64_t                      g_manual_navigation_refresh_next_allowed_ms = 0;
const char*                  g_manual_navigation_refresh_source          = "manual";
int64_t                      g_manual_navigation_refresh_captured_node_id = 0;
int64_t                      g_manual_navigation_refresh_reload_node_id   = 0;
int64_t                      g_manual_navigation_refresh_last_load_node_id = 0;
int64_t                      g_manual_navigation_refresh_last_setwatch_node_id = 0;
int                          g_manual_navigation_refresh_stage_ticks      = 0;
int                          g_manual_navigation_refresh_stable_ticks     = 0;
int                          g_manual_navigation_refresh_last_layer_items = -1;
int                          g_manual_navigation_refresh_last_layer_fleet_entities = -1;
bool                         g_manual_navigation_refresh_saw_depth2       = false;
bool                         g_manual_navigation_refresh_saw_setwatch_node = false;
bool                         g_manual_navigation_refresh_repopulation_started = false;
const char*                  g_manual_navigation_refresh_reload_node_source = "unresolved";
const char*                  g_manual_navigation_refresh_reload_primitive   = "unresolved";
int                          g_load_solar_system_depth                    = 0;
void*                        g_last_navigation_manager                    = nullptr;

IL2CppClassHelper& SectionManagerHelper()
{
  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  return class_helper;
}

IL2CppClassHelper& NavigationManagerHelper()
{
  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  return class_helper;
}

IL2CppClassHelper& NavigationCameraEventsHelper()
{
  static auto class_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationCameraEvents");
  return class_helper;
}

ptrdiff_t runtime_system_field_offset(const char* class_name, const char* field_name)
{
  auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", class_name);
  if (!class_helper.isValidHelper()) {
    return -1;
  }

  auto field = class_helper.GetField(field_name);
  return field.isValidHelper() ? field.offset() : -1;
}

ptrdiff_t navigation_manager_field_offset(const char* field_name)
{
  auto& class_helper = NavigationManagerHelper();
  if (!class_helper.isValidHelper()) {
    return -1;
  }

  auto field = class_helper.GetField(field_name);
  return field.isValidHelper() ? field.offset() : -1;
}

void* read_reference_field(void* instance, ptrdiff_t offset)
{
  if (!instance || offset < 0) {
    return nullptr;
  }

  return *reinterpret_cast<void**>(static_cast<char*>(instance) + offset);
}

int read_int_field(void* instance, ptrdiff_t offset)
{
  if (!instance || offset < 0) {
    return 0;
  }

  return *reinterpret_cast<int*>(static_cast<char*>(instance) + offset);
}

int64_t read_int64_field(void* instance, ptrdiff_t offset)
{
  if (!instance || offset < 0) {
    return 0;
  }

  return *reinterpret_cast<int64_t*>(static_cast<char*>(instance) + offset);
}

bool read_bool_field(void* instance, ptrdiff_t offset)
{
  if (!instance || offset < 0) {
    return false;
  }

  return *reinterpret_cast<bool*>(static_cast<char*>(instance) + offset);
}

int64_t read_int64_property(void* instance, IL2CppClassHelper& class_helper, const char* property_name)
{
  if (!instance || !class_helper.isValidHelper()) {
    return 0;
  }

  auto property = class_helper.GetProperty(property_name);
  if (!property.isValidHelper()) {
    return 0;
  }

  auto value = property.Get<int64_t>(instance);
  return value ? *value : 0;
}

const char* object_class_name(void* object)
{
  auto* il2cpp_object = reinterpret_cast<Il2CppObject*>(object);
  if (!il2cpp_object || !il2cpp_object->klass || !il2cpp_object->klass->name) {
    return "";
  }

  return il2cpp_object->klass->name;
}

bool is_navigation_fleet_entity(void* entity)
{ return std::strcmp(object_class_name(entity), "NavigationFleetEntity") == 0; }

IList* runtime_set_items(void* runtime_set)
{
  if (!runtime_set) {
    return nullptr;
  }

  static auto runtime_set_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "RuntimeSet");
  static auto get_items          = runtime_set_helper.GetMethod<IList*(void*)>("get_Items", 0);
  if (!get_items) {
    return nullptr;
  }

  return get_items(runtime_set);
}

IList* fleet_entity_movement_static_entities()
{
  static auto helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", "FleetEntityMovementSystem");
  static FieldInfo* field =
      helper.isValidHelper() ? il2cpp_class_get_field_from_name(helper.get_cls(), "NavigationFleetEntities") : nullptr;
  if (!field) {
    return nullptr;
  }

  IList* list = nullptr;
  il2cpp_field_static_get_value(field, &list);
  return list;
}

void scan_navigation_fleet_totals(RuntimeLayerTotals& totals, IList* items)
{
  if (!items) {
    return;
  }

  ++totals.scannedCollections;
  const auto item_count = items->Count < 0 ? 0 : items->Count;
  totals.scannedItems += item_count;

  for (int index = 0; index < item_count; ++index) {
    if (is_navigation_fleet_entity(items->Get(index))) {
      ++totals.scannedFleetEntities;
    }
  }
}

void scan_runtime_set_field_totals(RuntimeLayerTotals& totals, ptrdiff_t field_offset,
                                   const std::vector<void*>& systems)
{
  for (auto* system : systems) {
    scan_navigation_fleet_totals(totals, runtime_set_items(read_reference_field(system, field_offset)));
  }
}

void scan_runtime_list_field_totals(RuntimeLayerTotals& totals, ptrdiff_t field_offset,
                                    const std::vector<void*>& systems)
{
  for (auto* system : systems) {
    scan_navigation_fleet_totals(totals, reinterpret_cast<IList*>(read_reference_field(system, field_offset)));
  }
}

RuntimeLayerTotals navigation_runtime_layer_totals()
{
  RuntimeLayerTotals totals;
  scan_runtime_set_field_totals(totals, runtime_system_field_offset("FleetEntityMovementSystem", "_fleetEntitySet"),
                                g_fleet_entity_movement_systems);
  scan_runtime_set_field_totals(totals, runtime_system_field_offset("SystemViewVFXSystem", "_fleetEntitySet"),
                                g_system_view_vfx_systems);
  scan_runtime_list_field_totals(totals, runtime_system_field_offset("SystemViewVFXSystem", "_queue"),
                                 g_system_view_vfx_systems);
  scan_navigation_fleet_totals(totals, fleet_entity_movement_static_entities());

  if (auto* movement_list = fleet_entity_movement_static_entities()) {
    totals.movementListCount = movement_list->Count < 0 ? 0 : movement_list->Count;
  }

  return totals;
}

int64_t monotonic_now_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

const char* section_name(SectionID section_id)
{
  switch (section_id) {
    case SectionID::Navigation_System:
      return "Navigation_System";
    case SectionID::Navigation_Galaxy:
      return "Navigation_Galaxy";
    case SectionID::Navigation_Planet:
      return "Navigation_Planet";
    default:
      return "other";
  }
}

SectionID active_navigation_section()
{
  if (auto* section_manager = Hub::get_SectionManager(); section_manager) {
    return section_manager->CurrentSection;
  }

  return SectionID::ZDummyTerminator;
}

NavigationManagerState read_navigation_manager_state(void* manager)
{
  static auto solar_system_node_id_offset               = navigation_manager_field_offset("_solarSystemNodeId");
  static auto switch_count_offset                       = navigation_manager_field_offset("_switchCount");
  static auto resources_loaded_offset                   = navigation_manager_field_offset("_resourcesLoaded");
  static auto load_solar_system_error_offset            = navigation_manager_field_offset("_loadSolarSystemError");
  static auto moving_back_to_galaxy_due_to_error_offset =
      navigation_manager_field_offset("_movingBackToGalaxyDueToError");
  static auto view_data_offset       = navigation_manager_field_offset("_viewData");
  static auto solar_system_offset    = navigation_manager_field_offset("_system");
  static auto galaxy_managers_offset = navigation_manager_field_offset("_galaxyManagers");
  static auto system_managers_offset = navigation_manager_field_offset("_systemManagers");

  NavigationManagerState state{};
  state.solarSystemNodeId            = read_int64_field(manager, solar_system_node_id_offset);
  state.switchCount                  = read_int_field(manager, switch_count_offset);
  state.resourcesLoaded              = read_bool_field(manager, resources_loaded_offset);
  state.loadSolarSystemError         = read_bool_field(manager, load_solar_system_error_offset);
  state.movingBackToGalaxyDueToError = read_bool_field(manager, moving_back_to_galaxy_due_to_error_offset);
  state.viewData                     = read_reference_field(manager, view_data_offset);
  state.solarSystem                  = read_reference_field(manager, solar_system_offset);
  state.galaxyManagers               = read_reference_field(manager, galaxy_managers_offset);
  state.systemManagers               = read_reference_field(manager, system_managers_offset);
  return state;
}

int64_t read_navigation_manager_reload_node_id(void* manager, const NavigationManagerState& state, const char*& source)
{
  auto& class_helper = NavigationManagerHelper();
  if (class_helper.isValidHelper()) {
    if (auto watch_node_id = read_int64_property(manager, class_helper, "WatchNodeId"); watch_node_id > 0) {
      source = "NavigationManager.WatchNodeId";
      return watch_node_id;
    }

    static auto watch_node_id_offset = navigation_manager_field_offset("_watchNodeId");
    if (auto watch_node_id = read_int64_field(manager, watch_node_id_offset); watch_node_id > 0) {
      source = "NavigationManager._watchNodeId";
      return watch_node_id;
    }

    if (auto current_node_id = read_int64_property(manager, class_helper, "CurrentNodeId"); current_node_id > 0) {
      source = "NavigationManager.CurrentNodeId";
      return current_node_id;
    }

    static auto current_node_id_offset = navigation_manager_field_offset("_currentNodeId");
    if (auto current_node_id = read_int64_field(manager, current_node_id_offset); current_node_id > 0) {
      source = "NavigationManager._currentNodeId";
      return current_node_id;
    }

    if (auto node_id = read_int64_property(manager, class_helper, "NodeId"); node_id > 0) {
      source = "NavigationManager.NodeId";
      return node_id;
    }

    static auto node_id_offset = navigation_manager_field_offset("_nodeId");
    if (auto node_id = read_int64_field(manager, node_id_offset); node_id > 0) {
      source = "NavigationManager._nodeId";
      return node_id;
    }
  }

  if (state.solarSystemNodeId > 0) {
    source = "NavigationManager._solarSystemNodeId";
    return state.solarSystemNodeId;
  }

  source = "unavailable";
  return 0;
}

void log_manual_navigation_refresh_state(const char* phase, const char* reason, void* manager)
{
  const auto manager_state = read_navigation_manager_state(manager);
  const auto totals        = navigation_runtime_layer_totals();
  const auto active_section = active_navigation_section();
  const auto system_like_runtime_layer_present = totals.scannedItems > 0 || totals.scannedFleetEntities > 0
                                                 || totals.movementListCount > 0 || !g_system_view_vfx_systems.empty();

  spdlog::info("[ManualNavigationRefresh] phase={} reason={} source={} current_node_id={} reload_node_id={} "
               "reload_node_source={} active_nav_section={} system_view_active={} load_depth={} layer_items={} "
               "layer_fleet_entities={} movement_list_count={} system_like_runtime_layer_present={} "
               "resources_loaded={} load_solar_system_error={} moving_back_to_galaxy_due_to_error={} "
               "tracked_movement_systems={} tracked_vfx_systems={}",
               phase, reason ? reason : "none", g_manual_navigation_refresh_source, manager_state.solarSystemNodeId,
               g_manual_navigation_refresh_reload_node_id, g_manual_navigation_refresh_reload_node_source,
               section_name(active_section), g_system_view_active, g_load_solar_system_depth, totals.scannedItems,
               totals.scannedFleetEntities, totals.movementListCount, system_like_runtime_layer_present,
               manager_state.resourcesLoaded, manager_state.loadSolarSystemError,
               manager_state.movingBackToGalaxyDueToError, g_fleet_entity_movement_systems.size(),
               g_system_view_vfx_systems.size());
}

bool trigger_synthetic_navigation_leave_event()
{
  auto& class_helper = NavigationCameraEventsHelper();
  if (!class_helper.isValidHelper()) {
    return false;
  }

  static auto trigger_leave = class_helper.GetMethod<void()>("TriggerLeaveNavigationViewEvent", 0);
  if (!trigger_leave) {
    return false;
  }

  trigger_leave();
  return true;
}

bool request_manual_navigation_system_reload(void* navigation_manager, int64_t reload_node_id,
                                             const char*& primitive_used, bool& section_state_present,
                                             bool& section_trigger_accepted)
{
  primitive_used           = "unavailable";
  section_state_present    = false;
  section_trigger_accepted = false;

  if (auto* section_manager = Hub::get_SectionManager(); section_manager) {
    if (auto* section_storage = section_manager->_sectionStorage) {
      if (auto* section_data = section_storage->GetState(SectionID::Navigation_System)) {
        section_state_present = true;
        static auto trigger_section_change =
            SectionManagerHelper().GetMethod<bool(void*, SectionID, void*, bool, bool, bool)>("TriggerSectionChange");
        if (trigger_section_change) {
          primitive_used = "SectionManager.TriggerSectionChange(saved Navigation_System state)";
          section_trigger_accepted =
              trigger_section_change(section_manager, SectionID::Navigation_System, section_data, false, false, true);
          if (section_trigger_accepted) {
            return true;
          }
        }
      }
    }
  }

  static auto navigation_section_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationSectionManager");
  if (navigation_section_manager_helper.isValidHelper()) {
    static auto change_navigation_section =
        navigation_section_manager_helper.GetMethod<void(SectionID, int64_t)>("ChangeNavigationSection", 2);
    if (change_navigation_section) {
      primitive_used = "NavigationSectionManager.ChangeNavigationSection(Navigation_System,-1)";
      change_navigation_section(SectionID::Navigation_System, -1);
      return true;
    }
  }

  auto& navigation_manager_helper = NavigationManagerHelper();
  if (navigation_manager_helper.isValidHelper()) {
    static auto load_solar_system = navigation_manager_helper.GetMethod<void(void*, int64_t)>("LoadSolarSystem", 1);
    if (load_solar_system && navigation_manager && reload_node_id > 0) {
      primitive_used = "NavigationManager.LoadSolarSystem(fallback)";
      load_solar_system(navigation_manager, reload_node_id);
      return true;
    }
  }

  return false;
}

template <typename T>
void remember_tracked_system(std::vector<T*>& systems, T* system)
{
  if (!system || std::ranges::find(systems, system) != systems.end()) {
    return;
  }

  if (systems.size() >= kMaxTrackedSystems) {
    systems.erase(systems.begin());
  }

  systems.push_back(system);
}

template <typename T>
void forget_tracked_system(std::vector<T*>& systems, T* system)
{
  std::erase(systems, system);
}

bool navigation_refresh_context_allowed(void* navigation_manager, const NavigationManagerState& manager_state,
                                        const RuntimeLayerTotals& totals, const char*& reason)
{
  if (!navigation_manager) {
    reason = "no-navigation-manager";
    return false;
  }

  if (g_load_solar_system_depth > 0) {
    reason = "load-solar-system-in-progress";
    return false;
  }

  if (manager_state.loadSolarSystemError || manager_state.movingBackToGalaxyDueToError) {
    reason = "unsafe-manager-state";
    return false;
  }

  const auto active_section = active_navigation_section();
  const auto system_like_runtime_layer_present = totals.scannedItems > 0 || totals.scannedFleetEntities > 0
                                                 || totals.movementListCount > 0 || !g_system_view_vfx_systems.empty();
  if (active_section != SectionID::Navigation_System && active_section != SectionID::Navigation_Galaxy
      && !system_like_runtime_layer_present) {
    reason = "not-in-navigation";
    return false;
  }

  reason = nullptr;
  return true;
}

void run_manual_navigation_refresh_if_requested(void* navigation_manager)
{
  g_last_navigation_manager = navigation_manager ? navigation_manager : g_last_navigation_manager;

  if (g_manual_navigation_refresh_pending && !g_manual_navigation_refresh_running) {
    const auto manager_state = read_navigation_manager_state(g_last_navigation_manager);
    const auto totals        = navigation_runtime_layer_totals();
    const char* reason       = nullptr;
    if (!navigation_refresh_context_allowed(g_last_navigation_manager, manager_state, totals, reason)) {
      g_manual_navigation_refresh_pending = false;
      log_manual_navigation_refresh_state("skip", reason, g_last_navigation_manager);
      return;
    }

    const char* reload_node_source = "unresolved";
    const auto  reload_node_id =
        read_navigation_manager_reload_node_id(g_last_navigation_manager, manager_state, reload_node_source);

    g_manual_navigation_refresh_pending          = false;
    g_manual_navigation_refresh_running          = true;
    g_manual_navigation_refresh_stage            = ManualNavigationRefreshStage::WaitingForDrain;
    g_manual_navigation_refresh_stage_ticks      = 0;
    g_manual_navigation_refresh_stable_ticks     = 0;
    g_manual_navigation_refresh_captured_node_id = manager_state.solarSystemNodeId;
    g_manual_navigation_refresh_reload_node_id   = reload_node_id;
    g_manual_navigation_refresh_reload_node_source = reload_node_source;
    g_manual_navigation_refresh_reload_primitive = "unresolved";
    g_manual_navigation_refresh_last_layer_items = totals.scannedItems;
    g_manual_navigation_refresh_last_layer_fleet_entities = totals.scannedFleetEntities;
    g_manual_navigation_refresh_saw_depth2       = false;
    g_manual_navigation_refresh_saw_setwatch_node = false;
    g_manual_navigation_refresh_repopulation_started = false;
    g_manual_navigation_refresh_last_load_node_id = 0;
    g_manual_navigation_refresh_last_setwatch_node_id = 0;

    const auto leave_called = trigger_synthetic_navigation_leave_event();
    spdlog::info("[ManualNavigationRefresh] phase=drain-start source={} captured_node_id={} reload_node_id={} "
                 "reload_node_source={} leave_called={} layer_items={} layer_fleet_entities={} "
                 "movement_list_count={} system_view_active={} active_nav_section={}",
                 g_manual_navigation_refresh_source, g_manual_navigation_refresh_captured_node_id,
                 g_manual_navigation_refresh_reload_node_id, g_manual_navigation_refresh_reload_node_source,
                 leave_called, totals.scannedItems, totals.scannedFleetEntities, totals.movementListCount,
                 g_system_view_active, section_name(active_navigation_section()));
    return;
  }

  if (!g_manual_navigation_refresh_running) {
    return;
  }

  auto totals = navigation_runtime_layer_totals();
  if (g_manual_navigation_refresh_stage == ManualNavigationRefreshStage::WaitingForDrain) {
    ++g_manual_navigation_refresh_stage_ticks;
    const auto drained = totals.scannedItems == 0 && totals.scannedFleetEntities == 0 && totals.movementListCount <= 0;
    const auto timed_out = g_manual_navigation_refresh_stage_ticks >= kManualNavigationRefreshDrainTimeoutTicks;
    const auto min_ticks_met = g_manual_navigation_refresh_stage_ticks >= kManualNavigationRefreshMinDrainTicks;
    if (min_ticks_met && (drained || timed_out)) {
      auto        section_state_present    = false;
      auto        section_trigger_accepted = false;
      const char* reload_primitive         = "unresolved";
      const auto  reload_called = request_manual_navigation_system_reload(
          g_last_navigation_manager, g_manual_navigation_refresh_reload_node_id, reload_primitive,
          section_state_present, section_trigger_accepted);
      g_manual_navigation_refresh_reload_primitive = reload_primitive;

      spdlog::info("[ManualNavigationRefresh] phase=reload-start source={} reload_node_id={} reload_node_source={} "
                   "drained={} timed_out={} stage_ticks={} reload_called={} reload_primitive='{}' "
                   "section_state_present={} section_trigger_accepted={} layer_items={} layer_fleet_entities={} "
                   "movement_list_count={} system_view_active={} active_nav_section={}",
                   g_manual_navigation_refresh_source, g_manual_navigation_refresh_reload_node_id,
                   g_manual_navigation_refresh_reload_node_source, drained, timed_out,
                   g_manual_navigation_refresh_stage_ticks, reload_called,
                   g_manual_navigation_refresh_reload_primitive, section_state_present, section_trigger_accepted,
                   totals.scannedItems, totals.scannedFleetEntities, totals.movementListCount, g_system_view_active,
                   section_name(active_navigation_section()));

      if (!reload_called) {
        g_manual_navigation_refresh_stage = ManualNavigationRefreshStage::Idle;
        g_manual_navigation_refresh_running = false;
        g_manual_navigation_refresh_next_allowed_ms = monotonic_now_ms() + kManualNavigationRefreshCooldownMs;
        log_manual_navigation_refresh_state("complete", "reload-unavailable", g_last_navigation_manager);
        return;
      }

      g_manual_navigation_refresh_stage = ManualNavigationRefreshStage::WaitingForRepopulate;
      g_manual_navigation_refresh_stage_ticks = 0;
      g_manual_navigation_refresh_stable_ticks = 0;
      g_manual_navigation_refresh_last_layer_items = totals.scannedItems;
      g_manual_navigation_refresh_last_layer_fleet_entities = totals.scannedFleetEntities;
      return;
    }
  }

  if (g_manual_navigation_refresh_stage == ManualNavigationRefreshStage::WaitingForRepopulate) {
    ++g_manual_navigation_refresh_stage_ticks;
    totals = navigation_runtime_layer_totals();
    if (totals.scannedItems > 0 || totals.scannedFleetEntities > 0 || totals.movementListCount > 0) {
      g_manual_navigation_refresh_repopulation_started = true;
    }

    const auto counts_stable = totals.scannedItems == g_manual_navigation_refresh_last_layer_items
                               && totals.scannedFleetEntities
                                      == g_manual_navigation_refresh_last_layer_fleet_entities;
    if (g_manual_navigation_refresh_repopulation_started && counts_stable) {
      ++g_manual_navigation_refresh_stable_ticks;
    } else {
      g_manual_navigation_refresh_stable_ticks = 0;
    }

    g_manual_navigation_refresh_last_layer_items = totals.scannedItems;
    g_manual_navigation_refresh_last_layer_fleet_entities = totals.scannedFleetEntities;

    const auto timed_out = g_manual_navigation_refresh_stage_ticks >= kManualNavigationRefreshRepopulateTimeoutTicks;
    const auto navigation_confirmed =
        g_manual_navigation_refresh_saw_depth2 || g_manual_navigation_refresh_saw_setwatch_node;
    const auto stable_repopulated = g_manual_navigation_refresh_repopulation_started && navigation_confirmed
                                    && g_manual_navigation_refresh_stable_ticks
                                           >= kManualNavigationRefreshStableTicks;
    if (stable_repopulated || timed_out) {
      spdlog::info("[ManualNavigationRefresh] phase=complete source={} reload_node_id={} repopulated={} "
                   "navigation_confirmed={} timed_out={} stage_ticks={} stable_ticks={} saw_setwatch_node={} "
                   "saw_depth2={} reload_primitive='{}' last_load_node_id={} last_setwatch_node_id={} "
                   "layer_items={} layer_fleet_entities={} movement_list_count={} system_view_active={} "
                   "active_nav_section={}",
                   g_manual_navigation_refresh_source, g_manual_navigation_refresh_reload_node_id,
                   g_manual_navigation_refresh_repopulation_started, navigation_confirmed, timed_out,
                   g_manual_navigation_refresh_stage_ticks, g_manual_navigation_refresh_stable_ticks,
                   g_manual_navigation_refresh_saw_setwatch_node, g_manual_navigation_refresh_saw_depth2,
                   g_manual_navigation_refresh_reload_primitive, g_manual_navigation_refresh_last_load_node_id,
                   g_manual_navigation_refresh_last_setwatch_node_id, totals.scannedItems,
                   totals.scannedFleetEntities, totals.movementListCount, g_system_view_active,
                   section_name(active_navigation_section()));

      g_manual_navigation_refresh_stage = ManualNavigationRefreshStage::Idle;
      g_manual_navigation_refresh_running = false;
      g_manual_navigation_refresh_next_allowed_ms = monotonic_now_ms() + kManualNavigationRefreshCooldownMs;
    }
  }
}

void NavigationManager_Update_Hook(auto original, void* manager)
{
  g_last_navigation_manager = manager;
  original(manager);
  run_manual_navigation_refresh_if_requested(manager);
}

void NavigationManager_LoadSolarSystem_Hook(auto original, void* manager, int64_t node_id)
{
  ++g_load_solar_system_depth;
  if (g_manual_navigation_refresh_running && node_id > 0) {
    g_manual_navigation_refresh_last_load_node_id = node_id;
  }
  spdlog::info("[ManualNavigationRefresh] navigation_manager method=LoadSolarSystem phase=before node_id={} "
               "load_depth={} system_view_active={} active_nav_section={}",
               node_id, g_load_solar_system_depth, g_system_view_active, section_name(active_navigation_section()));
  original(manager, node_id);
  spdlog::info("[ManualNavigationRefresh] navigation_manager method=LoadSolarSystem phase=after node_id={} "
               "load_depth={} system_view_active={} active_nav_section={}",
               node_id, g_load_solar_system_depth, g_system_view_active, section_name(active_navigation_section()));
  --g_load_solar_system_depth;
}

void NavigationManager_SetWatchNodeId_Hook(auto original, void* manager, int64_t node_id, bool trigger_change,
                                           bool clear_starbase_data_cache)
{
  if (g_manual_navigation_refresh_running && node_id > 0) {
    g_manual_navigation_refresh_last_setwatch_node_id = node_id;
    if (node_id == g_manual_navigation_refresh_reload_node_id) {
      g_manual_navigation_refresh_saw_setwatch_node = true;
    }
  }
  spdlog::info("[ManualNavigationRefresh] navigation_manager method=SetWatchNodeId phase=before node_id={} "
               "trigger_change={} clear_starbase_data_cache={} system_view_active={} active_nav_section={}",
               node_id, trigger_change, clear_starbase_data_cache, g_system_view_active,
               section_name(active_navigation_section()));
  original(manager, node_id, trigger_change, clear_starbase_data_cache);
}

void NavigationManager_OnViewChanged_Hook(auto original, void* manager, int32_t depth)
{
  if (g_manual_navigation_refresh_running && depth == kSolarSystemNodeDepth) {
    g_manual_navigation_refresh_saw_depth2 = true;
  }
  spdlog::info("[ManualNavigationRefresh] navigation_manager method=OnViewChanged phase=before depth={} "
               "system_view_active={} active_nav_section={}",
               depth, g_system_view_active, section_name(active_navigation_section()));
  original(manager, depth);
}

void FleetEntityMovementSystem_Update_Hook(auto original, void* system)
{
  remember_tracked_system(g_fleet_entity_movement_systems, system);
  original(system);
}

void FleetEntityMovementSystem_Destroy_Hook(auto original, void* system)
{
  forget_tracked_system(g_fleet_entity_movement_systems, system);
  original(system);
}

void SystemViewVFXSystem_Initialize_Hook(auto original, void* system)
{
  original(system);
  remember_tracked_system(g_system_view_vfx_systems, system);
  g_system_view_active = true;
}

void SystemViewVFXSystem_Update_Hook(auto original, void* system)
{
  remember_tracked_system(g_system_view_vfx_systems, system);
  g_system_view_active = true;
  original(system);
}

void SystemViewVFXSystem_Destroy_Hook(auto original, void* system)
{
  forget_tracked_system(g_system_view_vfx_systems, system);
  if (g_system_view_vfx_systems.empty()) {
    g_system_view_active = false;
  }
  original(system);
}
} // namespace

bool ManualNavigationRefreshRequest(const char* source)
{
  g_manual_navigation_refresh_source = source && source[0] ? source : "manual";

  if (!ManualNavigationRefreshEnabled()) {
    log_manual_navigation_refresh_state("skip", "disabled-in-config", g_last_navigation_manager);
    return false;
  }

  if (g_manual_navigation_refresh_running || g_manual_navigation_refresh_pending) {
    log_manual_navigation_refresh_state("skip", "already-running", g_last_navigation_manager);
    return false;
  }

  const auto now_ms = monotonic_now_ms();
  if (now_ms < g_manual_navigation_refresh_next_allowed_ms) {
    log_manual_navigation_refresh_state("skip", "cooldown", g_last_navigation_manager);
    return false;
  }

  g_manual_navigation_refresh_pending = true;
  spdlog::info("[ManualNavigationRefresh] phase=queued source={} cooldown_ms={}",
               g_manual_navigation_refresh_source, kManualNavigationRefreshCooldownMs);
  log_manual_navigation_refresh_state("queued", "hotkey", g_last_navigation_manager);
  return true;
}

void InstallManualNavigationRefreshHooks()
{
  auto& navigation_manager = NavigationManagerHelper();
  if (navigation_manager.isValidHelper()) {
    if (auto ptr = navigation_manager.GetMethod("Update", 0); ptr) {
      SPUD_STATIC_DETOUR(ptr, NavigationManager_Update_Hook);
    }
    if (auto ptr = navigation_manager.GetMethod("LoadSolarSystem", 1); ptr) {
      SPUD_STATIC_DETOUR(ptr, NavigationManager_LoadSolarSystem_Hook);
    }
    if (auto ptr = navigation_manager.GetMethod("SetWatchNodeId", 3); ptr) {
      SPUD_STATIC_DETOUR(ptr, NavigationManager_SetWatchNodeId_Hook);
    }
    if (auto ptr = navigation_manager.GetMethod("OnViewChanged", 1); ptr) {
      SPUD_STATIC_DETOUR(ptr, NavigationManager_OnViewChanged_Hook);
    }
  }

  if (LiveDebugChannelEnabled()) {
    spdlog::info("[ManualNavigationRefresh] runtime layer tracking hooks skipped reason=live-debug-hooks-active");
  } else {
    auto movement_system =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", "FleetEntityMovementSystem");
    if (movement_system.isValidHelper()) {
      if (auto update = movement_system.GetMethod("Update", 0); update) {
        SPUD_STATIC_DETOUR(update, FleetEntityMovementSystem_Update_Hook);
      }
      if (auto destroy = movement_system.GetMethod("Destroy", 0); destroy) {
        SPUD_STATIC_DETOUR(destroy, FleetEntityMovementSystem_Destroy_Hook);
      }
    }

    auto vfx_system = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", "SystemViewVFXSystem");
    if (vfx_system.isValidHelper()) {
      if (auto initialize = vfx_system.GetMethod("Initialize", 0); initialize) {
        SPUD_STATIC_DETOUR(initialize, SystemViewVFXSystem_Initialize_Hook);
      }
      if (auto update = vfx_system.GetMethod("Update", 0); update) {
        SPUD_STATIC_DETOUR(update, SystemViewVFXSystem_Update_Hook);
      }
      if (auto destroy = vfx_system.GetMethod("Destroy", 0); destroy) {
        SPUD_STATIC_DETOUR(destroy, SystemViewVFXSystem_Destroy_Hook);
      }
    }
  }

  spdlog::info("[ManualNavigationRefresh] hooks installed enabled={} shortcut=manual_navigation_refresh",
               ManualNavigationRefreshEnabled());
}
