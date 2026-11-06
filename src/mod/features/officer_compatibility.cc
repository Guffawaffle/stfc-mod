// [ui].officer_compatibility_filter scopes shared AAI compatibility to the
// native Outposts predicate. Other native predicates and their grouping remain native.
#include "features/features.h"

#if defined(STFC_AAI_ENABLED)
#include "game/officer_compatibility.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_row_insertion.h"
#include "settings/native_row_quarantine.h"
#include <array>
#include <chrono>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <stfc_aai/officer_compatibility_bridge.h>
#include <thread>

namespace
{
namespace ocb                                                                        = officer_compatibility_binding;
namespace bridge                                                                     = stfc_aai::compatibility_bridge;
constexpr size_t                                              kCompatibilityOptions  = 7;
constexpr size_t                                              kCompatibilityContexts = 64;
constexpr std::array<std::string_view, kCompatibilityOptions> kCompatibilityLabels{
    "Outposts: use compatibility",         "Outposts: specialists only",       "Outposts: defensive encounter",
    "Outposts: unresolved only",           "Outposts: captain maneuvers (CA)", "Outposts: officer abilities (OA)",
    "Outposts: below-decks abilities (BD)"};

struct CompatibilityPolicy {
  bool use_compatibility     = true;
  bool specialists_only      = false;
  bool defensive             = false;
  bool unresolved_only       = false;
  bool captain_maneuvers     = true;
  bool officer_abilities     = true;
  bool below_decks_abilities = true;
  int  RoleMask() const
  { return (captain_maneuvers ? 1 : 0) | (officer_abilities ? 2 : 0) | (below_decks_abilities ? 4 : 0); }
  bool& Option(size_t index)
  {
    switch (index) {
      case 0:
        return use_compatibility;
      case 1:
        return specialists_only;
      case 2:
        return defensive;
      case 3:
        return unresolved_only;
      case 4:
        return captain_maneuvers;
      case 5:
        return officer_abilities;
      default:
        return below_decks_abilities;
    }
  }
};
struct CompatibilityRow {
  il2cpp::GcHandle row, getter, setter;
  bool             attempted  = false;
  bool             configured = false;
};
struct CompatibilityContext {
  il2cpp::GcHandle                                    owner, parent;
  CompatibilityPolicy                                 pending, committed;
  std::array<CompatibilityRow, kCompatibilityOptions> rows;
  bool                                                dirty        = true;
  bool                                                updating     = false;
  bool                                                appending    = false;
  bool                                                usable       = true;
  bool                                                ui_failed    = false;
  bool                                                parent_bound = false;
};
struct OfficerCompatibilityState {
  std::map<ocb::Context*, std::shared_ptr<CompatibilityContext>>              contexts;
  std::array<mod_settings::NativeCallback<bool>, kCompatibilityOptions>       getters;
  std::array<mod_settings::NativeCallback<void, bool>, kCompatibilityOptions> setters;
  mod_settings::NativeCallback<int>                                           query;
  mod_settings::native::RowInsertion<il2cpp::GcHandle>                        insertions;
  mod_settings::native::RowQuarantine<il2cpp::GcHandle>                       quarantine;
  il2cpp::GcHandle                                                            service;
  Throttle        maintenance{std::chrono::milliseconds(250)};
  Throttle        service_probe{std::chrono::seconds(1)};
  std::thread::id thread;
  uint64_t        revision    = 0;
  bool            ready       = false;
  bool            enabled     = true;
  bool            active      = false;
  bool            full_logged = false;
  bool            ui_logged   = false;
};
OfficerCompatibilityState& OfficerCompatibility()
{
  static auto* state = new OfficerCompatibilityState;
  return *state;
}
bool OfficerCompatibilityThread()
{
  const auto& state = OfficerCompatibility();
  return state.active && state.thread == std::this_thread::get_id();
}
std::shared_ptr<CompatibilityContext> FindCompatibilityContext(ocb::Context* context)
{
  auto&      states = OfficerCompatibility().contexts;
  const auto found  = states.find(context);
  return found != states.end() && found->second->owner.Get() == context ? found->second : nullptr;
}
bool SameCompatibilityParent(ocb::Context* context, const std::shared_ptr<CompatibilityContext>& state)
{ return state && state->usable && state->parent.Get() && context->AbilityOptions() == state->parent.Get(); }
void CompatibilityUiFailure()
{
  auto& state = OfficerCompatibility();
  if (!std::exchange(state.ui_logged, true))
    spdlog::warn("[OfficerCompatibility] Native filter controls could not be qualified; preserving native filtering.");
}
void ProbeCompatibilityService()
{
  auto service = il2cpp::GcHandle::Strong(ocb::ExistingSpecService());
  if (!service || service.Get() == OfficerCompatibility().service.Get())
    return;
  bridge::BindService(service.Get());
  OfficerCompatibility().service = il2cpp::GcHandle::Weak(service.Get());
}

// --- Native rows and closed callbacks ---
struct CompatibilityOwnedRow {
  std::shared_ptr<CompatibilityContext> state;
  size_t                                index;
};
std::optional<CompatibilityOwnedRow> CompatibilityRowOwner(ToggleOptionContext* row)
{
  auto get = il2cpp::GcHandle::Strong(row->Getter());
  if (!get)
    return std::nullopt;
  auto& adapter = OfficerCompatibility();
  for (size_t i = 0; i < kCompatibilityOptions; ++i) {
    if (!adapter.getters[i].Owns(reinterpret_cast<Il2CppDelegate*>(get.Get())))
      continue;
    auto* context = ocb::CallbackOwner(get.Get());
    auto  state   = context ? FindCompatibilityContext(context) : nullptr;
    if (!state || state->rows[i].getter.Get() != get.Get())
      return std::nullopt;
    const auto owned = row->CreatedWith(get.Get(), state->rows[i].setter.Get());
    if (owned && *owned)
      return CompatibilityOwnedRow{std::move(state), i};
  }
  return std::nullopt;
}
struct CompatibilityRowCleanup {
  std::optional<std::vector<il2cpp::GcHandle>> Children(Il2CppObject* parent)
  { return ocb::Children(reinterpret_cast<SettingsContainerContext*>(parent)); }
  std::optional<bool> Contains(Il2CppObject* parent, Il2CppObject* row)
  { return ocb::Contains(reinterpret_cast<SettingsContainerContext*>(parent), row); }
  std::optional<bool> Owned(Il2CppObject* row, Il2CppObject* getter, Il2CppObject* setter)
  {
    auto* toggle = reinterpret_cast<il2cpp::Object*>(row)->As<ToggleOptionContext>();
    return toggle ? toggle->CreatedWith(getter, setter) : std::optional<bool>(false);
  }
  void Remove(Il2CppObject* parent, Il2CppObject* row)
  {
    (void)reinterpret_cast<SettingsContainerContext*>(parent)->TryCall<"RemoveChild", void(SettingsOptionContext*)>(
        reinterpret_cast<SettingsOptionContext*>(row));
  }
  bool Configure(Il2CppObject* row, Il2CppObject* query)
  {
    auto* toggle = reinterpret_cast<il2cpp::Object*>(row)->As<ToggleOptionContext>();
    auto  owned  = toggle ? CompatibilityRowOwner(toggle) : std::nullopt;
    if (!owned)
      return false;
    if (!ocb::ConfigureRow(toggle, query, kCompatibilityLabels[owned->index])) {
      owned->state->ui_failed = true;
      owned->state->dirty     = true;
      CompatibilityUiFailure();
      return false;
    }
    auto& slot      = owned->state->rows[owned->index];
    slot.row        = il2cpp::GcHandle::Weak(row);
    slot.configured = static_cast<bool>(slot.row);
    return slot.configured;
  }
  bool Quarantine(Il2CppObject* parent, Il2CppObject* row, Il2CppObject* getter, Il2CppObject* setter)
  {
    auto* toggle = reinterpret_cast<il2cpp::Object*>(row)->As<ToggleOptionContext>();
    if (auto owned = toggle ? CompatibilityRowOwner(toggle) : std::nullopt) {
      owned->state->ui_failed = true;
      owned->state->dirty     = true;
      CompatibilityUiFailure();
    }
    return OfficerCompatibility().quarantine.Add(parent, row, getter, setter);
  }
};
void CleanupCompatibilityRows()
{
  auto&                   state = OfficerCompatibility();
  CompatibilityRowCleanup cleanup;
  if (state.insertions.Size())
    state.insertions.Retry(cleanup);
  if (state.quarantine.Size())
    state.quarantine.Retry(cleanup);
}
template <size_t Index> bool CompatibilityGetter(Il2CppObject* owner, const MethodInfo*)
{
  if (!OfficerCompatibilityThread())
    return false;
  auto* context = reinterpret_cast<il2cpp::Object*>(owner)->As<ocb::Context>();
  auto  state   = context ? FindCompatibilityContext(context) : nullptr;
  return SameCompatibilityParent(context, state) && state->pending.Option(Index);
}
template <size_t Index> void CompatibilitySetter(Il2CppObject* owner, bool value, const MethodInfo*)
{
  if (!OfficerCompatibilityThread() || !Config::Get().ui.officer_compatibility_filter)
    return;
  auto* context = reinterpret_cast<il2cpp::Object*>(owner)->As<ocb::Context>();
  auto  state   = context ? FindCompatibilityContext(context) : nullptr;
  if (!SameCompatibilityParent(context, state) || state->ui_failed || state->updating || state->appending)
    return;
  const auto& slot = state->rows[Index];
  auto        row  = il2cpp::GcHandle::Strong(slot.row.Get());
  if (!slot.configured || !row)
    return;
  const auto member = ocb::Contains(state->parent.Get<SettingsContainerContext>(), row.Get());
  const auto owned  = row.Get<ToggleOptionContext>()->CreatedWith(slot.getter.Get(), slot.setter.Get());
  if (!member || !*member || !owned || !*owned)
    return;
  if (state->pending.Option(Index) != value) {
    state->pending.Option(Index) = value;
    state->dirty                 = true;
    if constexpr (Index == 0)
      if (value && bridge::Unavailable())
        bridge::Refresh();
  }
}
int CompatibilityQuery(Il2CppObject* parent, const MethodInfo*)
{
  if (!OfficerCompatibilityThread() || !Config::Get().ui.officer_compatibility_filter)
    return 2; // Native OptionState.Hidden.
  for (const auto& [context, state] : OfficerCompatibility().contexts)
    if (state->parent.Get() == parent)
      return state->usable && !state->ui_failed ? 0 : 2;
  return 2;
}
void AppendCompatibilityRows(ocb::Context* context, const std::shared_ptr<CompatibilityContext>& state)
{
  if (!Config::Get().ui.officer_compatibility_filter || !SameCompatibilityParent(context, state) || state->appending
      || state->ui_failed)
    return;
  auto parent = il2cpp::GcHandle::Strong(state->parent.Get());
  if (!parent)
    return;
  state->appending = true;
  for (size_t i = 0; i < kCompatibilityOptions; ++i) {
    auto& slot = state->rows[i];
    if (slot.attempted)
      continue;
    if (OfficerCompatibility().insertions.Pending(parent.Get()))
      break;
    auto before = ocb::Children(parent.Get<SettingsContainerContext>());
    if (!before) {
      state->ui_failed = true;
      CompatibilityUiFailure();
      break;
    }
    auto get =
        il2cpp::GcHandle::Strong(ocb::Context::ToggleDelegate(2, context, OfficerCompatibility().getters[i].method()));
    auto set =
        il2cpp::GcHandle::Strong(ocb::Context::ToggleDelegate(3, context, OfficerCompatibility().setters[i].method()));
    auto query = il2cpp::GcHandle::Strong(
        ToggleOptionContext::QueryDelegate(parent.Get<il2cpp::Object>(), OfficerCompatibility().query.method()));
    if (!get || !set || !query) {
      state->ui_failed = true;
      CompatibilityUiFailure();
      break;
    }
    slot.getter = il2cpp::GcHandle::Weak(get.Get());
    slot.setter = il2cpp::GcHandle::Weak(set.Get());
    const auto operation =
        OfficerCompatibility().insertions.Begin(parent.Get(), get.Get(), set.Get(), query.Get(), std::move(*before));
    if (!operation || !slot.getter || !slot.setter) {
      state->ui_failed = true;
      CompatibilityUiFailure();
      break;
    }
    slot.attempted = true; // Unknown append results can never authorize a duplicate.
    const auto added =
        context->TryCall<"AddToggle", void(SettingsContainerContext*, Il2CppString*, il2cpp::Object*, il2cpp::Object*)>(
            parent.Get<SettingsContainerContext>(), il2cpp::NewString(kCompatibilityLabels[i]),
            get.Get<il2cpp::Object>(), set.Get<il2cpp::Object>());
    OfficerCompatibility().insertions.Complete(operation, added.succeeded);
    CleanupCompatibilityRows();
    if (!added || (!slot.configured && !OfficerCompatibility().insertions.Pending(parent.Get()))) {
      state->ui_failed = true;
      CompatibilityUiFailure();
      break;
    }
  }
  state->appending = false;
}
void CompleteCompatibilityContext(ocb::Context* context, const std::shared_ptr<CompatibilityContext>& state)
{
  if (!state || !state->usable)
    return;
  if (!state->parent_bound) {
    auto parent = il2cpp::GcHandle::Strong(context->AbilityOptions());
    if (!parent)
      return;
    state->parent       = il2cpp::GcHandle::Weak(parent.Get());
    state->parent_bound = static_cast<bool>(state->parent);
    // A new native filter session is one explicit retry event. A terminal
    // warm-up failure must not become an automatic per-frame analysis loop.
    if (state->parent_bound && Config::Get().ui.officer_compatibility_filter && state->pending.use_compatibility
        && bridge::Unavailable())
      bridge::Refresh();
  }
  if (state->parent_bound)
    AppendCompatibilityRows(context, state);
}

// --- Scoped native filter lifecycle ---
void OfficerCompatibility_InitFilters_Hook(auto original, ocb::Context* _this)
{
  original(_this);
  if (!OfficerCompatibilityThread())
    return;
  auto& adapter = OfficerCompatibility();
  auto  state   = FindCompatibilityContext(_this);
  if (state) {
    // InitFilters is normally construction-only. If the same managed receiver
    // replaces its options, old closed delegates must not control a new tree.
    if (state->parent_bound && !SameCompatibilityParent(_this, state)) {
      state->usable = false;
      CompatibilityUiFailure();
    }
    return;
  }
  std::erase_if(adapter.contexts, [](const auto& entry) { return !entry.second->owner.Get(); });
  if (adapter.contexts.size() >= kCompatibilityContexts) {
    if (!std::exchange(adapter.full_logged, true))
      spdlog::warn("[OfficerCompatibility] Native context tracking is full; preserving additional native contexts.");
    return;
  }
  state        = std::make_shared<CompatibilityContext>();
  state->owner = il2cpp::GcHandle::Weak(_this);
  if (!state->owner)
    return;
  adapter.contexts.insert_or_assign(_this, state);
  ProbeCompatibilityService();
  // Native construction creates AbilityOptionContext after this method returns.
  // The factory hook completes rows before its caller can bind the panel.
}
ocb::Context* OfficerCompatibility_CreateContext_Hook(auto original)
{
  auto* context = original();
  if (context && OfficerCompatibilityThread()) {
    auto root = il2cpp::GcHandle::Strong(context);
    if (root)
      CompleteCompatibilityContext(root.Get<ocb::Context>(), FindCompatibilityContext(root.Get<ocb::Context>()));
  }
  return context;
}
bool OfficerCompatibility_Outposts_Hook(auto original, ocb::Context* _this, il2cpp::Object* argument)
{
  if (!OfficerCompatibilityThread() || !Config::Get().ui.officer_compatibility_filter)
    return original(_this, argument);
  const auto state = FindCompatibilityContext(_this);
  if (!state || !state->usable || !state->parent.Get() || state->ui_failed || !state->pending.use_compatibility)
    return original(_this, argument);
  const auto& policy = state->pending;
  if (policy.specialists_only && !policy.unresolved_only)
    return original(_this, argument);
  const auto id = ocb::OfficerId(argument);
  if (!id)
    return original(_this, argument);
  const int role_mask = policy.RoleMask();
  if (!role_mask)
    return false;
  if (!bridge::Ready())
    return original(_this, argument);
  const auto result = bridge::FilterOfficer(*id, policy.defensive ? 18 : 17, role_mask, policy.specialists_only,
                                            policy.unresolved_only);
  return result ? *result : original(_this, argument);
}
void OfficerCompatibility_Apply_Hook(auto original, ocb::Context* _this)
{
  original(_this);
  if (OfficerCompatibilityThread() && Config::Get().ui.officer_compatibility_filter)
    if (auto state = FindCompatibilityContext(_this))
      state->committed = state->pending;
}
void OfficerCompatibility_Cancel_Hook(auto original, ocb::Context* _this)
{
  if (OfficerCompatibilityThread())
    if (auto state = FindCompatibilityContext(_this)) {
      state->pending = state->committed;
      state->dirty   = true;
    }
  original(_this);
}
void OfficerCompatibility_Reset_Hook(auto original, ocb::Context* _this)
{
  if (OfficerCompatibilityThread() && Config::Get().ui.officer_compatibility_filter)
    if (auto state = FindCompatibilityContext(_this)) {
      state->pending = {};
      state->dirty   = true;
    }
  original(_this);
}
void OfficerCompatibilityTick()
{
  if (!OfficerCompatibilityThread())
    return;
  auto& adapter = OfficerCompatibility();
  if (adapter.service_probe.Due())
    ProbeCompatibilityService();
  bridge::Tick();
  const bool enabled  = Config::Get().ui.officer_compatibility_filter;
  const bool ready    = bridge::Ready();
  const auto revision = bridge::Revision();
  if (enabled != adapter.enabled || ready != adapter.ready || revision != adapter.revision) {
    for (const auto& [context, state] : adapter.contexts) {
      if (!enabled)
        state->pending = state->committed;
      state->dirty = true;
    }
    adapter.enabled  = enabled;
    adapter.ready    = ready;
    adapter.revision = revision;
  }
  if (!adapter.maintenance.Due())
    return;
  CleanupCompatibilityRows();
  std::erase_if(adapter.contexts, [](const auto& entry) { return !entry.second->owner.Get(); });
  std::vector<std::shared_ptr<CompatibilityContext>> contexts;
  for (const auto& [context, state] : adapter.contexts)
    if (state->dirty || (enabled && !state->ui_failed && !std::ranges::all_of(state->rows, [](const auto& row) {
                           return row.configured;
                         })))
      contexts.push_back(state);
  for (const auto& state : contexts) {
    auto  owner   = il2cpp::GcHandle::Strong(state->owner.Get());
    auto* context = owner.Get<ocb::Context>();
    if (!context || !state->usable || state->updating)
      continue;
    if (enabled && !std::ranges::all_of(state->rows, [](const auto& row) { return row.configured; }))
      CompleteCompatibilityContext(context, state);
    if (!state->parent_bound)
      continue;
    if (!state->dirty)
      continue;
    state->updating = true;
    state->dirty    = !context->RefreshTemporaryFilters();
    state->updating = false;
  }
}
bool InstallOfficerCompatibility()
{
  auto&      state = OfficerCompatibility();
  const auto get   = ocb::Context::Class::Method<bool()>("IsOutpostsFilterOn");
  const auto set   = ocb::Context::Class::Method<void(bool)>("ToggleOutpostsFilter");
  const auto query = SettingsOptionContext::Class::Method<int32_t()>("get_OptionState");
  if (!ocb::Available() || !get || !set || !query || !state.query.Initialize(query.Info(), CompatibilityQuery)) {
    spdlog::warn("[OfficerCompatibility] Native filter metadata is unavailable.");
    return false;
  }
  constexpr std::array<mod_settings::NativeCallback<bool>::Function, kCompatibilityOptions> getters{
      CompatibilityGetter<0>, CompatibilityGetter<1>, CompatibilityGetter<2>, CompatibilityGetter<3>,
      CompatibilityGetter<4>, CompatibilityGetter<5>, CompatibilityGetter<6>};
  constexpr std::array<mod_settings::NativeCallback<void, bool>::Function, kCompatibilityOptions> setters{
      CompatibilitySetter<0>, CompatibilitySetter<1>, CompatibilitySetter<2>, CompatibilitySetter<3>,
      CompatibilitySetter<4>, CompatibilitySetter<5>, CompatibilitySetter<6>};
  for (size_t i = 0; i < kCompatibilityOptions; ++i)
    if (!state.getters[i].Initialize(get.Info(), getters[i]) || !state.setters[i].Initialize(set.Info(), setters[i]))
      return false;
  il2cpp::HookSet hooks("OfficerCompatibility");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ocb::Context::Class::StaticMethod<ocb::Context*()>("CreateContext")),
                  OfficerCompatibility_CreateContext_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ocb::Context::Class::Method<void()>("InitFilters")),
                  OfficerCompatibility_InitFilters_Hook);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(ocb::Context::Class::Method<bool(il2cpp::Object*)>("OfficerHasOutpostAbility")),
                  OfficerCompatibility_Outposts_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ocb::Context::Class::Method<void()>("ApplyFilter")),
                  OfficerCompatibility_Apply_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ocb::Context::Class::Method<void()>("CancelFilter")),
                  OfficerCompatibility_Cancel_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ocb::Context::Class::Method<void()>("ResetAbilityFilters")),
                  OfficerCompatibility_Reset_Hook);
  if (!hooks.Commit())
    return false;
  state.thread = std::this_thread::get_id();
  state.active = true;
  return true;
}
} // namespace

const Feature kOfficerCompatibility{"OfficerCompatibility", &Config::Patches::officercompatibilityhooks,
                                    InstallOfficerCompatibility, OfficerCompatibilityTick};
#endif
