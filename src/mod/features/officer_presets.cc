// [ui] allow_officer_preset_reordering: local display order only. Native
// SlotId/OrderId, crew saves and rename saves remain owned by the game.
#include "config.h"
#include "features/features.h"
#include "mod_state.h"
#include "officer_preset_handoff.h"
#include "officer_preset_order.h"

#include "game/officer_presets.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>

#include <charconv>
#include <map>
#include <memory>

namespace
{
namespace preset_boundary = officer_preset_handoff;
namespace preset_order    = officer_preset_order;
preset_game::Fields       preset_fields;
std::vector<std::int64_t> preset_session_order;
std::uint64_t             preset_generation = 0, preset_save_revision = 0;
bool                      preset_save_pending = false;

bool PresetEnabled()
{ return Config::Get().ui.allow_officer_preset_reordering; }

void LoadPresetOrder()
{
  const auto state = mod_state::Read();
  if (!state)
    return;
  const auto found = state->find("officer_preset_order");
  if (found == state->end() || !found->is_array())
    return;
  std::vector<std::int64_t> loaded;
  for (const auto& value : *found) {
    std::int64_t id = -1;
    if (value.is_string()) {
      const auto& text   = value.get_ref<const std::string&>();
      const auto  parsed = std::from_chars(text.data(), text.data() + text.size(), id);
      if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        continue;
    } else if (value.is_number_unsigned()) {
      const auto number = value.get<std::uint64_t>();
      if (number > static_cast<std::uint64_t>(INT64_MAX))
        continue;
      id = static_cast<std::int64_t>(number);
    } else if (value.is_number_integer()) {
      id = value.get<std::int64_t>();
    }
    if (id >= 0 && std::ranges::find(loaded, id) == loaded.end())
      loaded.push_back(id);
  }
  preset_session_order = std::move(loaded);
}

// The controller's canonical list roots its current rows. Display sources and
// their old rows remain rooted across rebuilds, nested saves and replacement.
struct PresetView {
  il2cpp::GcHandle                                   controller, context, scroller, canonical, display, candidate;
  preset_game::ListAccess                            list;
  preset_boundary::Boundary                          boundary;
  preset_boundary::Identity                          identity;
  std::vector<OfficerPresetItemContext*>             tokens;
  std::vector<std::int32_t>                          presentations;
  std::vector<std::int64_t>                          slots;
  std::vector<bool>                                  empty;
  std::vector<std::size_t>                           native_mapping;
  std::shared_ptr<const preset_boundary::Projection> projected;
  bool                                               detached = false, busy = false, repair_pending = false;

  preset_boundary::Identity Observe() const noexcept
  {
    if (detached || preset_fields.controller_context.Read(controller.Get()) != context.Get()
        || preset_fields.scroller.Read(controller.Get()) != scroller.Get()
        || preset_fields.presets.Read(context.Get()) != canonical.Get())
      return {};
    return identity;
  }
  const void* Source() const noexcept
  { return preset_fields.source.Read(scroller.Get()); }
  il2cpp::Array<OfficerPresetItemContext*>* Items() const noexcept
  {
    std::size_t count = 0;
    auto*       array = list.Read(canonical.Get<il2cpp::Object>(), count);
    return array && count == tokens.size() ? array : nullptr;
  }
  bool ItemValid(OfficerPresetItemContext* item, std::size_t index) const noexcept
  {
    return !item
           || (preset_fields.item_owner.Read(item) == context.Get()
               && preset_fields.order.Read(item) == static_cast<std::int32_t>(index)
               && preset_fields.slot.Read(item) >= 0
               && (empty[index] || preset_fields.slot.Read(item) == slots[index]));
  }
  bool RestoreNative() noexcept
  {
    auto* items = Items();
    if (!items)
      return false;
    bool ok = true;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
      auto* item = (*items)[index];
      if (!ItemValid(item, index))
        return false;
      if (!item)
        continue;
      if (tokens[index] != item) {
        // A native save can replace a row with the same canonical OrderId.
        presentations[index] = preset_fields.presentation.Read(item);
        tokens[index]        = item;
      }
      ok = preset_fields.presentation.Write(item, presentations[index]) && ok;
    }
    // Native save generates its replacement using the cached item's
    // Presentation. That cache can still name the previous row object.
    if (auto* cached = preset_fields.cached_context.Read(context.Get())) {
      const auto index = preset_fields.order.Read(cached);
      if (index < 0 || static_cast<std::size_t>(index) >= tokens.size() || !ItemValid(cached, index))
        return false;
      ok = preset_fields.presentation.Write(cached, presentations[index]) && ok;
    }
    return ok;
  }
  bool NativePresentations(const preset_boundary::SaveScope& scope) noexcept
  { return scope.Current(boundary, Observe()) && RestoreNative(); }
  bool RestoreSource(const void* source) noexcept
  { return preset_fields.source.Write(scroller.Get(), reinterpret_cast<il2cpp::Object*>(const_cast<void*>(source))); }
  bool Canonical(const preset_boundary::SaveScope& scope) noexcept
  {
    if (!NativePresentations(scope))
      boundary.RequestCanonical(); // Don't certify display restoration after an unqualified annotation.
    // Prepared field metadata, a strong receiver root and a synchronous runtime
    // field write: no managed code, rebuild, allocation or index-map mutation.
    return RestoreSource(canonical.Get());
  }
  [[noreturn]] void Unsupported()
  {
    spdlog::critical("[OfficerPresetReorder] canonical field invariant failed; refusing a misdirected native save");
    std::terminate();
  }
  bool SyncArray(const preset_boundary::Projection& projection, std::int32_t canonical_index,
                 std::int32_t view) noexcept
  {
    if (canonical_index < 0 || view < 0 || static_cast<std::size_t>(canonical_index) >= tokens.size()
        || static_cast<std::size_t>(view) >= tokens.size())
      return false;
    auto* items = Items();
    auto* array = reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(const_cast<void*>(projection.source));
    if (!items || !array || array->size() != tokens.size())
      return false;
    auto* item = (*items)[static_cast<std::size_t>(canonical_index)];
    if (!ItemValid(item, static_cast<std::size_t>(canonical_index)))
      return false;
    if (item && tokens[canonical_index] != item) {
      presentations[canonical_index] = preset_fields.presentation.Read(item);
      tokens[canonical_index]        = item;
    }
    array->Set(static_cast<std::size_t>(view), item);
    return (*array)[view] == item && (!item || preset_fields.presentation.Write(item, presentations[view]));
  }
  bool Sync(const preset_boundary::SaveScope& scope, std::int32_t canonical_index, std::int32_t view) noexcept
  { return scope.Current(boundary, Observe()) && scope.display && SyncArray(*scope.display, canonical_index, view); }
  bool SyncAll(const preset_boundary::SaveScope& scope) noexcept
  {
    if (!scope.Current(boundary, Observe()))
      return false;
    bool ok = true;
    for (const auto& projection : {scope.display, scope.candidate})
      if (projection)
        for (std::size_t view = 0; view < projection->view_to_canonical.size(); ++view)
          ok = SyncArray(*projection, static_cast<std::int32_t>(projection->view_to_canonical[view]),
                         static_cast<std::int32_t>(view))
               && ok;
    return ok;
  }
  bool Matches(const preset_boundary::SaveScope& scope, il2cpp::Object* receiver,
               std::int32_t canonical_index) const noexcept
  {
    auto* widget = receiver ? receiver->As<OfficerPresetItemWidget>() : nullptr;
    auto* item   = widget ? preset_fields.widget_context.Read(widget) : nullptr;
    // The receiver still has the old context until the game's original binds
    // its replacement. OrderId and owner identify the logical canonical slot;
    // an empty slot is permitted to acquire its native assigned SlotId.
    return scope.Current(boundary, Observe()) && item && canonical_index >= 0
           && static_cast<std::size_t>(canonical_index) < slots.size()
           && preset_fields.order.Read(item) == canonical_index && preset_fields.item_owner.Read(item) == context.Get()
           && (empty[canonical_index] || preset_fields.slot.Read(item) == slots[canonical_index]);
  }
};

std::shared_ptr<PresetView>                                          active_preset_view;
std::map<OfficerPresetsViewController*, std::shared_ptr<PresetView>> preset_views;
struct PresetSaving {
  std::shared_ptr<PresetView> view;
  PresetSaving*               previous;
};
PresetSaving* preset_saving = nullptr; // Game-thread stack; never a worker-owned pointer.

bool ReadPresetRows(PresetView& view, std::vector<preset_order::Row>& rows)
{
  std::size_t count = 0;
  auto*       items = view.list.Read(view.canonical.Get<il2cpp::Object>(), count);
  if (!items)
    return false;
  rows.clear();
  rows.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    auto*             item = (*items)[index];
    preset_order::Row row{.qualified = true};
    if (item) {
      if (preset_fields.item_owner.Read(item) != view.context.Get())
        return false;
      const auto crew = il2cpp::TryReferenceArray<Il2CppObject*>(preset_fields.officers.Read(item));
      if (!crew.succeeded)
        return false;
      auto* name = preset_fields.name.Read(item);
      row = {.qualified        = true,
             .present          = true,
             .slot             = preset_fields.slot.Read(item),
             .order            = preset_fields.order.Read(item),
             .presentation     = preset_fields.presentation.Read(item),
             .name_present     = name != nullptr,
             .name_empty       = name && name->length == 0,
             .officers_present = crew.value != nullptr,
             .assigned =
                 crew.value && std::ranges::any_of(*crew.value, [](auto* officer) { return officer != nullptr; })};
    }
    rows.push_back(row);
  }
  return true;
}

// Rebuild readiness is observed independently of invocation success. A pending
// coroutine cannot certify a renderer map merely because it adopted _data.
bool PresetRendererReady(PresetView& view, const void* source, std::span<const std::size_t> mapping)
{
  if (!view.boundary.Current(view.Observe()) || view.Source() != source
      || preset_fields.regenerating.Read(view.scroller.Get()) || preset_fields.initializing.Read(view.scroller.Get()))
    return false;
  auto* items = view.Items();
  if (!items || mapping.size() != view.tokens.size())
    return false;
  auto* scroller = view.scroller.Get<PresetSmartScroller>();
  for (std::size_t index = 0; index < mapping.size(); ++index) {
    il2cpp::Object* receiver = nullptr;
    const auto      result   = scroller->TryCall<"TryGetInstanceForIndex", bool(std::int32_t, il2cpp::Object*&)>(
        static_cast<std::int32_t>(index), receiver);
    if (!result.succeeded)
      return false;
    if (!result.value)
      continue; // An off-screen/virtualized row legitimately has no receiver.
    auto* widget = receiver ? receiver->As<OfficerPresetItemWidget>() : nullptr;
    if (!widget || preset_fields.widget_context.Read(widget) != (*items)[mapping[index]])
      return false;
  }
  return view.boundary.Current(view.Observe()) && view.Source() == source;
}

void RepairPresetView(const std::shared_ptr<PresetView>& view)
{
  if (!view || view->busy || !view->boundary.Current(view->Observe()))
    return;
  view->busy = true;
  struct Exit {
    PresetView& view;
    ~Exit()
    { view.busy = false; }
  } exit{*view};
  view->boundary.RequestCanonical();
  (void)view->RestoreNative();
  if (!view->RestoreSource(view->canonical.Get()))
    view->Unsupported();
  (void)view->scroller.Get<PresetSmartScroller>()
      ->TryCall<"ClearAndGenerateContents", void(il2cpp::Object*, il2cpp::Object*)>(
          view->controller.Get<il2cpp::Object>(), view->canonical.Get<il2cpp::Object>());
  if (!view->boundary.Current(view->Observe()))
    return;
  const bool ready     = PresetRendererReady(*view, view->canonical.Get(), view->native_mapping);
  view->repair_pending = !view->boundary.ConfirmCanonical(view->Observe(), view->Source(), ready);
  if (!view->repair_pending) {
    view->display.Reset();
    view->candidate.Reset();
    view->projected.reset();
  }
}

bool RenderPresetOrder(const std::shared_ptr<PresetView>& view, bool preserve_scroll)
{
  if (!view || view->busy || !view->boundary.Current(view->Observe()))
    return false;
  std::vector<preset_order::Row> rows;
  if (!ReadPresetRows(*view, rows))
    return false;
  const auto plan = preset_order::Prepare(rows, preset_session_order);
  if (plan.status != preset_order::Status::Ready || rows.size() != view->tokens.size())
    return false;
  auto root = il2cpp::GcHandle::Strong(
      il2cpp::Array<OfficerPresetItemContext*>::New(OfficerPresetItemContext::Class::Raw(), rows.size()));
  if (!root)
    return false;
  auto* array = reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(root.Get());
  auto  projection =
      std::make_shared<preset_boundary::Projection>(preset_boundary::Projection{array, plan.view_to_canonical});
  auto* items = view->Items();
  if (!items || !view->boundary.Current(view->Observe()))
    return false;
  const auto ticket = view->boundary.BeginTransition(projection);
  if (!ticket)
    return false;
  view->candidate = std::move(root);
  view->busy      = true;
  struct BusyExit {
    PresetView& view;
    ~BusyExit()
    {
      view.busy = false;
      if (view.boundary.State() == preset_boundary::Phase::Transition) {
        view.boundary.RequestCanonical();
        view.repair_pending = false; // Request repair, not an already-started rebuild.
      }
    }
  } busy_exit{*view};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto* item = (*items)[plan.view_to_canonical[index]];
    array->Set(index, item);
    if (item)
      (void)preset_fields.presentation.Write(item, view->presentations[index]);
  }
  auto*      scroller = view->scroller.Get<PresetSmartScroller>();
  const auto scroll   = scroller->TryCall<"get_ScrollPosition", float()>();
  (void)scroller->TryCall<"ClearAndGenerateContents", void(il2cpp::Object*, il2cpp::Object*)>(
      view->controller.Get<il2cpp::Object>(), reinterpret_cast<il2cpp::Object*>(array));
  const bool ready   = PresetRendererReady(*view, array, projection->view_to_canonical);
  const bool adopted = view->boundary.FinishTransition(ticket, view->Observe(), view->Source(), ready);
  view->busy         = false;
  if (!adopted) {
    RepairPresetView(view);
    return false;
  }
  view->display   = std::move(view->candidate);
  view->projected = std::move(projection);
  if (preserve_scroll && scroll.succeeded)
    (void)scroller->TryCall<"RestoreScrollPosition", void(float)>(scroll.value);
  return true;
}

std::shared_ptr<PresetView> AcquirePresetView(OfficerPresetsViewController* controller)
{
  auto view        = std::make_shared<PresetView>();
  view->controller = il2cpp::GcHandle::Strong(controller);
  if (!view->controller)
    return {};
  view->context  = il2cpp::GcHandle::Strong(preset_fields.controller_context.Read(controller));
  view->scroller = il2cpp::GcHandle::Strong(preset_fields.scroller.Read(controller));
  if (!view->context || !view->scroller)
    return {};
  view->canonical = il2cpp::GcHandle::Strong(preset_fields.presets.Read(view->context.Get()));
  if (!view->canonical || !view->list.Resolve(view->canonical.Get<il2cpp::Object>()))
    return {};
  view->identity = {++preset_generation, controller, view->context.Get(), view->scroller.Get(), view->canonical.Get()};
  view->boundary.Bind(view->identity);
  std::vector<preset_order::Row> rows;
  if (!ReadPresetRows(*view, rows))
    return {};
  const auto plan = preset_order::Prepare(rows, preset_session_order);
  if (plan.status != preset_order::Status::Ready)
    return {};
  std::size_t count = 0;
  auto*       items = view->list.Read(view->canonical.Get<il2cpp::Object>(), count);
  if (!items || count != rows.size())
    return {};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    view->tokens.push_back((*items)[index]);
    view->presentations.push_back(rows[index].presentation);
    view->slots.push_back(rows[index].slot);
    view->empty.push_back(rows[index].Empty());
    view->native_mapping.push_back(index);
  }
  preset_session_order = plan.session_order;
  return view;
}

void RetirePresetView(const std::shared_ptr<PresetView>& view)
{
  if (!view)
    return;
  // Keep guard/roots alive until the enclosing native release/rebind returns.
  view->boundary.BeginRelease();
  (void)view->RestoreNative();
  if (view->boundary.OwnsSource(view->Source()) && !view->RestoreSource(view->canonical.Get()))
    view->Unsupported();
}

void OfficerPresetsViewController_OnDidBindCanvasContext_Hook(auto original, OfficerPresetsViewController* self)
{
  const auto binding_ticket = ++preset_generation;
  const auto found          = preset_views.find(self);
  auto       old            = found != preset_views.end() ? found->second : nullptr;
  if (old)
    RetirePresetView(old);
  original(self);
  // A reentrant bind owns the newer projection; never replace it on return.
  if (binding_ticket != preset_generation)
    return;
  if (old) {
    old->detached = true;
    old->boundary.Detach();
    preset_views.erase(self);
    if (active_preset_view == old)
      active_preset_view.reset();
  }
  if (!PresetEnabled())
    return;
  auto view = AcquirePresetView(self);
  if (!view)
    return;
  preset_views.insert_or_assign(self, view);
  active_preset_view = view;
  if (!RenderPresetOrder(view, false))
    spdlog::warn("[OfficerPresetReorder] bound native view; local projection not ready");
}

void OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(auto original, OfficerPresetsViewController* self)
{
  const auto found = preset_views.find(self);
  auto       view  = found != preset_views.end() ? found->second : nullptr;
  if (!view) {
    original(self);
    return;
  }
  RetirePresetView(view);
  original(self);
  view->detached = true;
  view->boundary.Detach();
  const auto current = preset_views.find(self);
  if (current != preset_views.end() && current->second == view)
    preset_views.erase(current);
  if (active_preset_view == view)
    active_preset_view.reset();
}

void OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(auto original, OfficerPresetsViewController* self,
                                                          bool increase)
{
  const auto found = preset_views.find(self);
  auto       view  = found != preset_views.end() ? found->second : nullptr;
  if (!view) {
    original(self, increase);
    return;
  }
  PresetSaving save{view, preset_saving};
  preset_saving = &save;
  struct Exit {
    PresetSaving* previous;
    ~Exit()
    { preset_saving = previous; }
  } exit{save.previous};
  preset_boundary::SaveOnce(view->boundary, *view, [&] { original(self, increase); });
  // Frame observes Restore and repairs outside this save/lookup stack. No second save call.
}

bool PresetSmartScroller_TryGetInstanceForIndex_Hook(auto original, PresetSmartScroller* self, std::int32_t index,
                                                     il2cpp::Object** receiver)
{
  for (auto* save = preset_saving; save; save = save->previous) {
    if (save->view->scroller.Get() != self)
      continue;
    il2cpp::Object* result = nullptr;
    const bool      found  = preset_boundary::LookupOnce(
        save->view->boundary, *save->view, self, index, result,
        [&](std::int32_t view, il2cpp::Object*& value) { return original(self, view, &value); });
    if (receiver)
      *receiver = result;
    return found;
  }
  return original(self, index, receiver);
}

void OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(auto original, OfficerPresetItemWidget* self)
{
  if (!PresetEnabled()) {
    original(self);
    return;
  }
  const bool up = unity::Input::GetKey(unity::KeyCode::LeftShift) || unity::Input::GetKey(unity::KeyCode::RightShift);
#if __APPLE__
  const bool down =
      unity::Input::GetKey(unity::KeyCode::LeftCommand) || unity::Input::GetKey(unity::KeyCode::RightCommand);
#else
  const bool down =
      unity::Input::GetKey(unity::KeyCode::LeftControl) || unity::Input::GetKey(unity::KeyCode::RightControl);
#endif
  if (up == down) {
    original(self);
    return;
  }
  // This recognized chord is a reorder request even at a boundary or if the
  // adapter declines. Never accidentally open the native rename dialog.
  auto  view     = active_preset_view;
  auto* selected = preset_fields.widget_context.Read(self);
  if (!view || !selected || view->busy || !view->boundary.Current(view->Observe())
      || preset_fields.item_owner.Read(selected) != view->context.Get())
    return;
  std::vector<preset_order::Row> rows;
  if (!ReadPresetRows(*view, rows))
    return;
  auto moved = preset_order::Move(rows, preset_session_order, preset_fields.slot.Read(selected), up ? -1 : 1);
  if (moved.status != preset_order::MoveStatus::Changed)
    return;
  preset_session_order = std::move(moved.session_order);
  preset_save_revision = mod_state::QueueOfficerOrder(preset_session_order);
  preset_save_pending  = preset_save_revision != 0;
  spdlog::info("[OfficerPresetReorder] local reorder accepted; {}",
               preset_save_pending ? "disk save pending" : "not queued for disk; session only");
  if (!RenderPresetOrder(view, true))
    spdlog::warn("[OfficerPresetReorder] local order accepted but display rebuild not ready");
}

void PresetFrame()
{
  if (preset_save_pending) {
    const auto status = mod_state::Status(mod_state::Group::OfficerOrder);
    if (status.revision >= preset_save_revision && status.outcome != mod_state::SaveOutcome::Pending) {
      const bool saved    = status.revision == preset_save_revision && status.outcome == mod_state::SaveOutcome::Saved;
      preset_save_pending = false;
      if (saved)
        spdlog::info("[OfficerPresetReorder] local order persisted (revision {})", preset_save_revision);
      else
        spdlog::warn("[OfficerPresetReorder] local order not persisted; current session order retained");
    }
  }
  auto view = active_preset_view;
  if (!view || view->busy || !view->boundary.Current(view->Observe()))
    return;
  if (view->boundary.State() == preset_boundary::Phase::Restore && !view->repair_pending) {
    RepairPresetView(view);
    return;
  }
  if (!PresetEnabled() && view->boundary.State() == preset_boundary::Phase::Projected)
    RepairPresetView(view);
  else if (view->repair_pending) {
    // Readiness may complete on a later frame; don't regenerate every frame.
    const bool ready = PresetRendererReady(*view, view->canonical.Get(), view->native_mapping);
    if (view->boundary.ConfirmCanonical(view->Observe(), view->Source(), ready)) {
      view->repair_pending = false;
      view->candidate.Reset();
      view->display.Reset();
      view->projected.reset();
    }
  }
}

bool InstallOfficerPresetReorder()
{
  if (!preset_fields.Resolve())
    return false;
  // Preflight all native calls used after projection before installing any hook.
  if (!PresetSmartScroller::Class::Method<void(il2cpp::Object*, il2cpp::Object*)>("ClearAndGenerateContents")
      || !PresetSmartScroller::Class::Method<float()>("get_ScrollPosition")
      || !PresetSmartScroller::Class::Method<void(float)>("RestoreScrollPosition"))
    return false;
  if (!mod_state::PrepareAsync())
    return false;
  LoadPresetOrder();
  il2cpp::HookSet hooks("OfficerPresetReorder");
  IL2CPP_STAGE(hooks, OfficerPresetsViewController::Class::Method<void()>("OnDidBindCanvasContext"),
               OfficerPresetsViewController_OnDidBindCanvasContext_Hook);
  IL2CPP_STAGE(hooks, OfficerPresetsViewController::Class::Method<void()>("OnAboutToReleaseCanvasContext"),
               OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook);
  IL2CPP_STAGE(hooks, OfficerPresetsViewController::Class::Method<void(bool)>("OnSaveSlotsSuccess"),
               OfficerPresetsViewController_OnSaveSlotsSuccess_Hook);
  IL2CPP_STAGE(hooks, OfficerPresetItemWidget::Class::Method<void()>("OnEditNameButtonClicked"),
               OfficerPresetItemWidget_OnEditNameButtonClicked_Hook);
  IL2CPP_STAGE(hooks,
               PresetSmartScroller::Class::Method<bool(std::int32_t, il2cpp::Object*&)>("TryGetInstanceForIndex"),
               PresetSmartScroller_TryGetInstanceForIndex_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kOfficerPresetReorder{"OfficerPresetReorder", &Config::Patches::officerpresetreorderhooks,
                                    InstallOfficerPresetReorder, PresetFrame};
