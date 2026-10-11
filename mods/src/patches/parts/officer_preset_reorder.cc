#include "config.h"
#include "errormsg.h"
#include "mod_state.h"
#include "prime/KeyCode.h"
#include "settings/upstream_features.h"
#include "str_utils.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
struct OfficerPresetItemContext {
  Il2CppObject  object;
  void*         synthetic_fleet_player_data;
  int32_t       presentation;
  bool          is_save_available;
  bool          is_occupied;
  int32_t       order_id;
  int64_t       slot_id;
  Il2CppString* preset_name;
  Il2CppArray*  officers;
  Il2CppArray*  below_deck_officers;
  bool          below_deck_slots_unlocked;
  void*         officer_presets_view_context;
};

using ClearAndGenerateContentsFn = void(void*, Il2CppObject*, Il2CppObject*);
using GetScrollPositionFn        = float(void*);
using RestoreScrollPositionFn    = void(void*, float);

bool reorder_installed             = false;
bool diagnostic_extra_layout_valid = false;

std::vector<int64_t>        session_order;
std::vector<int32_t>        active_presentations;
Il2CppObject*               active_controller          = nullptr;
Il2CppClass*                item_context_class         = nullptr;
ptrdiff_t                   widget_context_offset      = 0;
ptrdiff_t                   controller_context_offset  = 0;
ptrdiff_t                   controller_scroller_offset = 0;
ptrdiff_t                   presets_items_offset       = 0;
ClearAndGenerateContentsFn* clear_and_generate         = nullptr;
GetScrollPositionFn*        get_scroll_position        = nullptr;
RestoreScrollPositionFn*    restore_scroll_position    = nullptr;

// Share the normal .log sinks, but keep this diagnostic build's evidence at info even when the user changes verbosity.
// Do not register this logger: spdlog::set_level (including the logging hotkey) must not silence the capture.
spdlog::logger& diagnostics()
{
  static auto logger = [] {
    auto source = spdlog::default_logger();
    auto result =
        std::make_shared<spdlog::logger>("OfficerPresetDiagnostics", source->sinks().begin(), source->sinks().end());
    result->set_level(spdlog::level::info);
    result->flush_on(spdlog::level::info);
    return result;
  }();
  return *logger;
}

void log_session_order(const char* phase)
{
  auto order = nlohmann::json::array();
  for (const auto slot : session_order)
    order.push_back(std::to_string(slot));
  diagnostics().info("[OfficerPresetReorder] phase={} saved_positions={} saved_order={}", phase, session_order.size(),
                     order.dump());
}

void load_session_order()
{
  const auto state = mod_state::Read();
  if (!state) {
    diagnostics().info("[OfficerPresetReorder] phase=state-load state unavailable; starting with session order");
    log_session_order("state-load");
    return;
  }

  try {
    const auto order = state->find("officer_preset_order");
    if (order == state->end() || !order->is_array()) {
      diagnostics().info("[OfficerPresetReorder] phase=state-load order_present={} order_is_array={}",
                         order != state->end(), order != state->end() && order->is_array());
      log_session_order("state-load");
      return;
    }

    std::vector<int64_t> loaded_order;
    loaded_order.reserve(order->size());
    size_t invalid_entries   = 0;
    size_t duplicate_entries = 0;
    for (const auto& item : *order) {
      int64_t slot_id = -1;
      if (item.is_string()) {
        const auto& value  = item.get_ref<const std::string&>();
        const auto  result = std::from_chars(value.data(), value.data() + value.size(), slot_id);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
          ++invalid_entries;
          continue;
        }
      } else if (item.is_number_integer()) {
        slot_id = item.get<int64_t>();
      } else {
        ++invalid_entries;
        continue;
      }

      if (slot_id >= 0 && std::find(loaded_order.begin(), loaded_order.end(), slot_id) == loaded_order.end()) {
        loaded_order.push_back(slot_id);
      } else if (slot_id < 0) {
        ++invalid_entries;
      } else {
        ++duplicate_entries;
      }
    }

    session_order = std::move(loaded_order);
    diagnostics().info("[OfficerPresetReorder] phase=state-load raw_entries={} invalid={} duplicates={}", order->size(),
                       invalid_entries, duplicate_entries);
    log_session_order("state-load");
    spdlog::info("[OfficerPresetReorder] loaded {} persisted slot positions", session_order.size());
  } catch (const std::exception& error) {
    spdlog::warn("[OfficerPresetReorder] ignored invalid persisted order: {}", error.what());
    diagnostics().warn("[OfficerPresetReorder] phase=state-load exception={}", error.what());
    log_session_order("state-load-exception");
  }
}

bool save_session_order()
{
  auto order = nlohmann::json::array();
  for (const auto slot_id : session_order) {
    order.push_back(std::to_string(slot_id));
  }
  const bool saved =
      mod_state::Update([&order](nlohmann::json& state) { state["officer_preset_order"] = std::move(order); });
  diagnostics().info("[OfficerPresetReorder] phase=state-save success={}", saved);
  log_session_order("state-save");
  return saved;
}

static_assert(offsetof(OfficerPresetItemContext, presentation) == 0x18);
static_assert(offsetof(OfficerPresetItemContext, is_occupied) == 0x1D);
static_assert(offsetof(OfficerPresetItemContext, order_id) == 0x20);
static_assert(offsetof(OfficerPresetItemContext, slot_id) == 0x28);
static_assert(offsetof(OfficerPresetItemContext, preset_name) == 0x30);

bool validate_context_field(Il2CppClass* context_class, const char* name, ptrdiff_t expected_offset,
                            Il2CppTypeEnum expected_type)
{
  auto* field = il2cpp_class_get_field_from_name(context_class, name);
  if (field == nullptr || field->type == nullptr) {
    diagnostics().error("[OfficerPresetReorder] phase=layout field={} unavailable", name);
    spdlog::error("[OfficerPresetReorder] required context field '{}' is unavailable", name);
    return false;
  }
  diagnostics().info("[OfficerPresetReorder] phase=layout field={} offset=0x{:X} type={} expected_offset=0x{:X} "
                     "expected_type={}",
                     name, field->offset, static_cast<int>(field->type->type), expected_offset,
                     static_cast<int>(expected_type));
  if (field->offset != expected_offset || field->type->type != expected_type) {
    spdlog::error("[OfficerPresetReorder] context field '{}' layout mismatch: offset=0x{:X} type={} expected "
                  "offset=0x{:X} type={}; feature disabled",
                  name, field->offset, static_cast<int>(field->type->type), expected_offset,
                  static_cast<int>(expected_type));
    return false;
  }
  return true;
}

bool validate_context_layout(Il2CppClass* context_class)
{
  return context_class != nullptr
         && validate_context_field(context_class, "Presentation", offsetof(OfficerPresetItemContext, presentation),
                                   IL2CPP_TYPE_VALUETYPE)
         && validate_context_field(context_class, "IsOccupied", offsetof(OfficerPresetItemContext, is_occupied),
                                   IL2CPP_TYPE_BOOLEAN)
         && validate_context_field(context_class, "OrderId", offsetof(OfficerPresetItemContext, order_id),
                                   IL2CPP_TYPE_I4)
         && validate_context_field(context_class, "SlotId", offsetof(OfficerPresetItemContext, slot_id), IL2CPP_TYPE_I8)
         && validate_context_field(context_class, "PresetName", offsetof(OfficerPresetItemContext, preset_name),
                                   IL2CPP_TYPE_STRING)
         && validate_context_field(context_class, "Officers", offsetof(OfficerPresetItemContext, officers),
                                   IL2CPP_TYPE_SZARRAY)
         && validate_context_field(context_class, "_officerPresetsViewContext",
                                   offsetof(OfficerPresetItemContext, officer_presets_view_context), IL2CPP_TYPE_CLASS);
}

bool is_reorderable_preset(const OfficerPresetItemContext* context)
{
  return context != nullptr && context->slot_id >= 0 && context->order_id >= 0 && context->preset_name != nullptr
         && context->officers != nullptr && reinterpret_cast<Il2CppArraySize*>(context->officers)->max_length > 0;
}

template <typename T> T* read_object_field(void* object, ptrdiff_t offset)
{ return object != nullptr ? *reinterpret_cast<T**>(reinterpret_cast<char*>(object) + offset) : nullptr; }

bool key_pressed(KeyCode key)
{
  static auto get_key = il2cpp_resolve_icall_typed<bool(KeyCode)>("UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)");
  return get_key != nullptr && get_key(key);
}

bool shift_pressed()
{ return key_pressed(KeyCode::LeftShift) || key_pressed(KeyCode::RightShift); }

bool move_down_modifier_pressed()
{
#if __APPLE__
  return key_pressed(KeyCode::LeftCommand) || key_pressed(KeyCode::RightCommand);
#else
  return key_pressed(KeyCode::LeftControl) || key_pressed(KeyCode::RightControl);
#endif
}

void remember_slots(OfficerPresetItemContext** items, il2cpp_array_size_t size)
{
  for (il2cpp_array_size_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (!is_reorderable_preset(context)) {
      continue;
    }
    if (std::find(session_order.begin(), session_order.end(), context->slot_id) == session_order.end()) {
      session_order.push_back(context->slot_id);
    }
  }
}

bool has_native_identity(const OfficerPresetItemContext* context)
{ return context != nullptr && context->slot_id >= 0 && context->order_id >= 0; }

void log_preset_row(const char* phase, uint64_t snapshot, int32_t index, const OfficerPresetItemContext* context)
{
  if (context == nullptr) {
    diagnostics().info("[OfficerPresetReorder] phase={} snapshot={} row={} context=null", phase, snapshot, index);
    return;
  }
  if (context->object.klass != item_context_class) {
    diagnostics().warn("[OfficerPresetReorder] phase={} snapshot={} row={} context={} class={} expected_class={}; "
                       "row details omitted for unexpected class",
                       phase, snapshot, index, static_cast<const void*>(context),
                       static_cast<void*>(context->object.klass), static_cast<void*>(item_context_class));
    return;
  }
  const auto position    = std::find(session_order.begin(), session_order.end(), context->slot_id);
  const auto saved_index = position == session_order.end() ? -1 : std::distance(session_order.begin(), position);
  // JSON quoting keeps embedded newlines/control characters from splitting diagnostic records.
  const auto name = context->preset_name == nullptr ? "<null>"
                    : context->preset_name->length < 0 || context->preset_name->length > 256
                        ? "<length-out-of-range>"
                        : to_string(context->preset_name);
  diagnostics().info(
      "[OfficerPresetReorder] phase={} snapshot={} row={} context={} class={} slot={} order={} "
      "identity={} canonical={} occupied={} reorderable={} presentation={} saved_index={} "
      "name_ptr={} name_length={} name={} officers={} officer_count={} owner={}",
      phase, snapshot, index, static_cast<const void*>(context), static_cast<void*>(context->object.klass),
      context->slot_id, context->order_id, has_native_identity(context),
      !has_native_identity(context) || context->order_id == index, context->is_occupied, is_reorderable_preset(context),
      context->presentation, saved_index, static_cast<void*>(context->preset_name),
      context->preset_name != nullptr ? context->preset_name->length : -1,
      nlohmann::json(name).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace),
      static_cast<void*>(context->officers),
      context->officers != nullptr ? reinterpret_cast<Il2CppArraySize*>(context->officers)->max_length : 0,
      context->officer_presets_view_context);
  if (diagnostic_extra_layout_valid) {
    diagnostics().info("[OfficerPresetReorder] phase={} snapshot={} row={} save_available={} synthetic_fleet={} "
                       "below_deck={} below_deck_count={} below_deck_unlocked={}",
                       phase, snapshot, index, context->is_save_available, context->synthetic_fleet_player_data,
                       static_cast<void*>(context->below_deck_officers),
                       context->below_deck_officers != nullptr
                           ? reinterpret_cast<Il2CppArraySize*>(context->below_deck_officers)->max_length
                           : 0,
                       context->below_deck_slots_unlocked);
  }
  if (context->officers != nullptr) {
    auto*      array    = reinterpret_cast<Il2CppArraySize*>(context->officers);
    auto**     officers = reinterpret_cast<void**>(array->vector);
    auto       pointers = nlohmann::json::array();
    const auto count    = std::min<il2cpp_array_size_t>(array->max_length, 32);
    for (il2cpp_array_size_t slot = 0; slot < count; ++slot)
      pointers.push_back(fmt::format("{}", officers[slot]));
    diagnostics().info("[OfficerPresetReorder] phase={} snapshot={} row={} officer_pointers={} omitted_officers={}",
                       phase, snapshot, index, pointers.dump(), array->max_length - count);
  }
}

uint64_t log_preset_rows(const char* phase, OfficerPresetItemContext** items, int32_t size)
{
  static uint64_t sequence = 0;
  const auto      snapshot = ++sequence;
  diagnostics().info("[OfficerPresetReorder] phase={} snapshot={} rows={} storage={} active_controller={} "
                     "presentation_count={}",
                     phase, snapshot, size, static_cast<void*>(items), static_cast<void*>(active_controller),
                     active_presentations.size());
  log_session_order(phase);
  // Normal preset lists are small. Bound a corrupt/oversized capture while retaining both ends of the list.
  for (int32_t index = 0; index < size; ++index) {
    if (size > 256 && index == 128) {
      diagnostics().warn("[OfficerPresetReorder] phase={} snapshot={} omitted {} middle rows", phase, snapshot,
                         size - 256);
      index = size - 128;
    }
    log_preset_row(phase, snapshot, index, items[index]);
  }
  return snapshot;
}

bool validate_unique_preset_identity(OfficerPresetItemContext** items, int32_t size, const char* phase)
{
  std::vector<int64_t> slot_ids;
  std::vector<int32_t> order_ids;
  std::vector<int32_t> row_indices;
  slot_ids.reserve(size);
  order_ids.reserve(size);
  row_indices.reserve(size);
  bool     valid      = true;
  uint64_t snapshot   = 0;
  int32_t  collisions = 0;
  for (int32_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (!has_native_identity(context)) {
      continue;
    }
    const auto slot_match  = std::find(slot_ids.begin(), slot_ids.end(), context->slot_id);
    const auto order_match = std::find(order_ids.begin(), order_ids.end(), context->order_id);
    if (slot_match != slot_ids.end() || order_match != order_ids.end()) {
      spdlog::error("[OfficerPresetReorder] duplicate preset identity detected at index {}: slot={} order={}", index,
                    context->slot_id, context->order_id);
      if (valid)
        snapshot = log_preset_rows(phase, items, size);
      valid = false;
      if (++collisions <= 32) {
        const auto slot_row  = slot_match == slot_ids.end() ? -1 : row_indices[slot_match - slot_ids.begin()];
        const auto order_row = order_match == order_ids.end() ? -1 : row_indices[order_match - order_ids.begin()];
        diagnostics().error("[OfficerPresetReorder] phase={} snapshot={} duplicate_row={} slot={} order={} "
                            "slot_matches_row={} order_matches_row={} slot_pointer_alias={} order_pointer_alias={}",
                            phase, snapshot, index, context->slot_id, context->order_id, slot_row, order_row,
                            slot_row >= 0 && items[slot_row] == context, order_row >= 0 && items[order_row] == context);
        // Also include the exact matching rows when they fall outside the bounded overview.
        log_preset_row(phase, snapshot, index, context);
        if (slot_row >= 0)
          log_preset_row(phase, snapshot, slot_row, items[slot_row]);
        if (order_row >= 0 && order_row != slot_row)
          log_preset_row(phase, snapshot, order_row, items[order_row]);
      }
    }
    slot_ids.push_back(context->slot_id);
    order_ids.push_back(context->order_id);
    row_indices.push_back(index);
  }
  if (!valid)
    diagnostics().error("[OfficerPresetReorder] phase={} snapshot={} duplicate_rows={} detailed_limit=32; "
                        "identity guard retained",
                        phase, snapshot, collisions);
  return valid;
}

bool validate_canonical_order(OfficerPresetItemContext** items, int32_t size, const char* phase)
{
  if (!validate_unique_preset_identity(items, size, phase)) {
    return false;
  }
  for (int32_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (!has_native_identity(context)) {
      continue;
    }
    if (context->order_id != index) {
      spdlog::error("[OfficerPresetReorder] canonical preset invariant failed at index {}: slot={} order={}; "
                    "local ordering disabled for this view",
                    index, context->slot_id, context->order_id);
      log_preset_rows(phase, items, size);
      return false;
    }
  }
  return true;
}

std::vector<OfficerPresetItemContext*> make_view_order(OfficerPresetItemContext** canonical_items, int32_t size)
{
  std::vector<OfficerPresetItemContext*> view_items(canonical_items, canonical_items + size);
  remember_slots(canonical_items, static_cast<il2cpp_array_size_t>(size));

  std::vector<int32_t>                   occupied_positions;
  std::vector<OfficerPresetItemContext*> occupied_contexts;
  occupied_positions.reserve(size);
  occupied_contexts.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    if (is_reorderable_preset(canonical_items[index])) {
      occupied_positions.push_back(index);
      occupied_contexts.push_back(canonical_items[index]);
    }
  }

  std::stable_sort(occupied_contexts.begin(), occupied_contexts.end(), [](const auto* left, const auto* right) {
    const auto left_order  = std::find(session_order.begin(), session_order.end(), left->slot_id);
    const auto right_order = std::find(session_order.begin(), session_order.end(), right->slot_id);
    return left_order < right_order;
  });
  for (size_t index = 0; index < occupied_positions.size(); ++index) {
    view_items[occupied_positions[index]] = occupied_contexts[index];
  }
  return view_items;
}

bool try_get_preset_list(void* view_context, Il2CppObject** list, Il2CppArraySize** backing_items, int32_t* size)
{
  *list = read_object_field<Il2CppObject>(view_context, presets_items_offset);
  if (*list == nullptr) {
    return false;
  }

  auto list_helper = IL2CppClassHelper{(*list)->klass};
  auto items_field = list_helper.GetField("_items");
  auto size_field  = list_helper.GetField("_size");
  if (!items_field.isValidHelper() || !size_field.isValidHelper()) {
    return false;
  }

  *backing_items = read_object_field<Il2CppArraySize>(*list, items_field.offset());
  *size          = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(*list) + size_field.offset());
  return *backing_items != nullptr && *size >= 0
         && static_cast<il2cpp_array_size_t>(*size) <= (*backing_items)->max_length;
}

void log_view(const char* phase, Il2CppObject* controller, void* view_context)
{
  diagnostics().info("[OfficerPresetReorder] phase={} controller={} view={} active_controller={} enabled={} "
                     "installed={}",
                     phase, static_cast<void*>(controller), view_context, static_cast<void*>(active_controller),
                     Config::Get().allow_officer_preset_reordering, reorder_installed);
  if (view_context != nullptr) {
    auto* object = reinterpret_cast<Il2CppObject*>(view_context);
    for (const auto* name : {"SlotsCount", "OccupiedSlotsCount", "IsSaveOptionAvailable", "BelowDeckSlotsUnlocked",
                             "CurrentCachedItemContext", "CurrentCrew", "AppliedOfficersCached"}) {
      auto* field = il2cpp_class_get_field_from_name(object->klass, name);
      if (field == nullptr || field->type == nullptr || field->offset < 0) {
        diagnostics().info("[OfficerPresetReorder] phase={} view_field={} unavailable", phase, name);
        continue;
      }
      auto* address = reinterpret_cast<char*>(view_context) + field->offset;
      if (field->type->type == IL2CPP_TYPE_I4) {
        diagnostics().info("[OfficerPresetReorder] phase={} view_field={} offset=0x{:X} value={}", phase, name,
                           field->offset, *reinterpret_cast<int32_t*>(address));
      } else if (field->type->type == IL2CPP_TYPE_BOOLEAN) {
        diagnostics().info("[OfficerPresetReorder] phase={} view_field={} offset=0x{:X} value={}", phase, name,
                           field->offset, *reinterpret_cast<bool*>(address));
      } else if (field->type->type == IL2CPP_TYPE_CLASS || field->type->type == IL2CPP_TYPE_SZARRAY) {
        auto* value = *reinterpret_cast<void**>(address);
        diagnostics().info("[OfficerPresetReorder] phase={} view_field={} offset=0x{:X} pointer={} array_count={}",
                           phase, name, field->offset, value,
                           field->type->type == IL2CPP_TYPE_SZARRAY && value != nullptr
                               ? reinterpret_cast<Il2CppArraySize*>(value)->max_length
                               : 0);
      } else {
        diagnostics().warn("[OfficerPresetReorder] phase={} view_field={} unsupported_type={}", phase, name,
                           static_cast<int>(field->type->type));
      }
    }
  }
  Il2CppObject*    list          = nullptr;
  Il2CppArraySize* backing_items = nullptr;
  int32_t          size          = 0;
  const bool       available     = try_get_preset_list(view_context, &list, &backing_items, &size);
  diagnostics().info("[OfficerPresetReorder] phase={} list_available={} list={} backing={} size={} capacity={}", phase,
                     available, static_cast<void*>(list), static_cast<void*>(backing_items), size,
                     backing_items != nullptr ? backing_items->max_length : 0);
  if (available)
    log_preset_rows(phase, reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector), size);
}

bool capture_native_presentations(OfficerPresetItemContext** canonical_items, int32_t size)
{
  if (!validate_canonical_order(canonical_items, size, "capture-presentations")) {
    return false;
  }
  active_presentations.clear();
  active_presentations.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    active_presentations.push_back(canonical_items[index] != nullptr ? canonical_items[index]->presentation : 0);
  }
  return true;
}

bool restore_native_presentations(OfficerPresetItemContext** canonical_items, int32_t size)
{
  if (!validate_canonical_order(canonical_items, size, "restore-presentations")
      || active_presentations.size() != static_cast<size_t>(size)) {
    spdlog::error("[OfficerPresetReorder] native presentation map is unavailable; local ordering disabled for this "
                  "view");
    return false;
  }
  for (int32_t index = 0; index < size; ++index) {
    if (canonical_items[index] != nullptr) {
      canonical_items[index]->presentation = active_presentations[index];
    }
  }
  return true;
}

bool render_local_order(Il2CppObject* controller, bool preserve_scroll)
{
  if (controller == nullptr || item_context_class == nullptr || clear_and_generate == nullptr) {
    return false;
  }

  auto*            view_context   = read_object_field<void>(controller, controller_context_offset);
  auto*            scroller       = read_object_field<void>(controller, controller_scroller_offset);
  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  if (scroller == nullptr || !try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
    return false;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size, "render-local")
      || active_presentations.size() != static_cast<size_t>(size)) {
    diagnostics().warn("[OfficerPresetReorder] phase=render-local rejected rows={} presentation_count={}", size,
                       active_presentations.size());
    return false;
  }

  auto  view_items = make_view_order(canonical_items, size);
  auto* view_array = il2cpp_array_new(item_context_class, static_cast<il2cpp_array_size_t>(size));
  if (view_array == nullptr) {
    spdlog::warn("[OfficerPresetReorder] unable to allocate the scroller view array");
    return false;
  }
  auto** view_slots = reinterpret_cast<void**>(reinterpret_cast<Il2CppArraySize*>(view_array)->vector);
  for (int32_t index = 0; index < size; ++index) {
    auto* context = view_items[index];
    if (context != nullptr) {
      context->presentation = active_presentations[index];
    }
    il2cpp_gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(view_array), &view_slots[index], context);
  }

  const auto scroll_position = preserve_scroll && get_scroll_position != nullptr ? get_scroll_position(scroller) : 0.0f;
  log_preset_rows("render-view", view_items.data(), size);
  diagnostics().info("[OfficerPresetReorder] phase=render-local scroller={} array={} preserve_scroll={} scroll={}",
                     scroller, static_cast<void*>(view_array), preserve_scroll, scroll_position);
  clear_and_generate(scroller, controller, reinterpret_cast<Il2CppObject*>(view_array));
  if (preserve_scroll && restore_scroll_position != nullptr) {
    restore_scroll_position(scroller, scroll_position);
  }
  return true;
}

bool move_preset(OfficerPresetItemContext* context, int direction)
{
  diagnostics().info("[OfficerPresetReorder] phase=move-attempt direction={} active_controller={}", direction,
                     static_cast<void*>(active_controller));
  log_preset_row("move-attempt", 0, -1, context);
  if (!is_reorderable_preset(context) || context->officer_presets_view_context == nullptr
      || active_controller == nullptr || clear_and_generate == nullptr) {
    diagnostics().info("[OfficerPresetReorder] phase=move-attempt unavailable; preserving native rename");
    return false;
  }

  Il2CppObject*    list          = nullptr;
  Il2CppArraySize* backing_items = nullptr;
  int32_t          size          = 0;
  if (!try_get_preset_list(context->officer_presets_view_context, &list, &backing_items, &size)) {
    spdlog::warn("[OfficerPresetReorder] unable to read live preset list");
    return false;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size, "move-attempt")) {
    return false;
  }
  auto view_items = make_view_order(canonical_items, size);

  int32_t current_index = -1;
  for (int32_t index = 0; index < size; ++index) {
    if (view_items[index] == context) {
      current_index = index;
      break;
    }
  }
  if (current_index < 0) {
    return false;
  }

  int32_t target_index = current_index + direction;
  while (target_index >= 0 && target_index < size && !is_reorderable_preset(view_items[target_index])) {
    target_index += direction;
  }
  if (target_index < 0 || target_index >= size) {
    spdlog::info("[OfficerPresetReorder] slot={} is already at the {}", context->slot_id,
                 direction < 0 ? "top" : "bottom");
    return true;
  }

  auto* scroller = read_object_field<void>(active_controller, controller_scroller_offset);
  if (scroller == nullptr) {
    spdlog::warn("[OfficerPresetReorder] unable to access the active preset scroller");
    return false;
  }

  auto*      target        = view_items[target_index];
  const auto current_order = std::find(session_order.begin(), session_order.end(), context->slot_id);
  const auto target_order  = std::find(session_order.begin(), session_order.end(), target->slot_id);
  diagnostics().info("[OfficerPresetReorder] phase=move-target current_row={} target_row={} slot={} target_slot={}",
                     current_index, target_index, context->slot_id, target->slot_id);
  bool persisted = false;
  if (current_order != session_order.end() && target_order != session_order.end()) {
    std::iter_swap(current_order, target_order);
    persisted = save_session_order();
  }

  spdlog::info("[OfficerPresetReorder] moved slot={} {} across slot={} ({})", context->slot_id,
               direction < 0 ? "up" : "down", target->slot_id,
               persisted ? "persisted locally" : "local persistence failed");
  if (!render_local_order(active_controller, true)) {
    spdlog::warn("[OfficerPresetReorder] local order changed but the scroller could not be refreshed");
  }
  return true;
}

void OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(auto original, Il2CppObject* _this,
                                                          bool increase_occupied_slots_count)
{
  diagnostics().info("[OfficerPresetReorder] phase=save-enter controller={} increase_occupied_slots_count={}",
                     static_cast<void*>(_this), increase_occupied_slots_count);
  if (!reorder_installed || !Config::Get().allow_officer_preset_reordering) {
    original(_this, increase_occupied_slots_count);
    return;
  }
  auto* view_context = read_object_field<void>(_this, controller_context_offset);
  auto* scroller     = read_object_field<void>(_this, controller_scroller_offset);
  log_view("save-before-native", _this, view_context);

  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  const bool       canonical_available =
      scroller != nullptr && try_get_preset_list(view_context, &canonical_list, &backing_items, &size)
      && validate_canonical_order(reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector), size,
                                  "save-before-native");
  const bool can_rerender =
      canonical_available && active_controller == _this
      && restore_native_presentations(reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector), size);
  const auto scroll_position = can_rerender && get_scroll_position != nullptr ? get_scroll_position(scroller) : 0.0f;
  if (canonical_available) {
    // Scopely's callback indexes SmartScroller._data by OrderId. Give it the canonical list for the duration of the
    // callback, then rebuild our separate presentation-only view.
    clear_and_generate(scroller, _this, canonical_list);
  }

  original(_this, increase_occupied_slots_count);
  log_view("save-after-native", _this, read_object_field<void>(_this, controller_context_offset));
  diagnostics().info("[OfficerPresetReorder] phase=save-after-native canonical_available={} can_rerender={}",
                     canonical_available, can_rerender);

  if (!canonical_available) {
    spdlog::warn("[OfficerPresetReorder] save succeeded without an available canonical scroller source");
    return;
  }
  if (!can_rerender) {
    spdlog::debug("[OfficerPresetReorder] save succeeded in canonical order without an active local view");
    return;
  }

  if (!try_get_preset_list(read_object_field<void>(_this, controller_context_offset), &canonical_list, &backing_items,
                           &size)) {
    spdlog::warn("[OfficerPresetReorder] save succeeded but the canonical preset list became unavailable");
    return;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size, "save-after-native")) {
    spdlog::error("[OfficerPresetReorder] canonical preset invariant failed after save; local ordering was not "
                  "reapplied");
    return;
  }
  if (active_presentations.size() != static_cast<size_t>(size)) {
    if (!capture_native_presentations(canonical_items, size)) {
      return;
    }
  }
  if (!render_local_order(_this, false)) {
    spdlog::warn("[OfficerPresetReorder] save reconciled safely but the local scroller order could not be restored");
    return;
  }
  if (restore_scroll_position != nullptr) {
    restore_scroll_position(scroller, scroll_position);
  }
  spdlog::info("[OfficerPresetReorder] reconciled presentation-only ordering after preset save");
}

bool OfficerManager_TryGetPresetItemContext_Hook(auto original, void* _this, Il2CppArraySize** preset_contexts,
                                                 void* view_context)
{
  diagnostics().info("[OfficerPresetReorder] phase=manager-enter manager={} view={} output_address={} enabled={} "
                     "installed={}",
                     _this, view_context, static_cast<void*>(preset_contexts),
                     Config::Get().allow_officer_preset_reordering, reorder_installed);
  const bool result = original(_this, preset_contexts, view_context);
  diagnostics().info("[OfficerPresetReorder] phase=manager-return result={} array={}", result,
                     preset_contexts != nullptr ? static_cast<void*>(*preset_contexts) : nullptr);
  if (!reorder_installed || !Config::Get().allow_officer_preset_reordering)
    return result;
  if (!result || preset_contexts == nullptr || *preset_contexts == nullptr) {
    spdlog::info("[OfficerPresetReorder] preset context load returned result={} array={}", result,
                 preset_contexts != nullptr ? static_cast<void*>(*preset_contexts) : nullptr);
    return result;
  }

  auto*  contexts = *preset_contexts;
  auto** items    = reinterpret_cast<OfficerPresetItemContext**>(contexts->vector);
  if (contexts->max_length > static_cast<il2cpp_array_size_t>(std::numeric_limits<int32_t>::max())) {
    diagnostics().error("[OfficerPresetReorder] phase=manager-return oversized array length={}; native rows untouched",
                        contexts->max_length);
    return result;
  }
  log_preset_rows("manager-native-rows", items, static_cast<int32_t>(contexts->max_length));
  log_view("manager-return-view", nullptr, view_context);
  if (validate_canonical_order(items, static_cast<int32_t>(contexts->max_length), "manager-native-identity")) {
    remember_slots(items, contexts->max_length);
    log_session_order("manager-remembered");
    spdlog::debug("[OfficerPresetReorder] observed {} canonical preset rows", contexts->max_length);
  } else {
    diagnostics().error("[OfficerPresetReorder] native context builder returned non-canonical preset rows; "
                        "leaving them untouched");
  }

  return result;
}

void OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(auto original, void* _this)
{
  if (!reorder_installed || !Config::Get().allow_officer_preset_reordering) {
    original(_this);
    return;
  }
  const bool move_up   = shift_pressed();
  const bool move_down = move_down_modifier_pressed();
  diagnostics().info("[OfficerPresetReorder] phase=edit-click widget={} move_up={} move_down={}", _this, move_up,
                     move_down);
  if (move_up != move_down) {
    auto* context = read_object_field<OfficerPresetItemContext>(_this, widget_context_offset);
    if (move_preset(context, move_up ? -1 : 1)) {
      return;
    }
  }
  diagnostics().info("[OfficerPresetReorder] phase=edit-click invoking native rename");
  original(_this);
}

void OfficerPresetsViewController_OnDidBindCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  diagnostics().info("[OfficerPresetReorder] phase=bind-enter controller={}", static_cast<void*>(_this));
  if (!reorder_installed || !Config::Get().allow_officer_preset_reordering) {
    original(_this);
    return;
  }
  original(_this);
  active_controller = _this;

  auto* view_context = read_object_field<void>(_this, controller_context_offset);
  log_view("bind-after-native", _this, view_context);
  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  if (!try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
    active_controller = nullptr;
    active_presentations.clear();
    spdlog::warn("[OfficerPresetReorder] unable to capture the bound canonical preset list");
    return;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!capture_native_presentations(canonical_items, size) || !render_local_order(_this, false)) {
    active_controller = nullptr;
    active_presentations.clear();
    spdlog::warn("[OfficerPresetReorder] local ordering was disabled for the bound preset view");
    diagnostics().warn("[OfficerPresetReorder] phase=bind-result local_ordering=false; native rows retained");
  } else {
    diagnostics().info("[OfficerPresetReorder] phase=bind-result local_ordering=true");
  }
}

void OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  diagnostics().info("[OfficerPresetReorder] phase=release-enter controller={} active_controller={}",
                     static_cast<void*>(_this), static_cast<void*>(active_controller));
  if (active_controller == _this) {
    auto*            view_context   = read_object_field<void>(_this, controller_context_offset);
    auto*            scroller       = read_object_field<void>(_this, controller_scroller_offset);
    Il2CppObject*    canonical_list = nullptr;
    Il2CppArraySize* backing_items  = nullptr;
    int32_t          size           = 0;
    if (scroller != nullptr && try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
      auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
      log_view("release-native-list", _this, view_context);
      if (validate_canonical_order(canonical_items, size, "release-native-list")) {
        restore_native_presentations(canonical_items, size);
        clear_and_generate(scroller, _this, canonical_list);
      }
    }
    active_controller = nullptr;
    active_presentations.clear();
  }
  original(_this);
}
} // namespace

bool mod_settings::OfficerPresetReorderAvailable()
{ return reorder_installed; }

void InstallOfficerPresetReorderHooks()
{
#if __APPLE__
  constexpr const char* platform = "macOS";
#elif _WIN32
  constexpr const char* platform = "Windows";
#else
  constexpr const char* platform = "other";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
  constexpr const char* architecture = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  constexpr const char* architecture = "x64";
#else
  constexpr const char* architecture = "other";
#endif
  diagnostics().info("[OfficerPresetReorder] diagnostic_capture=1 platform={} architecture={} pointer_bytes={} "
                     "context_bytes={} version={} commit={} identity={} enabled={}",
                     platform, architecture, sizeof(void*), sizeof(OfficerPresetItemContext), VER_FILE_VERSION_STR,
                     VERSION_COMMIT_HASH, STFC_IDENTITY_COMMENT_STR, Config::Get().allow_officer_preset_reordering);
  auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerManager");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Officers", "OfficerManager");
    return;
  }

  const auto method = helper.GetMethodInfo("TryGetPresetItemContext", 2);
  if (method == nullptr || method->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("OfficerManager", "TryGetPresetItemContext");
    return;
  }

  auto widget_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemWidget");
  auto controller_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewController");
  auto view_context_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewContext");
  auto item_context_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemContext");
  auto scroller_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase");
  for (const auto& [valid, name] : std::array{
           std::pair{widget_helper.isValidHelper(), "Digit.Prime.OfficerPresets.OfficerPresetItemWidget"},
           std::pair{controller_helper.isValidHelper(), "Digit.Prime.OfficerPresets.OfficerPresetsViewController"},
           std::pair{view_context_helper.isValidHelper(), "Digit.Prime.OfficerPresets.OfficerPresetsViewContext"},
           std::pair{item_context_helper.isValidHelper(), "Digit.Prime.OfficerPresets.OfficerPresetItemContext"},
           std::pair{scroller_helper.isValidHelper(), "Digit.Client.UI.SmartScrollerBase"}}) {
    if (!valid) {
      spdlog::warn("[OfficerPresetReorder] unavailable: missing class {}", name);
      return;
    }
  }
  if (!validate_context_layout(item_context_helper.get_cls())) {
    return;
  }
  item_context_class            = item_context_helper.get_cls();
  diagnostic_extra_layout_valid = true;
  for (const auto& [name, offset, type] : std::array{
           std::tuple{"SyntheticFleetPlayerData", offsetof(OfficerPresetItemContext, synthetic_fleet_player_data),
                      IL2CPP_TYPE_CLASS},
           std::tuple{"IsSaveAvailable", offsetof(OfficerPresetItemContext, is_save_available), IL2CPP_TYPE_BOOLEAN},
           std::tuple{"BelowDeckOfficers", offsetof(OfficerPresetItemContext, below_deck_officers),
                      IL2CPP_TYPE_SZARRAY},
           std::tuple{"BelowDeckSlotsUnlocked", offsetof(OfficerPresetItemContext, below_deck_slots_unlocked),
                      IL2CPP_TYPE_BOOLEAN}}) {
    auto*      field = il2cpp_class_get_field_from_name(item_context_class, name);
    const bool valid =
        field != nullptr && field->type != nullptr && field->offset == offset && field->type->type == type;
    diagnostics().info("[OfficerPresetReorder] phase=diagnostic-layout field={} available={} offset={} type={} "
                       "expected_offset={} expected_type={} valid={}",
                       name, field != nullptr, field != nullptr ? field->offset : -1,
                       field != nullptr && field->type != nullptr ? static_cast<int>(field->type->type) : -1, offset,
                       static_cast<int>(type), valid);
    diagnostic_extra_layout_valid = diagnostic_extra_layout_valid && valid;
  }

  auto context_field            = widget_helper.GetField("m_context");
  auto controller_context_field = controller_helper.GetField("m_context");
  auto scroller_field           = controller_helper.GetField("_smartScroller");
  auto presets_field            = view_context_helper.GetField("PresetsItemsContext");
  for (const auto& [valid, name] :
       std::array{std::pair{context_field.isValidHelper(), "OfficerPresetItemWidget.m_context"},
                  std::pair{controller_context_field.isValidHelper(), "OfficerPresetsViewController.m_context"},
                  std::pair{scroller_field.isValidHelper(), "OfficerPresetsViewController._smartScroller"},
                  std::pair{presets_field.isValidHelper(), "OfficerPresetsViewContext.PresetsItemsContext"}}) {
    if (!valid) {
      spdlog::warn("[OfficerPresetReorder] unavailable: missing field {}", name);
      return;
    }
  }
  widget_context_offset      = context_field.offset();
  controller_context_offset  = controller_context_field.offset();
  controller_scroller_offset = scroller_field.offset();
  presets_items_offset       = presets_field.offset();
  diagnostics().info("[OfficerPresetReorder] phase=bindings widget_context=0x{:X} controller_context=0x{:X} "
                     "scroller=0x{:X} presets_items=0x{:X}",
                     widget_context_offset, controller_context_offset, controller_scroller_offset,
                     presets_items_offset);

  const auto clear_method          = scroller_helper.GetMethodInfo("ClearAndGenerateContents", 2);
  const auto get_scroll_method     = scroller_helper.GetMethodInfo("get_ScrollPosition", 0);
  const auto restore_scroll_method = scroller_helper.GetMethodInfo("RestoreScrollPosition", 1);
  const auto edit_method           = widget_helper.GetMethodInfo("OnEditNameButtonClicked", 0);
  const auto bind_method           = controller_helper.GetMethodInfo("OnDidBindCanvasContext", 0);
  const auto release_method        = controller_helper.GetMethodInfo("OnAboutToReleaseCanvasContext", 0);
  const auto save_success_method   = controller_helper.GetMethodInfo("OnSaveSlotsSuccess", 1);
  for (const auto& [target, name] :
       std::array{std::pair{clear_method, "SmartScrollerBase.ClearAndGenerateContents(2 arguments)"},
                  std::pair{get_scroll_method, "SmartScrollerBase.get_ScrollPosition()"},
                  std::pair{restore_scroll_method, "SmartScrollerBase.RestoreScrollPosition(1 argument)"},
                  std::pair{edit_method, "OfficerPresetItemWidget.OnEditNameButtonClicked()"},
                  std::pair{bind_method, "OfficerPresetsViewController.OnDidBindCanvasContext()"},
                  std::pair{release_method, "OfficerPresetsViewController.OnAboutToReleaseCanvasContext()"},
                  std::pair{save_success_method, "OfficerPresetsViewController.OnSaveSlotsSuccess(1 argument)"}}) {
    if (!target || !target->methodPointer) {
      spdlog::warn("[OfficerPresetReorder] unavailable: missing method/native pointer {}", name);
      return;
    }
  }
  clear_and_generate      = reinterpret_cast<ClearAndGenerateContentsFn*>(clear_method->methodPointer);
  get_scroll_position     = reinterpret_cast<GetScrollPositionFn*>(get_scroll_method->methodPointer);
  restore_scroll_position = reinterpret_cast<RestoreScrollPositionFn*>(restore_scroll_method->methodPointer);

  load_session_order();
  diagnostics().info(
      "[OfficerPresetReorder] phase=hook-targets construct={} edit={} bind={} release={} save={}",
      reinterpret_cast<void*>(method->methodPointer), reinterpret_cast<void*>(edit_method->methodPointer),
      reinterpret_cast<void*>(bind_method->methodPointer), reinterpret_cast<void*>(release_method->methodPointer),
      reinterpret_cast<void*>(save_success_method->methodPointer));

  const bool context = SPUD_STATIC_DETOUR(method->methodPointer, OfficerManager_TryGetPresetItemContext_Hook);
  const bool edit =
      SPUD_STATIC_DETOUR(edit_method->methodPointer, OfficerPresetItemWidget_OnEditNameButtonClicked_Hook);
  const bool bind =
      SPUD_STATIC_DETOUR(bind_method->methodPointer, OfficerPresetsViewController_OnDidBindCanvasContext_Hook);
  const bool release = SPUD_STATIC_DETOUR(release_method->methodPointer,
                                          OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook);
  const bool save =
      SPUD_STATIC_DETOUR(save_success_method->methodPointer, OfficerPresetsViewController_OnSaveSlotsSuccess_Hook);
  const std::array installed{context, edit, bind, release, save};
  const std::array names{"OfficerManager.TryGetPresetItemContext", "OfficerPresetItemWidget.OnEditNameButtonClicked",
                         "OfficerPresetsViewController.OnDidBindCanvasContext",
                         "OfficerPresetsViewController.OnAboutToReleaseCanvasContext",
                         "OfficerPresetsViewController.OnSaveSlotsSuccess"};
  for (std::size_t i = 0; i < installed.size(); ++i)
    if (!installed[i])
      spdlog::warn("[OfficerPresetReorder] failed to install {}; feature retains native behavior", names[i]);
  reorder_installed = context && edit && bind && release && save;
  diagnostics().info("[OfficerPresetReorder] phase=installed construct={} edit={} bind={} release={} save={} ready={} "
                     "extra_layout={}",
                     context, edit, bind, release, save, reorder_installed, diagnostic_extra_layout_valid);
}
