// Native Settings boolean controls. Retained models own values/qualification; this adapter owns pooled views,
// native callback metadata and the single SettingsSectionDirector.AddGeneralSettings hook.
#include "game/settings.h"
#include "config.h"
#include "features/features.h"
#include "runtime_config.h"
#include "settings/confirmation_settings.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"
#include "settings/native_row_insertion.h"
#include "settings/native_row_quarantine.h"
#include "settings/native_view_state.h"
#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

namespace
{
using namespace mod_settings;
struct SettingsBooleanView {
  il2cpp::GcHandle                widget, context, label;
  std::array<il2cpp::GcHandle, 2> indicators;
  std::array<bool, 2>             active{};
  std::optional<NativeViewState>  state;
  bool hidden = false, overridden = false, rendering = false, requesting = false, clearing = false;
  bool preserve_refresh = false, binding = false, cleanup_pending = false, rebind_pending = false, usable = false;
  std::array<bool, 2> restore_indicator{};
  il2cpp::GcHandle    whole_object;
  std::optional<bool> whole_before;
  bool                restore_whole = false;
  std::string         applied_label;
};
struct NativeSettingsState {
  std::map<ToggleOptionWidget*, std::shared_ptr<SettingsBooleanView>> views;
  native::RowQuarantine<il2cpp::GcHandle>                             quarantine;
  native::RowInsertion<il2cpp::GcHandle>                              insertions;
  NativeCallback<bool>                                                get;
  NativeCallback<void, bool>                                          set;
  NativeCallback<int>                                                 query;
  mod_settings::native::PageAppender                                  append     = nullptr;
  mod_settings::native::BooleanResolver                               resolve    = nullptr;
  mod_settings::native::PageInvalidator                               invalidate = nullptr;
  std::thread::id                                                     thread;
  bool                                                                active = false;
};
NativeSettingsState& SettingsState()
{
  static auto* state = new NativeSettingsState;
  return *state;
}
bool SettingsThread()
{ return SettingsState().active && std::this_thread::get_id() == SettingsState().thread; }
thread_local SettingsBooleanView* rendering_settings_boolean = nullptr;

struct SettingsRowCleanup {
  std::optional<std::vector<il2cpp::GcHandle>> Children(Il2CppObject* parent)
  {
    auto* container = reinterpret_cast<SettingsContainerContext*>(parent)->As<SettingsContainerContext>();
    if (!container)
      return std::nullopt;
    auto       root     = il2cpp::GcHandle::Strong(container->Children());
    auto*      children = root.Get<il2cpp::List<SettingsOptionContext*>>();
    const auto count    = children ? children->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
    if (!count || count.value < 0)
      return std::nullopt;
    std::vector<il2cpp::GcHandle> result;
    result.reserve(static_cast<size_t>(count.value));
    for (int32_t i = 0; i < count.value; ++i) {
      auto item = children->TryCall<"get_Item", SettingsOptionContext*(int32_t)>(i);
      if (!item)
        return std::nullopt;
      auto handle = il2cpp::GcHandle::Strong(item.value);
      if (item.value && !handle)
        return std::nullopt;
      result.push_back(std::move(handle));
    }
    return result;
  }
  std::optional<bool> Contains(Il2CppObject* parent, Il2CppObject* row)
  {
    auto* container = reinterpret_cast<SettingsContainerContext*>(parent)->As<SettingsContainerContext>();
    if (!container)
      return std::nullopt;
    auto       root     = il2cpp::GcHandle::Strong(container->Children());
    auto*      children = root.Get<il2cpp::List<SettingsOptionContext*>>();
    const auto count    = children ? children->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
    if (!count || count.value < 0)
      return std::nullopt;
    for (int32_t i = 0; i < count.value; ++i) {
      const auto item = children->TryCall<"get_Item", SettingsOptionContext*(int32_t)>(i);
      if (!item)
        return std::nullopt;
      if (item.value == row)
        return true;
    }
    return false;
  }
  std::optional<bool> Owned(Il2CppObject* row, Il2CppObject* getter, Il2CppObject* setter)
  {
    auto* toggle = reinterpret_cast<SettingsOptionContext*>(row)->As<ToggleOptionContext>();
    return toggle ? toggle->CreatedWith(getter, setter) : std::optional<bool>(false);
  }
  void Remove(Il2CppObject* parent, Il2CppObject* row)
  {
    (void)reinterpret_cast<SettingsContainerContext*>(parent)->TryCall<"RemoveChild", void(SettingsOptionContext*)>(
        reinterpret_cast<SettingsOptionContext*>(row));
  }
  bool Configure(Il2CppObject* row, Il2CppObject* query)
  {
    auto*      toggle = reinterpret_cast<ToggleOptionContext*>(row);
    const auto set =
        toggle->TryCall<"set_QueryOptionState", void(il2cpp::Object*)>(reinterpret_cast<il2cpp::Object*>(query));
    return set && toggle->Query() == query;
  }
  bool Quarantine(Il2CppObject* parent, Il2CppObject* row, Il2CppObject* get, Il2CppObject* set)
  { return SettingsState().quarantine.Add(parent, row, get, set); }
};
void RetrySettingsCleanup()
{
  SettingsRowCleanup cleanup;
  SettingsState().insertions.Retry(cleanup);
  SettingsState().quarantine.Retry(cleanup);
}
void RevokeBoolean(SettingsBooleanView& view)
{
  view.usable = false;
  if (view.state)
    view.state->Invalidate();
}

BooleanSetting* BooleanFor(SettingsOptionContext* context)
{
  if (!context)
    return nullptr;
  for (auto* setting : {&FleetCommanderConfirmationSetting(), &ForbiddenTechConfirmationSetting()})
    if (context->HasLabel(setting->id()))
      return setting;
  if (SettingsState().resolve)
    return SettingsState().resolve(context);
  return nullptr;
}
bool OwnsBoolean(ToggleOptionContext* context)
{
  if (!context || context->klass != ToggleOptionContext::Class::Raw())
    return false;
  auto* callback = reinterpret_cast<Il2CppDelegate*>(context->Query());
  return SettingsState().query.Owns(callback) && BooleanFor(context);
}
bool ProvisionalBoolean(ToggleOptionContext* context)
{
  if (!context || context->klass != ToggleOptionContext::Class::Raw() || OwnsBoolean(context))
    return false;
  auto  root     = il2cpp::GcHandle::Strong(context->Getter());
  auto* callback = reinterpret_cast<Il2CppDelegate*>(root.Get());
  return SettingsState().get.Owns(callback);
}
std::shared_ptr<SettingsBooleanView> BooleanViewFor(ToggleOptionWidget* widget)
{
  const auto found = SettingsState().views.find(widget);
  return found == SettingsState().views.end() ? nullptr : found->second;
}
bool SameBooleanBinding(ToggleOptionWidget* widget, const std::shared_ptr<SettingsBooleanView>& view)
{
  const auto observed = widget->TryCall<"get_Context", ToggleOptionContext*>();
  const bool same     = observed && BooleanViewFor(widget) == view && view->widget.Get() == widget
                        && observed.value == view->context.Get();
  if (!same) {
    view->cleanup_pending = view->rebind_pending = true;
    RevokeBoolean(*view);
  }
  return same;
}
bool RecheckBooleanContext(ToggleOptionWidget* widget, ToggleOptionContext* expected)
{
  const auto now = widget->TryCall<"get_Context", ToggleOptionContext*>();
  if (now && now.value == expected)
    return true;
  auto view = BooleanViewFor(widget);
  if (!view) {
    view          = std::make_shared<SettingsBooleanView>();
    view->widget  = il2cpp::GcHandle::Weak(widget);
    view->context = il2cpp::GcHandle::Weak(now ? now.value : nullptr);
    SettingsState().views.insert_or_assign(widget, view);
  }
  view->cleanup_pending = view->rebind_pending = true;
  RevokeBoolean(*view);
  return false;
}
bool SetBooleanVisibility(unity::GameObject* object, bool desired)
{
  if (!object)
    return true;
  const auto alive = unity::TryAlive(object);
  if (!alive)
    return false;
  if (!alive.value)
    return true;
  auto value = object->TryCall<"get_activeSelf", bool>();
  if (value && value.value == desired)
    return true;
  (void)object->TryCall<"SetActive", void(bool)>(desired);
  value = object->TryCall<"get_activeSelf", bool>();
  return value && value.value == desired;
}
bool RestoreBoolean(SettingsBooleanView& view)
{
  bool complete = true;
  for (size_t i = 0; i < view.indicators.size(); ++i) {
    if (!view.restore_indicator[i])
      continue;
    if (SetBooleanVisibility(view.indicators[i].Get<unity::GameObject>(), view.active[i]))
      view.restore_indicator[i] = false;
    else
      complete = false;
  }
  view.hidden = std::ranges::any_of(view.restore_indicator, [](bool value) { return value; });
  if (view.overridden) {
    auto       root  = il2cpp::GcHandle::Strong(view.label.Get());
    auto*      label = root.Get<TextLocalizer>();
    const auto alive = label ? unity::TryAlive(label) : il2cpp::CallResult<bool>{true, false};
    if (alive && !alive.value)
      view.overridden = false;
    else if (alive) {
      const auto current = label->TryCall<"CurrentText", Il2CppString*>();
      if (!current) {
        complete = false;
      } else if (il2cpp::ToUtf8(current.value) != view.applied_label) {
        view.overridden = false;
      } else {
        const auto cleared = label->TryCall<"ClearTextOverride", void>();
        if (cleared && !label->textOverride())
          view.overridden = false;
        else
          complete = false;
      }
    } else
      complete = false;
  }
  if (view.restore_whole) {
    if (view.whole_before && SetBooleanVisibility(view.whole_object.Get<unity::GameObject>(), *view.whole_before))
      view.restore_whole = false;
    else
      complete = false;
  }
  return complete;
}
bool ReleaseBoolean(ToggleOptionWidget* widget)
{
  auto view = BooleanViewFor(widget);
  if (!view)
    return true;
  RevokeBoolean(*view);
  if (view->clearing || view->rendering || view->requesting || view->binding) {
    view->cleanup_pending = true;
    return false;
  }
  view->clearing      = true; // Keep sentinel visible throughout managed restoration callbacks.
  const bool restored = RestoreBoolean(*view);
  if (view->state)
    view->state->Unbind();
  view->clearing = false;
  if (restored && BooleanViewFor(widget) == view)
    SettingsState().views.erase(widget);
  else
    view->cleanup_pending = true;
  return restored;
}
void HideUnsupportedBoolean(ToggleOptionWidget* widget, ToggleOptionContext* context)
{
  auto root = il2cpp::GcHandle::Strong(widget);
  if (widget->Context() != context)
    return;
  auto view = BooleanViewFor(widget);
  if (!view) {
    view          = std::make_shared<SettingsBooleanView>();
    view->widget  = il2cpp::GcHandle::Weak(widget);
    view->context = il2cpp::GcHandle::Weak(context);
    SettingsState().views.insert_or_assign(widget, view);
  }
  RevokeBoolean(*view);
  view->rebind_pending   = true; // Query installation or repaired metadata must get another binding attempt.
  auto       object_root = il2cpp::GcHandle::Strong(widget->Call<"get_gameObject", unity::GameObject*>());
  auto*      object      = object_root.Get<unity::GameObject>();
  const auto active      = object ? object->TryCall<"get_activeSelf", bool>() : il2cpp::CallResult<bool>{};
  if (active && !view->restore_whole) {
    view->whole_before  = active.value;
    view->whole_object  = il2cpp::GcHandle::Weak(object);
    view->restore_whole = bool(view->whole_object);
  }
  if (!view->restore_whole || !SetBooleanVisibility(object, false)) {
    static bool warned = false;
    if (!std::exchange(warned, true))
      spdlog::warn("[NativeSettings] owned row visibility unavailable; binding remains uneditable");
  }
}

bool BooleanGetter(Il2CppObject*, const MethodInfo*)
{
  return rendering_settings_boolean && rendering_settings_boolean->state
             ? rendering_settings_boolean->state->value().value_or(false)
             : false;
}
void BooleanSetter(Il2CppObject*, bool, const MethodInfo*) {}
int  BooleanQuery(Il2CppObject*, const MethodInfo*)
{ return 0; }

std::shared_ptr<SettingsBooleanView> TrackBoolean(ToggleOptionWidget* widget, ToggleOptionContext* context)
{
  auto* setting = BooleanFor(context);
  if (!setting)
    return {};
  auto view     = std::make_shared<SettingsBooleanView>();
  view->widget  = il2cpp::GcHandle::Weak(widget);
  view->context = il2cpp::GcHandle::Weak(context);
  if (!view->widget || !view->context)
    return {};
  view->state.emplace(*setting);
  view->binding = true;
  struct Guard {
    SettingsBooleanView& view;
    ~Guard()
    {
      view.binding = false;
      if (!view.usable)
        view.cleanup_pending = view.rebind_pending = true;
    }
  } guard{*view};
  SettingsState().views.insert_or_assign(widget, view);
  auto  widget_root     = il2cpp::GcHandle::Strong(widget);
  auto  context_root    = il2cpp::GcHandle::Strong(context);
  auto  label_root      = il2cpp::GcHandle::Strong(widget->Label());
  auto  toggle_root     = il2cpp::GcHandle::Strong(widget->Toggle());
  auto  animator_root   = il2cpp::GcHandle::Strong(widget->StateAnimator());
  auto* label           = label_root.Get<TextLocalizer>();
  auto* toggle          = toggle_root.Get<unity::Toggle>();
  auto* animator        = animator_root.Get<unity::Component>();
  auto* root            = widget->Call<"get_transform", unity::Transform*>();
  auto* label_transform = label ? label->Call<"get_transform", unity::Transform*>() : nullptr;
  if (!label || !toggle || !animator || !root || !label_transform)
    return {};
  const std::array objects{toggle->gameObject(), animator->gameObject()};
  for (size_t i = 0; i < objects.size(); ++i) {
    auto* transform = objects[i] ? objects[i]->transform() : nullptr;
    if (!transform || transform == root || !transform->Call<"IsChildOf", bool(unity::Transform*)>(root)
        || label_transform->Call<"IsChildOf", bool(unity::Transform*)>(transform))
      return {};
    view->indicators[i] = il2cpp::GcHandle::Weak(objects[i]);
    if (!view->indicators[i])
      return {};
  }
  view->label = il2cpp::GcHandle::Weak(label);
  if (!view->widget || !view->context || !view->label)
    return {};
  if (!SameBooleanBinding(widget, view))
    return {};
  view->usable = true;
  return view;
}

void RenderBoolean(const std::shared_ptr<SettingsBooleanView>& view, auto original, ToggleOptionWidget* widget)
{
  if (view->rendering || view->clearing || !view->state)
    return;
  auto widget_root  = il2cpp::GcHandle::Strong(widget);
  auto context_root = il2cpp::GcHandle::Strong(view->context.Get());
  struct Scope {
    SettingsBooleanView&         view;
    SettingsBooleanView*         previous;
    NativeViewState::RenderScope suppress;
    Scope(SettingsBooleanView& value)
        : view(value)
        , previous(rendering_settings_boolean)
        , suppress(*value.state)
    {
      view.rendering             = true;
      rendering_settings_boolean = &view;
    }
    ~Scope()
    {
      rendering_settings_boolean = previous;
      view.rendering             = false;
    }
  } scope(*view);
  if (!RestoreBoolean(*view)) {
    HideUnsupportedBoolean(widget, context_root.Get<ToggleOptionContext>());
    return;
  }
  if (!SameBooleanBinding(widget, view))
    return;
  original(widget);
  if (!SameBooleanBinding(widget, view))
    return;
  auto label_root = il2cpp::GcHandle::Strong(view->label.Get());
  if (auto* label = label_root.Get<TextLocalizer>()) {
    view->applied_label = view->state->presentation_label();
    const auto changed =
        label->TryCall<"OverrideLocalizedText", void(Il2CppString*)>(il2cpp::NewString(view->applied_label));
    const auto text  = label->TryCall<"CurrentText", Il2CppString*>();
    view->overridden = !text || il2cpp::ToUtf8(text.value) == view->applied_label;
    if (!SameBooleanBinding(widget, view))
      return;
    if (!changed || !text || il2cpp::ToUtf8(text.value) != view->applied_label) {
      HideUnsupportedBoolean(widget, context_root.Get<ToggleOptionContext>());
      return;
    }
  }
  if (!view->state->known()) {
    for (size_t i = 0; i < view->indicators.size(); ++i) {
      auto* object = view->indicators[i].Get<unity::GameObject>();
      if (!object || !unity::Alive(object)) {
        HideUnsupportedBoolean(widget, context_root.Get<ToggleOptionContext>());
        return;
      }
      const auto active = object->TryCall<"get_activeSelf", bool>();
      if (!active) {
        HideUnsupportedBoolean(widget, context_root.Get<ToggleOptionContext>());
        return;
      }
      view->active[i] = active.value;
    }
    view->hidden            = true;
    view->restore_indicator = {true, true};
    for (auto& handle : view->indicators) {
      if (!SameBooleanBinding(widget, view))
        return;
      if (!SetBooleanVisibility(handle.Get<unity::GameObject>(), false)) {
        HideUnsupportedBoolean(widget, context_root.Get<ToggleOptionContext>());
        return;
      }
    }
  }
}
void ToggleOptionWidget_SetWidgetData_Hook(auto original, ToggleOptionWidget* widget)
{
  if (!SettingsThread())
    return original(widget);
  auto  context_root = il2cpp::GcHandle::Strong(widget->Context());
  auto* context      = context_root.Get<ToggleOptionContext>();
  auto  view         = BooleanViewFor(widget);
  if (view && (view->rendering || view->requesting || view->clearing || view->binding)) {
    view->rebind_pending = true;
    RevokeBoolean(*view);
    return;
  }
  if (view && view->context.Get() != context) {
    if (!ReleaseBoolean(widget))
      return;
    view.reset();
  }
  if (!RecheckBooleanContext(widget, context))
    return;
  if (context && (SettingsState().quarantine.Contains(context) || ProvisionalBoolean(context))) {
    HideUnsupportedBoolean(widget, context);
    return;
  }
  if (!OwnsBoolean(context)) {
    if (view && !ReleaseBoolean(widget))
      return;
    return original(widget);
  }
  if (!view)
    view = TrackBoolean(widget, context);
  if (!view) {
    HideUnsupportedBoolean(widget, context);
    return;
  }
  if (!view->state || !view->usable) {
    if (ReleaseBoolean(widget))
      widget->Call<"SetWidgetData">();
    return;
  }
  if (!std::exchange(view->preserve_refresh, false)) {
    view->binding = true;
    view->state->Bind();
    view->binding = false;
  }
  if (!SameBooleanBinding(widget, view))
    return;
  RenderBoolean(view, original, widget);
}
void ToggleOptionWidget_OnToggleValueChanged_Hook(auto original, ToggleOptionWidget* widget, bool desired)
{
  if (!SettingsThread())
    return original(widget, desired);
  auto context_root = il2cpp::GcHandle::Strong(widget->Context());
  if (context_root
      && (SettingsState().quarantine.Contains(context_root.Get())
          || ProvisionalBoolean(context_root.Get<ToggleOptionContext>())))
    return;
  if (!OwnsBoolean(context_root.Get<ToggleOptionContext>()))
    return original(widget, desired);
  auto view = BooleanViewFor(widget);
  if (!view || !view->state || !view->usable || view->cleanup_pending || view->rebind_pending || view->requesting
      || view->rendering || view->clearing || view->binding || !SameBooleanBinding(widget, view))
    return;
  view->requesting  = true;
  const auto result = view->state->Request(desired);
  view->requesting  = false;
  if (result == Outcome::Suppressed || result == Outcome::Busy || !SameBooleanBinding(widget, view))
    return;
  // Refresh the native visual without rebinding, preserving unverified/failed write qualification.
  view->preserve_refresh = true;
  widget->Call<"SetWidgetData">();
}
void ToggleOptionWidget_OnAboutToReleaseContext_Hook(auto original, ToggleOptionWidget* widget)
{
  if (SettingsThread())
    ReleaseBoolean(widget);
  original(widget);
}
SettingsContainerContext* ConfirmationCategory(SettingsOptionContext* root, int depth, int& remaining)
{
  if (!root || depth > 3 || --remaining < 0)
    return nullptr;
  auto* container = root->As<SettingsContainerContext>();
  if (!container)
    return nullptr;
  if (container->HasLabel("game_settings_category_7"))
    return container;
  auto  children_root = il2cpp::GcHandle::Strong(container->Children());
  auto* children      = children_root.Get<il2cpp::List<SettingsOptionContext*>>();
  if (!children)
    return nullptr;
  for (int i = 0, count = children->Count(); i < count && remaining > 0; ++i)
    if (auto* found = ConfirmationCategory((*children)[i], depth + 1, remaining))
      return found;
  return nullptr;
}
void AddConfirmationRow(SettingsSectionDirector* director, SettingsContext* context, SettingsContainerContext* parent,
                        BooleanSetting& setting)
{
  if (setting.Observe().state.availability == Availability::Unsupported)
    return;
  if (SettingsState().insertions.Pending(parent))
    return; // Unknown discovery cannot authorize another append to this category.
  auto       children_root = il2cpp::GcHandle::Strong(parent->Children());
  auto*      children      = children_root.Get<il2cpp::List<SettingsOptionContext*>>();
  const auto count         = children ? children->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return;
  std::vector<il2cpp::GcHandle> before;
  before.reserve(static_cast<size_t>(count.value));
  for (int32_t i = 0; i < count.value; ++i) {
    const auto item = children->TryCall<"get_Item", SettingsOptionContext*(int32_t)>(i);
    if (!item)
      return;
    if (item.value && item.value->HasLabel(setting.id()))
      return;
    before.push_back(il2cpp::GcHandle::Strong(item.value));
  }
  auto get   = il2cpp::GcHandle::Strong(SettingsContext::ToggleDelegate(2, director, SettingsState().get.method()));
  auto set   = il2cpp::GcHandle::Strong(SettingsContext::ToggleDelegate(3, director, SettingsState().set.method()));
  auto query = il2cpp::GcHandle::Strong(ToggleOptionContext::QueryDelegate(director, SettingsState().query.method()));
  if (!get || !set || !query)
    return;
  const auto operation = SettingsState().insertions.Begin(parent, get.Get(), set.Get(), query.Get(), std::move(before));
  if (!operation)
    return;
  const auto inserted =
      context->TryCall<"AddToggle", void(SettingsContainerContext*, Il2CppString*, il2cpp::Object*, il2cpp::Object*)>(
          parent, il2cpp::NewString(setting.id()), reinterpret_cast<il2cpp::Object*>(get.Get()),
          reinterpret_cast<il2cpp::Object*>(set.Get()));
  SettingsState().insertions.Complete(operation, inserted.succeeded);
  RetrySettingsCleanup();
}

void SettingsSectionDirector_AddGeneralSettings_Hook(auto original, SettingsSectionDirector* director,
                                                     SettingsContext* context)
{
  original(director, context);
  if (!SettingsThread() || !context)
    return;
  auto director_root = il2cpp::GcHandle::Strong(director);
  auto context_root  = il2cpp::GcHandle::Strong(context);
  auto root          = il2cpp::GcHandle::Strong(context->RootOption());
  RetrySettingsCleanup();
  int  remaining = 128;
  auto category  = il2cpp::GcHandle::Strong(ConfirmationCategory(root.Get<SettingsOptionContext>(), 0, remaining));
  if (auto* parent = category.Get<SettingsContainerContext>()) {
    AddConfirmationRow(director, context, parent, FleetCommanderConfirmationSetting());
    AddConfirmationRow(director, context, parent, ForbiddenTechConfirmationSetting());
  }
  if (SettingsState().append)
    SettingsState().append(director, context);
}
void PrefsBoundary(auto original, PersistentPrefsManager* prefs)
{
  if (SettingsThread()) {
    if (SettingsState().invalidate)
      SettingsState().invalidate();
    InvalidateConfirmationSettings();
    std::vector<ToggleOptionWidget*> keys;
    for (const auto& [widget, view] : SettingsState().views)
      keys.push_back(widget);
    for (auto* key : keys) {
      auto  view   = BooleanViewFor(key);
      auto  root   = il2cpp::GcHandle::Strong(view ? view->widget.Get() : nullptr);
      auto* widget = root.Get<ToggleOptionWidget>();
      if (!widget || !view->state)
        continue;
      view->state->Invalidate();
      RenderBoolean(view, [](ToggleOptionWidget*) {}, widget);
    }
  }
  original(prefs);
}
void NativeSettingsFrame()
{
  auto& state = SettingsState();
  if (!SettingsThread() || (state.views.empty() && state.quarantine.Size() == 0 && state.insertions.Size() == 0))
    return;
  static uint64_t      last_status = 0;
  static const Config* last_config = nullptr;
  static Throttle      lifecycle(std::chrono::milliseconds(250));
  const auto           status         = runtime_config::StatusSequence();
  const bool           status_changed = std::exchange(last_status, status) != status;
  const bool           config_changed = std::exchange(last_config, &Config::Get()) != &Config::Get();
  if (!status_changed && !config_changed && !lifecycle.Due())
    return;
  RetrySettingsCleanup();
  std::vector<ToggleOptionWidget*> keys;
  for (const auto& [key, view] : state.views)
    keys.push_back(key);
  for (auto* key : keys) {
    auto view = BooleanViewFor(key);
    if (!view || view->binding || view->rendering || view->requesting || view->clearing)
      continue;
    auto       root   = il2cpp::GcHandle::Strong(view->widget.Get());
    auto*      widget = root.Get<ToggleOptionWidget>();
    const auto alive  = widget ? unity::TryAlive(widget) : il2cpp::CallResult<bool>{true, false};
    if (!alive)
      continue;
    if (!alive.value) {
      ReleaseBoolean(key);
      continue;
    }
    const bool cleanup = view->cleanup_pending;
    if (cleanup && !ReleaseBoolean(key))
      continue;
    if (!cleanup && !status_changed && !config_changed && !view->rebind_pending)
      continue;
    if (!widget->Context())
      continue;
    if (!cleanup)
      view->preserve_refresh = !config_changed && !std::exchange(view->rebind_pending, false);
    widget->Call<"SetWidgetData">();
  }
}
bool InstallNativeSettings()
{
  auto&      state = SettingsState();
  const auto get   = SettingsSectionDirector::Class::Method<bool()>("IsBorgCubeCuttingBeamConfirmationOn");
  const auto set   = SettingsSectionDirector::Class::Method<void(bool)>("ToggleBorgCubeCuttingBeamConfirmation");
  const auto query = SettingsSectionDirector::Class::Method<int32_t()>("QueryShouldShowGenericPcSetting");
  if (!get || !set || !query || !SettingsSectionDirector::QualifiedQueryState()
      || !state.get.Initialize(get.Info(), BooleanGetter) || !state.set.Initialize(set.Info(), BooleanSetter)
      || !state.query.Initialize(query.Info(), BooleanQuery)
      || !ToggleOptionWidget::Class::HasField<"_label", TextLocalizer*>()
      || !TextLocalizer::Class::HasField<"_textOverride", bool>()
      || !ToggleOptionWidget::Class::HasField<"_toggle", unity::Toggle*>()
      || !ToggleOptionWidget::Class::HasField<"_toggleStateAnimator", unity::Component*>())
    return false;
  il2cpp::HookSet hooks("NativeSettings");
  IL2CPP_STAGE_AT(
      hooks, il2cpp::CheckedEntry(SettingsSectionDirector::Class::Method<void(SettingsContext*)>("AddGeneralSettings")),
      SettingsSectionDirector_AddGeneralSettings_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ToggleOptionWidget::Class::Method<void()>("SetWidgetData")),
                  ToggleOptionWidget_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ToggleOptionWidget::Class::Method<void(bool)>("OnToggleValueChanged")),
                  ToggleOptionWidget_OnToggleValueChanged_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(ToggleOptionWidget::Class::Method<void()>("OnAboutToReleaseContext")),
                  ToggleOptionWidget_OnAboutToReleaseContext_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(PersistentPrefsManager::Class::Method<void()>("RegisterEvents")),
                  PrefsBoundary);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(PersistentPrefsManager::Class::Method<void()>("GameSessionStartedEventHandler")),
                  PrefsBoundary);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(PersistentPrefsManager::Class::Method<void()>("LoadPersistentPrefsFromCloud")),
                  PrefsBoundary);
  if (!hooks.Commit())
    return false;
  state.thread = std::this_thread::get_id();
  state.active = true;
  return true;
}
} // namespace

bool mod_settings::native::RegisterPageAppender(PageAppender appender)
{
  if (!appender || SettingsState().append)
    return false;
  SettingsState().append = appender;
  return true;
}
bool mod_settings::native::RegisterBooleanResolver(BooleanResolver resolver)
{
  if (!resolver || SettingsState().resolve)
    return false;
  SettingsState().resolve = resolver;
  return true;
}
mod_settings::BooleanSetting* mod_settings::native::OwnedBooleanSetting(SettingsOptionContext* context)
{
  auto* toggle = context ? context->As<ToggleOptionContext>() : nullptr;
  return toggle && OwnsBoolean(toggle) ? BooleanFor(context) : nullptr;
}
bool mod_settings::native::RegisterPageInvalidator(PageInvalidator invalidator)
{
  if (!invalidator || SettingsState().invalidate)
    return false;
  SettingsState().invalidate = invalidator;
  return true;
}
bool mod_settings::native::AppendBooleanRow(SettingsSectionDirector* director, SettingsContext* context,
                                            SettingsContainerContext* parent, BooleanSetting& setting)
{
  if (!SettingsThread() || !director || !context || !parent)
    return false;
  if (setting.Observe().state.availability == Availability::Unsupported)
    return true;
  AddConfirmationRow(director, context, parent, setting);
  if (SettingsState().insertions.Pending(parent))
    return false;
  SettingsRowCleanup cleanup;
  auto               children = cleanup.Children(parent);
  if (!children)
    return false;
  size_t owned = 0;
  for (const auto& child : *children) {
    auto* row = child.Get<SettingsOptionContext>();
    if (row && OwnedBooleanSetting(row) == &setting)
      ++owned;
  }
  return owned == 1;
}
const Feature kNativeSettings{"NativeSettings", &Config::Patches::nativesettingshooks, InstallNativeSettings,
                              NativeSettingsFrame};
