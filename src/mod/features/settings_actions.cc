// Native command rows. Each context owns a unique closed callback token; pooled widget release never cancels a
// page-owned editor. Dynamic repeated rows use the existing insertion/quarantine policies before native mutation.
#include "game/settings_actions.h"
#include "features/features.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"
#include "settings/native_row_insertion.h"
#include "settings/native_row_quarantine.h"
#include "shortcuts.h"

#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>

namespace
{
using namespace mod_settings;
using namespace mod_settings::native;
struct SettingsActionRow {
  il2cpp::GcHandle context, parent, click, query, token;
  ActionSetting*   action = nullptr;
  size_t           index  = 0;
};
struct SettingsActionPlan {
  il2cpp::GcHandle parent, click, query, token;
  ActionSetting*   action;
  size_t           index;
  uint64_t         epoch;
};
struct SettingsActionView {
  il2cpp::GcHandle                widget, context, token, button, button_object;
  std::array<il2cpp::GcHandle, 3> labels;
  std::array<std::string, 3>      applied;
  std::array<bool, 3>             label_owned{};
  ActionSetting*                  action = nullptr;
  size_t                          index  = 0;
  PageVisitToken                  visit  = 0;
  std::optional<bool>             interactable, hidden_before;
  bool revoked = false, rendering = false, invoking = false, ready = false, restoring = false;
};
struct NativeActionsState {
  NativeCallback<void>                                                 click;
  NativeCallback<Il2CppString*>                                        text;
  NativeCallback<int>                                                  query;
  std::map<SettingsOptionContext*, std::shared_ptr<SettingsActionRow>> rows;
  std::map<Il2CppObject*, std::shared_ptr<SettingsActionPlan>>         plans;
  std::map<SettingsActionWidget*, std::shared_ptr<SettingsActionView>> views;
  RowInsertion<il2cpp::GcHandle>                                       insertions;
  RowQuarantine<il2cpp::GcHandle>                                      quarantine;
  std::thread::id                                                      thread;
  uint64_t                                                             epoch  = 0;
  bool                                                                 active = false;
};
NativeActionsState& Actions()
{
  static auto* state = new NativeActionsState;
  return *state;
}
thread_local il2cpp::Object* invoking_action_widget = nullptr;
bool                         ActionsThread()
{ return Actions().active && Actions().thread == std::this_thread::get_id(); }
void ActionWarning() noexcept
{
  static bool reported = false;
  if (!std::exchange(reported, true)) {
    try {
      spdlog::warn("[NativeSettingsActions] command observation or presentation unavailable");
    } catch (...) {
    }
  }
}
Il2CppString* ActionEmptyText(Il2CppObject*, const MethodInfo*)
{ return il2cpp::NewString(""); }
int ActionQuery(Il2CppObject*, const MethodInfo*)
{ return 0; }

std::optional<bool> ActionOwned(Il2CppObject* object, Il2CppObject* click)
{
  if (!object || object->klass != SettingsActionContext::Class::Raw())
    return false;
  auto* delegate = reinterpret_cast<Il2CppDelegate*>(click);
  return Actions().click.Owns(delegate) ? reinterpret_cast<SettingsActionContext*>(object)->CreatedWith(click)
                                        : std::optional<bool>(false);
}
std::shared_ptr<SettingsActionRow> ActionRow(SettingsOptionContext* context)
{
  if (!context)
    return nullptr;
  const auto found = Actions().rows.find(context);
  if (found == Actions().rows.end() || found->second->context.Get() != context)
    return nullptr;
  const auto row       = found->second;
  auto       click     = il2cpp::GcHandle::Strong(row->click.Get());
  auto       query     = il2cpp::GcHandle::Strong(row->query.Get());
  auto       token     = il2cpp::GcHandle::Strong(row->token.Get());
  const auto parent    = settings_pages_binding::Parent(context);
  const auto current   = settings_pages_binding::Query(context);
  const auto owned     = click ? ActionOwned(context, click.Get()) : std::nullopt;
  const auto confirmed = Actions().rows.find(context);
  return confirmed != Actions().rows.end() && confirmed->second == row && click && query && token && parent
                 && parent.value == row->parent.Get() && current && current.value == query.Get()
                 && Actions().query.Owns(reinterpret_cast<Il2CppDelegate*>(query.Get()))
                 && reinterpret_cast<Il2CppDelegate*>(click.Get())->target == token.Get() && owned && *owned
             ? row
             : nullptr;
}
std::shared_ptr<SettingsActionView> ActionView(SettingsActionWidget* widget)
{
  const auto found = Actions().views.find(widget);
  return found == Actions().views.end() ? nullptr : found->second;
}
bool SameAction(SettingsActionWidget* widget, const std::shared_ptr<SettingsActionView>& view)
{
  if (!view || view->revoked || ActionView(widget) != view || view->widget.Get() != widget
      || !IsPageVisitCurrent(view->visit))
    return false;
  const auto context = widget->TryCall<"get_Context", SettingsActionContext*>();
  auto       row     = context ? ActionRow(context.value) : nullptr;
  return row && context.value == view->context.Get() && row->token.Get() == view->token.Get()
         && row->action == view->action && row->index == view->index && !view->revoked && ActionView(widget) == view;
}

struct ActionInsertionHost {
  std::optional<std::vector<il2cpp::GcHandle>> Children(Il2CppObject* parent)
  { return settings_pages_binding::Children(reinterpret_cast<SettingsContainerContext*>(parent)); }
  std::optional<bool> Owned(Il2CppObject* row, Il2CppObject*, Il2CppObject* click)
  { return ActionOwned(row, click); }
  std::optional<bool> Contains(Il2CppObject* parent, Il2CppObject* row)
  {
    const auto children = Children(parent);
    return children ? std::optional<bool>(
                          std::ranges::any_of(*children, [row](const auto& child) { return child.Get() == row; }))
                    : std::nullopt;
  }
  void Remove(Il2CppObject* parent, Il2CppObject* row)
  {
    (void)reinterpret_cast<SettingsContainerContext*>(parent)->TryCall<"RemoveChild", void(SettingsOptionContext*)>(
        reinterpret_cast<SettingsOptionContext*>(row));
  }
  bool Quarantine(Il2CppObject* parent, Il2CppObject* row, Il2CppObject* get, Il2CppObject* set)
  { return Actions().quarantine.Add(parent, row, get, set); }
  bool Configure(Il2CppObject* object, Il2CppObject* query)
  {
    const auto found = Actions().plans.find(query);
    if (found == Actions().plans.end())
      return false;
    const auto plan = found->second;
    if (plan->epoch != Actions().epoch || plan->query.Get() != query || !plan->click.Get() || !plan->token.Get())
      return false;
    auto*      row    = reinterpret_cast<SettingsActionContext*>(object);
    const auto parent = settings_pages_binding::Parent(row);
    if (!parent || parent.value != plan->parent.Get() || !row->HasLabel(plan->action->item_id(plan->index))
        || !row->TryCall<"set_QueryOptionState", void(settings_binding::ReferenceDelegate*)>(
            reinterpret_cast<settings_binding::ReferenceDelegate*>(query)))
      return false;
    const auto current = settings_pages_binding::Query(row);
    if (!current || current.value != query || plan->epoch != Actions().epoch)
      return false;
    SettingsActionRow record{il2cpp::GcHandle::Weak(row),
                             il2cpp::GcHandle::Weak(parent.value),
                             il2cpp::GcHandle::Weak(plan->click.Get()),
                             il2cpp::GcHandle::Weak(query),
                             il2cpp::GcHandle::Weak(plan->token.Get()),
                             plan->action,
                             plan->index};
    if (!record.context || !record.parent || !record.click || !record.query || !record.token)
      return false;
    Actions().rows.insert_or_assign(row, std::make_shared<SettingsActionRow>(std::move(record)));
    Actions().plans.erase(query);
    return true;
  }
};
bool RetryActionInsertions(SettingsContainerContext* parent)
{
  ActionInsertionHost host;
  Actions().insertions.Retry(host);
  Actions().quarantine.Retry(host);
  if (Actions().insertions.Pending(parent))
    return false;
  const auto children = settings_pages_binding::Children(parent);
  return children
         && std::ranges::none_of(*children, [](const auto& row) { return Actions().quarantine.Contains(row.Get()); });
}

bool AppendAction(SettingsSectionDirector* director, SettingsContext* context, SettingsContainerContext* parent,
                  const PageCatalog::Item& item)
{
  const auto* setting = std::get_if<ActionSetting*>(&item);
  if (!ActionsThread() || !setting || !*setting || !RetryActionInsertions(parent))
    return false;
  auto* action = *setting;
  auto  before = settings_pages_binding::Children(parent);
  if (!before || before->size() > PageCatalog::NativeChildLimit)
    return false;
  std::vector<size_t> missing;
  const auto          count = action->Count();
  for (size_t index = 0; index < count; ++index) {
    const bool exists = std::ranges::any_of(*before, [&](const auto& child) {
      auto row = ActionRow(child.template Get<SettingsOptionContext>());
      return row && row->action == action && row->index == index;
    });
    if (!exists)
      missing.push_back(index);
    if (before->size() + missing.size() > PageCatalog::NativeChildLimit)
      return false;
  }
  for (const auto index : missing) {
    before = settings_pages_binding::Children(parent);
    if (!before || before->size() >= PageCatalog::NativeChildLimit)
      return false;
    const auto method = SettingsActionOwner::AddMethod();
    auto       token  = il2cpp::GcHandle::Strong(il2cpp::Allocate<SettingsActionToken>());
    auto       click  = il2cpp::GcHandle::Strong(
        SettingsActionOwner::DelegateFor(method, 3, token.Get<il2cpp::Object>(), Actions().click.method()));
    auto text =
        il2cpp::GcHandle::Strong(SettingsActionOwner::DelegateFor(method, 4, director, Actions().text.method()));
    const auto query_method =
        SettingsOptionContext::Class::Method<void(settings_binding::ReferenceDelegate*)>("set_QueryOptionState");
    auto query =
        il2cpp::GcHandle::Strong(SettingsActionOwner::DelegateFor(query_method, 0, director, Actions().query.method()));
    auto label = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(action->item_id(index))));
    auto empty = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString("")));
    if (!token || !click || !text || !query || !label || !empty)
      return false;
    auto plan = std::make_shared<SettingsActionPlan>(SettingsActionPlan{
        il2cpp::GcHandle::Weak(parent), il2cpp::GcHandle::Weak(click.Get()), il2cpp::GcHandle::Weak(query.Get()),
        il2cpp::GcHandle::Weak(token.Get()), action, index, Actions().epoch});
    if (!plan->parent || !plan->click || !plan->query || !plan->token)
      return false;
    Actions().plans.insert_or_assign(query.Get(), plan);
    // Action custody is the unique click delegate; pass that identity in both generic callback slots.
    const auto receipt = Actions().insertions.Begin(parent, click.Get(), click.Get(), query.Get(), std::move(*before));
    if (!receipt) {
      Actions().plans.erase(query.Get());
      return false;
    }
    struct Completion {
      uint64_t receipt;
      bool     completed = false;
      ~Completion()
      {
        if (!completed)
          Actions().insertions.Complete(receipt, false);
      }
    } completion{receipt};
    const auto added = context->TryCall<"AddButtonAndText", void(SettingsContainerContext*, Il2CppString*,
                                                                 Il2CppString*, settings_binding::ReferenceDelegate*,
                                                                 settings_binding::ReferenceDelegate*, Il2CppString*)>(
        parent, reinterpret_cast<Il2CppString*>(label.Get()), reinterpret_cast<Il2CppString*>(empty.Get()),
        click.Get<settings_binding::ReferenceDelegate>(), text.Get<settings_binding::ReferenceDelegate>(),
        reinterpret_cast<Il2CppString*>(empty.Get()));
    Actions().insertions.Complete(receipt, added.succeeded);
    completion.completed = true;
    if (!RetryActionInsertions(parent))
      return false;
    const auto after = settings_pages_binding::Children(parent);
    if (!after || std::ranges::none_of(*after, [&](const auto& child) {
          auto row = ActionRow(child.template Get<SettingsOptionContext>());
          return row && row->action == action && row->index == index;
        }))
      return false;
  }
  return true;
}

std::optional<PageItemIdentity> ResolveAction(SettingsOptionContext* context)
{
  auto row = ActionsThread() ? ActionRow(context) : nullptr;
  if (!row)
    return {};
  const auto action  = row->action;
  const auto index   = row->index;
  bool       visible = false;
  try {
    visible = action->Read(index).visible && index < action->Count();
  } catch (...) {
    ActionWarning();
  }
  row = ActionRow(context);
  return row && row->action == action && row->index == index
             ? std::optional<PageItemIdentity>(PageItemIdentity{action->id(), index, visible})
             : std::nullopt;
}

bool RestoreAction(const std::shared_ptr<SettingsActionView>& view)
{
  auto  widget_root = il2cpp::GcHandle::Strong(view->widget.Get());
  auto* widget      = widget_root.Get<SettingsActionWidget>();
  if (!widget)
    return true;
  const auto current = widget->TryCall<"get_Context", SettingsActionContext*>();
  if (!current)
    return false;
  if (current.value && current.value != view->context.Get())
    return true; // A pooled replacement owns the component now.
  bool restored = true;
  for (size_t index = 0; index < view->labels.size(); ++index) {
    if (!view->label_owned[index])
      continue;
    auto       root  = il2cpp::GcHandle::Strong(view->labels[index].Get());
    auto*      label = root.Get<TextLocalizer>();
    const auto alive = label ? unity::TryAlive(label) : il2cpp::CallResult<bool>{true, false};
    if (!alive) {
      restored = false;
      continue;
    }
    if (!alive.value)
      continue;
    const auto text = label->TryCall<"CurrentText", Il2CppString*>();
    if (!text) {
      restored = false;
      continue;
    }
    if (il2cpp::ToUtf8(text.value) == view->applied[index])
      restored = (label->TryCall<"ClearTextOverride", void>().succeeded && !label->textOverride()) && restored;
  }
  auto  button_root = il2cpp::GcHandle::Strong(view->button.Get());
  auto* button      = button_root.Get<SettingsActionButton>();
  if (button && view->interactable) {
    const auto value = button->TryInteractable();
    if (!value)
      restored = false;
    else if (value.value == *view->interactable)
      restored = button->TryClear().succeeded && restored;
  }
  auto  object_root = il2cpp::GcHandle::Strong(view->button_object.Get());
  auto* object      = object_root.Get<unity::GameObject>();
  if (object && view->hidden_before) {
    const auto value = object->TryCall<"get_activeSelf", bool>();
    if (!value)
      restored = false;
    else if (!value.value) {
      const auto written  = object->TryCall<"SetActive", void(bool)>(*view->hidden_before);
      const auto observed = object->TryCall<"get_activeSelf", bool>();
      restored            = written && observed && observed.value == *view->hidden_before && restored;
    }
  }
  return restored;
}
void ReleaseAction(SettingsActionWidget* widget) noexcept
{
  auto view = ActionView(widget);
  if (!view)
    return;
  view->revoked = true;
  view->ready   = false;
  if (view->restoring)
    return;
  view->restoring = true;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{view->restoring};
  try {
    if (RestoreAction(view) && ActionView(widget) == view)
      Actions().views.erase(widget);
  } catch (...) {
    ActionWarning();
  }
}

void RenderAction(SettingsActionWidget* widget, const std::shared_ptr<SettingsActionView>& view)
{
  if (!SameAction(widget, view) || view->rendering || view->invoking)
    return;
  view->rendering = true;
  view->ready     = false;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{view->rendering};
  const auto presentation = view->action->Read(view->index);
  if (!SameAction(widget, view))
    return;
  const std::array text{presentation.label, presentation.button, presentation.value};
  for (size_t index = 0; index < text.size(); ++index) {
    auto  root  = il2cpp::GcHandle::Strong(view->labels[index].Get());
    auto* label = root.Get<TextLocalizer>();
    if (!label)
      return;
    const auto before = label->TryCall<"CurrentText", Il2CppString*>();
    if (!SameAction(widget, view))
      return;
    if (view->label_owned[index] && view->applied[index] == text[index] && before
        && il2cpp::ToUtf8(before.value) == text[index] && label->textOverride())
      continue;
    view->applied[index]     = text[index];
    view->label_owned[index] = true;
    if (!label->TryCall<"OverrideLocalizedText", void(Il2CppString*)>(il2cpp::NewString(text[index])))
      return;
    const auto observed = label->TryCall<"CurrentText", Il2CppString*>();
    if (!observed || il2cpp::ToUtf8(observed.value) != text[index])
      return;
    if (!SameAction(widget, view))
      return;
  }
  auto  button_root = il2cpp::GcHandle::Strong(view->button.Get());
  auto* button      = button_root.Get<SettingsActionButton>();
  auto  object_root = il2cpp::GcHandle::Strong(view->button_object.Get());
  auto* object      = object_root.Get<unity::GameObject>();
  if (!button || !object)
    return;
  if (view->hidden_before && !presentation.button.empty()) {
    if (!object->TryCall<"SetActive", void(bool)>(*view->hidden_before))
      return;
    view->hidden_before.reset();
  } else if (!view->hidden_before && presentation.button.empty()) {
    const auto value = object->TryCall<"get_activeSelf", bool>();
    if (!value)
      return;
    view->hidden_before = value.value;
    if (!object->TryCall<"SetActive", void(bool)>(false))
      return;
  }
  if (!SameAction(widget, view))
    return;
  const bool enabled  = presentation.actionable() && bool(view->action->invoke)
                        && (!Shortcut::CaptureBlocksShortcuts() || AllowsSettingsSearchAction(view->action->id()));
  const auto previous = button->TryInteractable();
  if (!SameAction(widget, view))
    return;
  const bool apply   = !view->interactable || *view->interactable != enabled || !previous || previous.value != enabled;
  view->interactable = enabled;
  if (apply && !button->TryOverride(enabled))
    return;
  const auto observed = button->TryInteractable();
  if (!observed || observed.value != enabled)
    ActionWarning();
  else
    view->ready = SameAction(widget, view);
}
void SettingsActionWidget_SetWidgetData_Hook(auto original, SettingsActionWidget* _this)
{
  if (ActionsThread())
    ReleaseAction(_this);
  original(_this);
  if (!ActionsThread() || ActionView(_this))
    return;
  try {
    auto context_root = il2cpp::GcHandle::Strong(_this->Context());
    auto row          = ActionRow(context_root.Get<SettingsOptionContext>());
    if (!row)
      return;
    auto view     = std::make_shared<SettingsActionView>();
    view->widget  = il2cpp::GcHandle::Weak(_this);
    view->context = il2cpp::GcHandle::Weak(context_root.Get());
    view->token   = il2cpp::GcHandle::Weak(row->token.Get());
    view->action  = row->action;
    view->index   = row->index;
    view->visit   = CurrentPageVisit();
    const std::array labels{_this->Label(), _this->ButtonLabel(), _this->ValueLabel()};
    for (size_t i = 0; i < labels.size(); ++i)
      if (!(view->labels[i] = il2cpp::GcHandle::Weak(labels[i])))
        return;
    auto       button = il2cpp::GcHandle::Strong(_this->Button());
    const auto object =
        button ? button.Get<SettingsActionButton>()->TryObject() : il2cpp::CallResult<unity::GameObject*>{};
    view->button        = il2cpp::GcHandle::Weak(button.Get());
    view->button_object = il2cpp::GcHandle::Weak(object ? object.value : nullptr);
    if (!view->widget || !view->context || !view->token || !view->button || !view->button_object
        || !IsPageVisitCurrent(view->visit) || _this->Context() != context_root.Get())
      return;
    Actions().views.insert_or_assign(_this, view);
    RenderAction(_this, view);
  } catch (...) {
    ReleaseAction(_this);
    ActionWarning();
  }
}
void SettingsActionWidget_OnAboutToReleaseContext_Hook(auto original, SettingsActionWidget* _this)
{
  if (ActionsThread())
    ReleaseAction(_this);
  original(_this);
}
void InvokeAction(Il2CppObject* token, const MethodInfo*)
{
  if (!ActionsThread() || PageRefreshInProgress())
    return;
  std::vector<std::shared_ptr<SettingsActionView>> views;
  for (const auto& [widget, view] : Actions().views)
    if (view->token.Get() == token)
      views.push_back(view);
  for (const auto& view : views) {
    auto  root   = il2cpp::GcHandle::Strong(view->widget.Get());
    auto* widget = root.Get<SettingsActionWidget>();
    if (!widget || !SameAction(widget, view) || !view->ready || view->rendering || view->invoking)
      continue;
    if (Shortcut::CaptureBlocksShortcuts() && !AllowsSettingsSearchAction(view->action->id()))
      continue;
    view->invoking = true;
    struct Done {
      bool& flag;
      ~Done()
      { flag = false; }
    } done{view->invoking};
    try {
      const auto presentation = view->action->Read(view->index);
      const bool enabled = presentation.actionable()
                           && view->applied == std::array{presentation.label, presentation.button, presentation.value};
      if (enabled && view->action->invoke && SameAction(widget, view)
          && (!Shortcut::CaptureBlocksShortcuts() || AllowsSettingsSearchAction(view->action->id()))) {
        const auto previous = std::exchange(invoking_action_widget, widget);
        struct Invocation {
          il2cpp::Object* previous;
          ~Invocation()
          { invoking_action_widget = previous; }
        } invoking{previous};
        view->action->invoke(view->index);
        RequestPageRefresh();
      }
    } catch (...) {
      ActionWarning();
    }
    return;
  }
}
void InvalidateActions()
{
  ++Actions().epoch;
  std::vector<SettingsActionWidget*> widgets;
  for (const auto& [widget, view] : Actions().views)
    widgets.push_back(widget);
  for (auto* widget : widgets)
    ReleaseAction(widget);
  Actions().rows.clear();
}
void ActionsFrame()
{
  if (!ActionsThread())
    return;
  static Throttle lifecycle(std::chrono::milliseconds(250));
  if (!lifecycle.Due())
    return;
  ActionInsertionHost host;
  Actions().insertions.Retry(host);
  Actions().quarantine.Retry(host);
  std::erase_if(Actions().rows, [](const auto& entry) { return !entry.second->context.Get(); });
  std::erase_if(Actions().plans, [](const auto& entry) { return !entry.second->query.Get(); });
  std::vector<std::shared_ptr<SettingsActionView>> views;
  for (const auto& [widget, view] : Actions().views)
    views.push_back(view);
  for (const auto& view : views) {
    auto  root   = il2cpp::GcHandle::Strong(view->widget.Get());
    auto* widget = root.Get<SettingsActionWidget>();
    if (!widget) {
      std::erase_if(Actions().views, [view](const auto& entry) { return entry.second == view; });
      continue;
    }
    try {
      if (!SameAction(widget, view))
        ReleaseAction(widget);
      else
        RenderAction(widget, view);
    } catch (...) {
      ReleaseAction(widget);
      ActionWarning();
    }
  }
}
bool InstallSettingsActions()
{
  auto&      state   = Actions();
  const auto refresh = SettingsActionWidget::Class::Method<void()>("SetWidgetData");
  const auto release = SettingsActionWidget::Class::Method<void()>("OnAboutToReleaseContext");
  if (!SettingsActionOwner::AddMethod() || !SettingsActionWidget::SurfaceAvailable()
      || !SettingsSectionDirector::QualifiedQueryState() || !refresh || !release
      || refresh.Info()->klass != SettingsActionWidget::Class::Raw()
      || release.Info()->klass != SettingsActionWidget::Class::Raw() || refresh.Pointer() == release.Pointer()
      || !state.click.Initialize(SettingsActionToken::Class::Method<void()>(".ctor").Info(), InvokeAction)
      || !state.text.Initialize(SettingsSectionDirector::Class::Method<Il2CppString*()>("GetClientVersion").Info(),
                                ActionEmptyText)
      || !state.query.Initialize(
          SettingsSectionDirector::Class::Method<int()>("QueryShouldShowGenericPcSetting").Info(), ActionQuery)) {
    spdlog::warn("[NativeSettingsActions] native command row contract unavailable");
    return false;
  }
  il2cpp::HookSet hooks("NativeSettingsActions");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(refresh), SettingsActionWidget_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(release), SettingsActionWidget_OnAboutToReleaseContext_Hook);
  if (!hooks.Commit())
    return false;
  state.thread = std::this_thread::get_id();
  state.active = RegisterPageItemAdapter(PageItemKind::Action, AppendAction, ResolveAction, InvalidateActions);
  return state.active;
}
} // namespace

il2cpp::Object* mod_settings::native::InvokingActionWidget()
{ return invoking_action_widget; }
void mod_settings::native::RefreshActions()
{ RequestPageRefresh(); }
const Feature kNativeSettingsActions{"NativeSettingsActions", &Config::Patches::nativesettingshooks,
                                     InstallSettingsActions, ActionsFrame};
