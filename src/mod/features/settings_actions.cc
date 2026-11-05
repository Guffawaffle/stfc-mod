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
struct SettingsActionRebind {
  std::weak_ptr<SettingsActionRow> row;
  PageVisitToken                   visit    = 0;
  uint64_t                         epoch    = 0;
  bool                             prepared = false;
};
struct SettingsActionView {
  il2cpp::GcHandle                      widget, context, token, button, button_object;
  std::array<il2cpp::GcHandle, 3>       labels;
  std::array<std::string, 3>            applied;
  std::array<bool, 3>                   label_owned{};
  ActionSetting*                        action = nullptr;
  size_t                                index  = 0;
  PageVisitToken                        visit  = 0;
  std::optional<bool>                   interactable, hidden_before;
  std::shared_ptr<SettingsActionRebind> rebind;
  bool                                  cleanup_complete = false;
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
  if (!row || context.value != view->context.Get() || row->token.Get() != view->token.Get()
      || row->action != view->action || row->index != view->index)
    return false;
  // Managed ownership reads may replace a context or depart its page. Recheck
  // the native context and both registrations after those observations.
  const auto checked   = widget->TryCall<"get_Context", SettingsActionContext*>();
  const auto confirmed = Actions().rows.find(context.value);
  return checked && checked.value == context.value && confirmed != Actions().rows.end() && confirmed->second == row
         && !view->revoked && IsPageVisitCurrent(view->visit) && view->context.Get() == context.value
         && view->widget.Get() == widget && ActionView(widget) == view;
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
  // Cleanup may outlive the page visit, but must still own this exact widget.
  // Unknown observations retain custody for retry; a confirmed replacement ends it.
  const auto current = [&]() -> std::optional<bool> {
    if (ActionView(widget) != view || view->widget.Get() != widget)
      return false;
    const auto context = widget->TryCall<"get_Context", SettingsActionContext*>();
    if (!context)
      return {};
    return (!context.value || context.value == view->context.Get()) && ActionView(widget) == view
           && view->widget.Get() == widget;
  };
  auto same = current();
  if (!same || !*same)
    return same.has_value();
  const auto label_at = [&](size_t index) {
    return index == 0 ? widget->Label() : index == 1 ? widget->ButtonLabel() : widget->ValueLabel();
  };
  bool restored = true;
  for (size_t index = 0; index < view->labels.size(); ++index) {
    if (!view->label_owned[index])
      continue;
    auto  root  = il2cpp::GcHandle::Strong(view->labels[index].Get());
    auto* label = root.Get<TextLocalizer>();
    if (!label || label_at(index) != label) {
      view->label_owned[index] = false;
      continue;
    }
    const auto alive = unity::TryAlive(label);
    same             = current();
    if (!same || !*same)
      return same.has_value();
    if (!alive) {
      restored = false;
      continue;
    }
    if (!alive.value) {
      view->label_owned[index] = false;
      continue;
    }
    const auto text = label->TryCall<"CurrentText", Il2CppString*>();
    same            = current();
    if (!same || !*same)
      return same.has_value();
    if (label_at(index) != label) {
      view->label_owned[index] = false;
      continue;
    }
    if (!text) {
      restored = false;
      continue;
    }
    if (il2cpp::ToUtf8(text.value) != view->applied[index]) {
      view->label_owned[index] = false;
      continue;
    }
    if (label->TryCall<"ClearTextOverride", void>() && !label->textOverride())
      view->label_owned[index] = false;
    else
      restored = false;
  }
  same = current();
  if (!same || !*same)
    return same.has_value();
  auto  button_root = il2cpp::GcHandle::Strong(view->button.Get());
  auto* button      = button_root.Get<SettingsActionButton>();
  if (!button || widget->Button() != button) {
    view->interactable.reset();
    view->hidden_before.reset();
    return restored;
  }
  if (view->interactable) {
    const auto value = button->TryInteractable();
    same             = current();
    if (!same || !*same)
      return same.has_value();
    if (widget->Button() != button) {
      view->interactable.reset();
      view->hidden_before.reset();
      return restored;
    }
    if (!value) {
      restored = false;
    } else if (value.value != *view->interactable || button->TryClear()) {
      view->interactable.reset();
    } else {
      restored = false;
    }
  }
  auto  object_root = il2cpp::GcHandle::Strong(view->button_object.Get());
  auto* object      = object_root.Get<unity::GameObject>();
  if (object && view->hidden_before) {
    const auto attached = button->TryObject();
    const auto value    = object->TryCall<"get_activeSelf", bool>();
    same                = current();
    if (!same || !*same)
      return same.has_value();
    if (widget->Button() != button || (attached && attached.value != object)) {
      view->hidden_before.reset();
    } else if (!attached || !value) {
      restored = false;
    } else if (!value.value) {
      const auto written  = object->TryCall<"SetActive", void(bool)>(*view->hidden_before);
      const auto observed = object->TryCall<"get_activeSelf", bool>();
      if (written && observed && observed.value == *view->hidden_before)
        view->hidden_before.reset();
      else
        restored = false;
    } else {
      view->hidden_before.reset();
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
  if (view->restoring || view->rendering || view->invoking)
    return;
  view->restoring = true;
  struct Done {
    bool& flag;
    ~Done()
    { flag = false; }
  } done{view->restoring};
  try {
    if (!view->cleanup_complete)
      view->cleanup_complete = RestoreAction(view);
    if (view->cleanup_complete && !view->rebind && ActionView(widget) == view)
      Actions().views.erase(widget);
  } catch (...) {
    ActionWarning();
  }
}

void RebindActionPresentation(SettingsActionWidget* widget, const std::shared_ptr<SettingsActionView>& view)
{
  if (ActionView(widget) != view || view->revoked)
    return;
  const auto found = Actions().rows.find(view->context.Get<SettingsOptionContext>());
  if (found != Actions().rows.end() && found->second->action == view->action && found->second->index == view->index
      && found->second->token.Get() == view->token.Get() && IsPageVisitCurrent(view->visit)) {
    auto request      = std::make_shared<SettingsActionRebind>();
    request->row      = found->second;
    request->visit    = view->visit;
    request->epoch    = Actions().epoch;
    request->prepared = true;
    view->rebind      = std::move(request);
  }
  ReleaseAction(widget);
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
  const auto current = [&] {
    if (!SameAction(widget, view))
      return false;
    if (widget->Label() == view->labels[0].Get() && widget->ButtonLabel() == view->labels[1].Get()
        && widget->ValueLabel() == view->labels[2].Get() && widget->Button() == view->button.Get())
      return true;
    RebindActionPresentation(widget, view);
    return false;
  };
  const auto presentation = view->action->Read(view->index);
  if (!current())
    return;
  const std::array text{presentation.label, presentation.button, presentation.value};
  for (size_t index = 0; index < text.size(); ++index) {
    auto  root  = il2cpp::GcHandle::Strong(view->labels[index].Get());
    auto* label = root.Get<TextLocalizer>();
    if (!label)
      return;
    const auto alive = unity::TryAlive(label);
    if (!current() || !alive || !alive.value)
      return;
    const auto before = label->TryCall<"CurrentText", Il2CppString*>();
    if (!current())
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
    if (!current())
      return;
  }
  auto  button_root = il2cpp::GcHandle::Strong(view->button.Get());
  auto* button      = button_root.Get<SettingsActionButton>();
  auto  object_root = il2cpp::GcHandle::Strong(view->button_object.Get());
  auto* object      = object_root.Get<unity::GameObject>();
  if (!button || !object)
    return;
  const auto attached = [&] {
    if (!current())
      return false;
    const auto observed = button->TryObject();
    if (!current() || !observed)
      return false;
    const auto confirmed = button->TryObject();
    if (!current() || !confirmed)
      return false;
    if (observed.value == object && confirmed.value == object)
      return true;
    RebindActionPresentation(widget, view);
    return false;
  };
  if (!attached())
    return;
  if (view->hidden_before && !presentation.button.empty()) {
    if (!object->TryCall<"SetActive", void(bool)>(*view->hidden_before))
      return;
    if (!attached())
      return;
    view->hidden_before.reset();
  } else if (!view->hidden_before && presentation.button.empty()) {
    const auto value = object->TryCall<"get_activeSelf", bool>();
    if (!attached() || !value)
      return;
    view->hidden_before = value.value;
    if (!object->TryCall<"SetActive", void(bool)>(false))
      return;
  }
  if (!attached())
    return;
  const bool enabled  = presentation.actionable() && bool(view->action->invoke)
                        && (!Shortcut::CaptureBlocksShortcuts() || AllowsSettingsSearchAction(view->action->id()));
  const auto previous = button->TryInteractable();
  if (!attached())
    return;
  const bool apply   = !view->interactable || *view->interactable != enabled || !previous || previous.value != enabled;
  view->interactable = enabled;
  if (apply && !button->TryOverride(enabled))
    return;
  const auto observed = button->TryInteractable();
  if (!observed || observed.value != enabled)
    ActionWarning();
  else
    view->ready = attached();
}
void CancelActionRebind(SettingsActionWidget* widget, const std::shared_ptr<SettingsActionView>& retired,
                        const std::shared_ptr<SettingsActionRebind>& request)
{
  if (ActionView(widget) != retired || retired->rebind != request)
    return;
  retired->rebind.reset();
  ReleaseAction(widget);
}
void ResumeActionBind(SettingsActionWidget* widget, const std::shared_ptr<SettingsActionView>& retired)
{
  const auto request = retired->rebind;
  if (!request || !request->prepared || !retired->cleanup_complete || retired->restoring || retired->rendering
      || retired->invoking || ActionView(widget) != retired)
    return;
  const auto row     = request->row.lock();
  const auto current = [&] {
    if (!row || ActionView(widget) != retired || retired->rebind != request || !retired->revoked
        || request->epoch != Actions().epoch || !IsPageVisitCurrent(request->visit))
      return false;
    auto*      context = row->context.Get<SettingsOptionContext>();
    const auto found   = Actions().rows.find(context);
    return context && found != Actions().rows.end() && found->second == row;
  };
  if (!current()) {
    CancelActionRebind(widget, retired, request);
    return;
  }
  auto       context_root = il2cpp::GcHandle::Strong(row->context.Get());
  const auto context      = widget->TryCall<"get_Context", SettingsActionContext*>();
  if (!context)
    return; // Unknown observations retain the prepared request for the next frame.
  if (context.value != context_root.Get() || !current()) {
    CancelActionRebind(widget, retired, request);
    return;
  }
  if (ActionRow(context_root.Get<SettingsOptionContext>()) != row)
    return;
  auto view     = std::make_shared<SettingsActionView>();
  view->widget  = il2cpp::GcHandle::Weak(widget);
  view->context = il2cpp::GcHandle::Weak(context_root.Get());
  view->token   = il2cpp::GcHandle::Weak(row->token.Get());
  view->action  = row->action;
  view->index   = row->index;
  view->visit   = request->visit;
  const std::array labels{widget->Label(), widget->ButtonLabel(), widget->ValueLabel()};
  for (size_t i = 0; i < labels.size(); ++i)
    if (!(view->labels[i] = il2cpp::GcHandle::Weak(labels[i])))
      return;
  auto       button = il2cpp::GcHandle::Strong(widget->Button());
  const auto object =
      button ? button.Get<SettingsActionButton>()->TryObject() : il2cpp::CallResult<unity::GameObject*>{};
  view->button        = il2cpp::GcHandle::Weak(button.Get());
  view->button_object = il2cpp::GcHandle::Weak(object ? object.value : nullptr);
  const auto checked  = widget->TryCall<"get_Context", SettingsActionContext*>();
  if (!checked)
    return;
  if (checked.value != context_root.Get() || !current()) {
    CancelActionRebind(widget, retired, request);
    return;
  }
  if (!view->widget || !view->context || !view->token || !view->button || !view->button_object
      || widget->Label() != labels[0] || widget->ButtonLabel() != labels[1] || widget->ValueLabel() != labels[2]
      || widget->Button() != button.Get())
    return;
  retired->rebind.reset();
  Actions().views.insert_or_assign(widget, view);
  try {
    RenderAction(widget, view);
  } catch (...) {
    if (ActionView(widget) == view)
      ReleaseAction(widget);
    throw;
  }
}
void SettingsActionWidget_SetWidgetData_Hook(auto original, SettingsActionWidget* _this)
{
  std::shared_ptr<SettingsActionView>   retired;
  std::shared_ptr<SettingsActionRebind> request;
  if (ActionsThread()) {
    try {
      retired = ActionView(_this);
      if (!retired) {
        // An inert reservation gives first binds the same nested-bind ordering
        // as a row whose old presentation still needs cleanup.
        retired                   = std::make_shared<SettingsActionView>();
        retired->widget           = il2cpp::GcHandle::Weak(_this);
        retired->cleanup_complete = true;
        Actions().views.emplace(_this, retired);
      }
      request        = std::make_shared<SettingsActionRebind>();
      request->visit = CurrentPageVisit();
      request->epoch = Actions().epoch;
      // Reserve before any managed cleanup/native bind: a nested bind supersedes this request.
      retired->rebind = request;
      ReleaseAction(_this);
    } catch (...) {
      if (retired)
        CancelActionRebind(_this, retired, request);
      ActionWarning();
    }
  }
  struct Completion {
    SettingsActionWidget*                 widget;
    std::shared_ptr<SettingsActionView>   retired;
    std::shared_ptr<SettingsActionRebind> request;
    bool                                  returned = false;
    ~Completion()
    {
      if (!returned && retired)
        CancelActionRebind(widget, retired, request);
    }
  } completion{_this, retired, request};
  original(_this);
  completion.returned = true;
  if (!ActionsThread() || !retired || !request || ActionView(_this) != retired || retired->rebind != request)
    return;
  try {
    const auto context      = _this->TryCall<"get_Context", SettingsActionContext*>();
    auto       context_root = il2cpp::GcHandle::Strong(context ? context.value : nullptr);
    auto       row          = ActionRow(context_root.Get<SettingsOptionContext>());
    if (!row) {
      CancelActionRebind(_this, retired, request);
      return;
    }
    request->row      = row;
    request->prepared = true;
    ResumeActionBind(_this, retired);
  } catch (...) {
    CancelActionRebind(_this, retired, request);
    ActionWarning();
  }
}
void SettingsActionWidget_OnAboutToReleaseContext_Hook(auto original, SettingsActionWidget* _this)
{
  if (ActionsThread()) {
    if (auto view = ActionView(_this))
      view->rebind.reset();
    ReleaseAction(_this);
  }
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
  for (auto* widget : widgets) {
    if (auto view = ActionView(widget))
      view->rebind.reset();
    ReleaseAction(widget);
  }
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
      if (!SameAction(widget, view)) {
        if (ActionView(widget) != view)
          continue;
        ReleaseAction(widget);
        ResumeActionBind(widget, view);
      } else
        RenderAction(widget, view);
    } catch (...) {
      if (ActionView(widget) == view)
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
