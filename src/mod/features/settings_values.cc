// Native choice/slider rows reuse the existing models and the page owner's off-screen publication transaction.
// No settings writes occur during native binding, refresh or disposal.
#include "game/settings_values.h"
#include "features/features.h"
#include "runtime_config.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_controls.h"
#include "settings/native_view_state.h"
#include <algorithm>
#include <array>
#include <format>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using namespace mod_settings;
using namespace mod_settings::native;
struct SettingsValueRow {
  il2cpp::GcHandle context, get, set, query;
  ChoiceSetting*   choice = nullptr;
  SliderSetting*   slider = nullptr;
  int              index  = 0;
};
struct SettingsValueView {
  il2cpp::GcHandle               widget, context, label, value_label;
  il2cpp::GcHandle               control, selection_group;
  std::optional<bool>            interactable_before;
  bool                           group_detached = false, interactable_applied = false, last_interactable = false;
  std::optional<NativeViewState> state;
  ChoiceSetting*                 choice = nullptr;
  SliderSetting*                 slider = nullptr;
  PageVisitToken                 visit  = 0;
  std::array<uint64_t, 3>        version{};
  std::string                    label_text, value_text;
  bool                           observed = false, label_applied = false, value_applied = false;
  bool                           rendering = false, requesting = false, clearing = false, revoked = false;
};
struct SettingsValuesState {
  std::map<SettingsOptionContext*, SettingsValueRow>            rows;
  std::map<il2cpp::Object*, std::shared_ptr<SettingsValueView>> views;
  NativeCallback<int>                                           choice_get, query;
  NativeCallback<void, int>                                     choice_set;
  NativeCallback<float>                                         slider_get;
  NativeCallback<void, float>                                   slider_set;
  std::thread::id                                               thread;
  bool                                                          active = false;
};
SettingsValuesState& ValuesState()
{
  static auto* state = new SettingsValuesState;
  return *state;
}
thread_local SettingsValueView* settings_value_rendering = nullptr;
template <class Native> void    ValueOriginal(Native&& native, SettingsValueView* view = nullptr)
{
  auto* previous = std::exchange(settings_value_rendering, view);
  struct Restore {
    SettingsValueView* previous;
    ~Restore()
    { settings_value_rendering = previous; }
  } restore{previous};
  std::forward<Native>(native)();
}
bool ValuesThread()
{ return ValuesState().active && ValuesState().thread == std::this_thread::get_id(); }
int ValueChoiceGet(Il2CppObject*, const MethodInfo*)
{
  return settings_value_rendering && settings_value_rendering->state ? settings_value_rendering->state->selected() : -1;
}
void  ValueChoiceSet(Il2CppObject*, int, const MethodInfo*) {}
float ValueSliderGet(Il2CppObject*, const MethodInfo*)
{
  return settings_value_rendering && settings_value_rendering->state ? settings_value_rendering->state->number() : 0.0f;
}
void ValueSliderSet(Il2CppObject*, float, const MethodInfo*) {}
int  ValueQuery(Il2CppObject*, const MethodInfo*)
{ return 0; }

std::optional<bool> ValueOwnership(SettingsOptionContext* context)
{
  auto&      rows  = ValuesState().rows;
  const auto found = rows.find(context);
  if (found == rows.end() || found->second.context.Get() != context)
    return false;
  auto&      row       = found->second;
  const auto query     = settings_pages_binding::Query(context);
  const auto callbacks = settings_values_binding::Callbacks(context, row.get.Get(), row.set.Get());
  if (!query || !callbacks)
    return {};
  return query.value && query.value == row.query.Get() && *callbacks;
}
SettingsValueRow* ValueRow(SettingsOptionContext* context)
{
  const auto owned = ValueOwnership(context);
  if (!owned || !*owned)
    return nullptr;
  const auto found = ValuesState().rows.find(context);
  return found != ValuesState().rows.end() && found->second.context.Get() == context ? &found->second : nullptr;
}
std::optional<PageItemIdentity> ValueIdentity(SettingsOptionContext* context)
{
  if (auto* row = ValueRow(context))
    return PageItemIdentity{row->choice ? row->choice->state().id() : row->slider->state().id(),
                            static_cast<size_t>(row->index)};
  return {};
}
std::shared_ptr<SettingsValueView> ValueView(il2cpp::Object* widget)
{
  const auto found = ValuesState().views.find(widget);
  return found == ValuesState().views.end() ? nullptr : found->second;
}
bool SameValue(il2cpp::Object* widget, const std::shared_ptr<SettingsValueView>& view)
{
  if (!view || view->revoked || !IsPageVisitCurrent(view->visit))
    return false;
  const auto current = widget->TryCall<"get_Context", SettingsOptionContext*>();
  return current && current.value == view->context.Get() && view->widget.Get() == widget && ValueView(widget) == view
         && ValueRow(current.value);
}
TextLocalizer* CurrentValueLabel(il2cpp::Object* widget, bool slider, bool number)
{
  if (slider) {
    auto* typed = widget->As<SettingsSliderWidget>();
    return typed ? (number ? typed->ValueLabel() : typed->Label()) : nullptr;
  }
  auto* typed = widget->As<SettingsSelectionWidget>();
  return typed && !number ? typed->Label() : nullptr;
}
SettingsValueControl* CurrentValueControl(il2cpp::Object* widget, bool slider)
{
  if (slider) {
    auto* typed = widget->As<SettingsSliderWidget>();
    return typed ? reinterpret_cast<SettingsValueControl*>(typed->Slider()) : nullptr;
  }
  auto* typed = widget->As<SettingsSelectionWidget>();
  return typed ? reinterpret_cast<SettingsValueControl*>(typed->Toggle()) : nullptr;
}
bool ReleaseValue(il2cpp::Object* widget)
{
  const auto view = ValueView(widget);
  if (!view)
    return true;
  view->revoked = true;
  if (view->state)
    view->state->Invalidate();
  if (view->rendering || view->requesting || view->clearing)
    return false;
  view->clearing = true;
  struct Clearing {
    bool& flag;
    ~Clearing()
    { flag = false; }
  } clearing{view->clearing};
  const auto current = widget->TryCall<"get_Context", SettingsOptionContext*>();
  if (!current)
    return false;
  const bool same      = !current.value || current.value == view->context.Get();
  const auto ownership = current.value ? ValueOwnership(current.value) : std::optional<bool>(true);
  if (same && !ownership)
    return false;
  bool complete = true;
  // Clear only our own override on the same live component. A pooled replacement's text is not ours to restore.
  if (same && ownership.value_or(false)) {
    // PLAY restores the detached ToggleGroup and the original interactability
    // before native pooling. Restore only the same component and our overrides.
    auto  control_root = il2cpp::GcHandle::Strong(view->control.Get());
    auto* control      = control_root.Get<SettingsValueControl>();
    if (control && CurrentValueControl(widget, view->slider != nullptr) == control) {
      if (view->group_detached) {
        auto*      toggle        = reinterpret_cast<unity::Toggle*>(control);
        const auto current_group = toggle->TryCall<"get_group", SettingsToggleGroup*>();
        if (!current_group) {
          complete = false;
        } else if (current_group.value) {
          view->group_detached = false; // A later owner has supplied a group.
        } else {
          auto       group = il2cpp::GcHandle::Strong(view->selection_group.Get());
          const auto restored =
              toggle->TryCall<"set_group", void(SettingsToggleGroup*)>(group.Get<SettingsToggleGroup>());
          const auto observed = toggle->TryCall<"get_group", SettingsToggleGroup*>();
          if (restored && observed && observed.value == group.Get())
            view->group_detached = false;
          else
            complete = false;
        }
      }
      if (view->interactable_applied) {
        const auto observed = control->TryCall<"get_interactable", bool>();
        if (!observed) {
          complete = false;
        } else if (observed.value != view->last_interactable) {
          view->interactable_applied = false;
        } else {
          const auto restored = control->TryCall<"set_interactable", void(bool)>(*view->interactable_before);
          const auto checked  = control->TryCall<"get_interactable", bool>();
          if (restored && checked && checked.value == *view->interactable_before)
            view->interactable_applied = false;
          else
            complete = false;
        }
      }
    }
    for (bool number : {false, true}) {
      auto& applied = number ? view->value_applied : view->label_applied;
      if (!applied)
        continue;
      auto* token      = (number ? view->value_label : view->label).Get();
      auto  label_root = il2cpp::GcHandle::Strong(token);
      auto* label      = label_root.Get<TextLocalizer>();
      if (token && !label_root) {
        complete = false;
        continue;
      }
      const auto alive = label ? unity::TryAlive(label) : il2cpp::CallResult<bool>{true, false};
      if (!alive) {
        complete = false;
        continue;
      }
      if (!alive.value || CurrentValueLabel(widget, view->slider != nullptr, number) != label) {
        applied = false;
        continue;
      }
      const auto text = label->TryCall<"CurrentText", Il2CppString*>();
      const auto now  = widget->TryCall<"get_Context", SettingsOptionContext*>();
      if (!text || !now) {
        complete = false;
        continue;
      }
      if ((now.value && now.value != view->context.Get()) || ValueView(widget) != view
          || CurrentValueLabel(widget, view->slider != nullptr, number) != label)
        continue;
      if (il2cpp::ToUtf8(text.value) != (number ? view->value_text : view->label_text)) {
        applied = false;
        continue;
      }
      const auto cleared = label->TryCall<"ClearTextOverride", void>();
      if (cleared && !label->textOverride())
        applied = false;
      else
        complete = false;
    }
  }
  if (complete && ValueView(widget) == view) {
    view->state->Unbind();
    ValuesState().views.erase(widget);
  }
  return complete;
}
template <class Widget> std::shared_ptr<SettingsValueView> BindValue(Widget* widget)
{
  auto  context_root = il2cpp::GcHandle::Strong(widget->Context());
  auto* context      = context_root.template Get<SettingsOptionContext>();
  auto* row          = context ? ValueRow(context) : nullptr;
  if (!row)
    return {};
  auto view = ValueView(widget);
  if (view && SameValue(widget, view))
    return view;
  if (!ReleaseValue(widget))
    return {};
  if (widget->Context() != context)
    return {};
  view                          = std::make_shared<SettingsValueView>();
  view->widget                  = il2cpp::GcHandle::Weak(widget);
  view->context                 = il2cpp::GcHandle::Weak(context);
  view->label                   = il2cpp::GcHandle::Weak(widget->Label());
  il2cpp::GcHandle control_root = il2cpp::GcHandle::Strong(CurrentValueControl(widget, row->slider != nullptr));
  const auto interactable = control_root ? control_root.Get<SettingsValueControl>()->TryCall<"get_interactable", bool>()
                                         : il2cpp::CallResult<bool>{};
  if (!interactable)
    return {};
  view->control             = il2cpp::GcHandle::Weak(control_root.Get());
  view->interactable_before = interactable.value;
  if constexpr (std::is_same_v<Widget, SettingsSliderWidget>)
    view->value_label = il2cpp::GcHandle::Weak(widget->ValueLabel());
  view->choice = row->choice;
  view->slider = row->slider;
  view->visit  = CurrentPageVisit();
  if (row->choice)
    view->state.emplace(*row->choice, row->index);
  else
    view->state.emplace(*row->slider);
  if (!view->widget || !view->context || !view->label || !view->control || (view->slider && !view->value_label)
      || !IsPageVisitCurrent(view->visit) || widget->Context() != context)
    return {};
  ValuesState().views.insert_or_assign(widget, view);
  return view;
}
template <class Widget, class Original> void RenderValue(Widget* widget, Original original)
{
  if (!ValuesThread()) {
    ValueOriginal([&] { original(widget); });
    return;
  }
  // Native OnDidBindContext calls virtual SetWidgetData. Preserve that nested
  // original once; only our optional render work is suppressed during reentry.
  auto previous = ValueView(widget);
  if (previous && (previous->rendering || previous->requesting || previous->clearing)) {
    const auto current = widget->template TryCall<"get_Context", SettingsOptionContext*>();
    ValueOriginal([&] { original(widget); },
                  current && current.value == previous->context.Get() && !previous->revoked ? previous.get() : nullptr);
    return;
  }
  auto view = BindValue(widget);
  if (!view) {
    ValueOriginal([&] { original(widget); });
    return;
  }
  auto widget_root  = il2cpp::GcHandle::Strong(widget);
  auto context_root = il2cpp::GcHandle::Strong(view->context.Get());
  struct Scope {
    SettingsValueView&           view;
    SettingsValueView*           previous;
    NativeViewState::RenderScope suppress;
    Scope(SettingsValueView& v)
        : view(v)
        , previous(settings_value_rendering)
        , suppress(*v.state)
    {
      view.rendering           = true;
      settings_value_rendering = &view;
    }
    ~Scope()
    {
      settings_value_rendering = previous;
      view.rendering           = false;
    }
  } scope{*view};
  if (!widget_root || !context_root || !SameValue(widget, view)) {
    original(widget);
    return;
  }
  const auto read_version = [](const auto& snapshot) {
    return std::array<uint64_t, 3>{snapshot.revision, snapshot.epoch, snapshot.owner};
  };
  const auto version =
      view->choice ? read_version(view->choice->state().Observe()) : read_version(view->slider->state().Observe());
  if (!view->observed || view->version != version) {
    // Publish the rendering sentinel before Observe/Bind can call native getters.
    // Rebinding only changed snapshots preserves a rejected request's Retry state.
    view->state->Bind();
    view->version  = version;
    view->observed = true;
  }
  if (!SameValue(widget, view))
    settings_value_rendering = nullptr; // A rebound row must not read the previous source's snapshot.
  if constexpr (std::is_same_v<Widget, SettingsSelectionWidget>) {
    if (SameValue(widget, view) && view->choice->multiple() && !view->group_detached) {
      il2cpp::GcHandle toggle = il2cpp::GcHandle::Strong(widget->Toggle());
      const auto       group  = toggle ? toggle.Get<unity::Toggle>()->TryCall<"get_group", SettingsToggleGroup*>()
                                       : il2cpp::CallResult<SettingsToggleGroup*>{};
      if (!group) {
        original(widget);
        return;
      }
      if (group.value) {
        view->selection_group = il2cpp::GcHandle::Weak(group.value);
        if (!view->selection_group) {
          original(widget);
          return;
        }
        view->group_detached = true; // A managed setter may mutate before an exception.
        const auto detached  = toggle.Get<unity::Toggle>()->TryCall<"set_group", void(SettingsToggleGroup*)>(nullptr);
        const auto observed  = toggle.Get<unity::Toggle>()->TryCall<"get_group", SettingsToggleGroup*>();
        if (!detached || !observed || observed.value || !SameValue(widget, view)) {
          original(widget);
          return;
        }
      }
    }
  }
  original(widget);
  if (!SameValue(widget, view))
    return;
  auto  label_root = il2cpp::GcHandle::Strong(view->label.Get());
  auto* label      = label_root.template Get<TextLocalizer>();
  view->label_text = view->state->presentation_label();
  auto label_text  = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(view->label_text)));
  if (label && label_text && SameValue(widget, view) && widget->Label() == label) {
    view->label_applied = true; // A contained exception can still follow a native mutation.
    (void)label->template TryCall<"OverrideLocalizedText", void(Il2CppString*)>(
        reinterpret_cast<Il2CppString*>(label_text.Get()));
  }
  if (!SameValue(widget, view))
    return;
  if constexpr (std::is_same_v<Widget, SettingsSelectionWidget>) {
    auto toggle_root = il2cpp::GcHandle::Strong(widget->Toggle());
    if (auto* toggle = toggle_root.template Get<unity::Toggle>()) {
      view->last_interactable    = view->state->enabled();
      view->interactable_applied = true;
      (void)toggle->template TryCall<"set_interactable", void(bool)>(view->state->enabled());
      if (SameValue(widget, view) && widget->Toggle() == toggle)
        (void)toggle->template TryCall<"SetIsOnWithoutNotify", void(bool)>(view->state->value().value_or(false));
    }
  } else {
    auto slider_root = il2cpp::GcHandle::Strong(widget->Slider());
    if (auto* slider = slider_root.template Get<SettingsValueSlider>()) {
      view->last_interactable    = view->state->enabled();
      view->interactable_applied = true;
      (void)slider->template TryCall<"set_interactable", void(bool)>(view->state->enabled());
      if (SameValue(widget, view) && widget->Slider() == slider)
        (void)slider->template TryCall<"SetValueWithoutNotify", void(float)>(view->state->number());
    }
    auto value_root = il2cpp::GcHandle::Strong(view->value_label.Get());
    if (auto* value = value_root.template Get<TextLocalizer>()) {
      view->value_text = view->state->known() ? (view->slider->label() == SliderLabel::Percentage
                                                     ? std::format("{}%", view->state->displayNumber() * 100.0f)
                                                     : std::format("{}", view->state->displayNumber()))
                                              : "Unavailable";
      auto text        = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(view->value_text)));
      if (text && SameValue(widget, view) && widget->ValueLabel() == value) {
        view->value_applied = true;
        (void)value->template TryCall<"OverrideLocalizedText", void(Il2CppString*)>(
            reinterpret_cast<Il2CppString*>(text.Get()));
      }
    }
  }
}
void SelectionValueRefresh(auto original, SettingsSelectionWidget* widget)
{ RenderValue(widget, original); }
void SliderValueRefresh(auto original, SettingsSliderWidget* widget)
{ RenderValue(widget, original); }
void SelectionValueChanged(auto original, SettingsSelectionWidget* widget, bool desired)
{
  if (!ValuesThread()) {
    original(widget, desired);
    return;
  }
  auto view = ValueView(widget);
  if (!view) {
    original(widget, desired);
    return;
  }
  if (view->rendering || view->requesting || view->clearing)
    return;
  if (view->choice && view->choice->multiple() && PageRefreshInProgress())
    return; // PLAY: a native panel bind cannot authorize a checkbox edit.
  const auto current = widget->TryCall<"get_Context", SettingsOptionContext*>();
  const auto owned   = current ? ValueOwnership(current.value) : std::nullopt;
  if (owned && !*owned) {
    ReleaseValue(widget);
    original(widget, desired);
    return;
  }
  if (!SameValue(widget, view))
    return;
  view->requesting = true;
  struct Requesting {
    bool& flag;
    ~Requesting()
    { flag = false; }
  } requesting{view->requesting};
  view->state->Request(desired);
  RequestPageRefresh();
}
void SliderValueChanged(auto original, SettingsSliderWidget* widget, float desired)
{
  if (!ValuesThread()) {
    original(widget, desired);
    return;
  }
  auto view = ValueView(widget);
  if (!view) {
    original(widget, desired);
    return;
  }
  if (view->rendering || view->requesting || view->clearing)
    return;
  const auto current = widget->TryCall<"get_Context", SettingsOptionContext*>();
  const auto owned   = current ? ValueOwnership(current.value) : std::nullopt;
  if (owned && !*owned) {
    ReleaseValue(widget);
    original(widget, desired);
    return;
  }
  if (!SameValue(widget, view))
    return;
  view->requesting = true;
  struct Requesting {
    bool& flag;
    ~Requesting()
    { flag = false; }
  } requesting{view->requesting};
  view->state->Request(desired);
  RequestPageRefresh();
}
void SliderValueLabel(auto original, SettingsSliderWidget* widget, float value)
{
  if (ValuesThread()) {
    auto view = ValueView(widget);
    if (view && view->slider && view->state && !view->clearing) {
      auto       root    = il2cpp::GcHandle::Strong(widget);
      auto       current = root ? widget->TryCall<"get_Context", SettingsOptionContext*>()
                                : il2cpp::CallResult<SettingsOptionContext*>{};
      auto       context = current ? il2cpp::GcHandle::Strong(current.value) : il2cpp::GcHandle{};
      const auto owned   = context ? ValueOwnership(context.Get<SettingsOptionContext>()) : std::nullopt;
      // Ownership observation can reenter native code. Qualify the same live
      // view again afterward; a pooled or departed row keeps native behavior.
      const auto checked = root ? widget->TryCall<"get_Context", SettingsOptionContext*>()
                                : il2cpp::CallResult<SettingsOptionContext*>{};
      if (owned && *owned && checked && checked.value == context.Get() && view->context.Get() == context.Get()
          && view->widget.Get() == widget && ValueView(widget) == view && !view->revoked && !view->clearing
          && IsPageVisitCurrent(view->visit) && view->state && view->state->known())
        // Unity invokes this listener separately after the change listener.
        // Substitute only its label argument, never the setting/slider/save.
        value = view->state->displayNumber();
    }
  }
  original(widget, value);
}
void SelectionValueRelease(auto original, SettingsSelectionWidget* widget)
{
  if (ValuesThread())
    ReleaseValue(widget);
  original(widget);
}
void SliderValueRelease(auto original, SettingsSliderWidget* widget)
{
  if (ValuesThread())
    ReleaseValue(widget);
  original(widget);
}

bool AppendValue(SettingsSectionDirector* director, SettingsContext* context, SettingsContainerContext* parent,
                 const PageCatalog::Item& item)
{
  if (!ValuesThread())
    return false;
  auto* choice_ptr = std::get_if<ChoiceSetting*>(&item);
  auto* slider_ptr = std::get_if<SliderSetting*>(&item);
  auto* choice     = choice_ptr ? *choice_ptr : nullptr;
  auto* slider     = slider_ptr ? *slider_ptr : nullptr;
  if (!choice && !slider)
    return false;
  auto before = settings_pages_binding::Children(parent);
  if (!before)
    return false;
  const auto  method = choice ? SettingsValueContext::AddChoiceMethod() : SettingsValueContext::AddSliderMethod();
  const auto& state  = ValuesState();
  auto        getter = il2cpp::GcHandle::Strong(settings_values_binding::DelegateFor(
      method, choice ? 3 : 2, director, choice ? state.choice_get.method() : state.slider_get.method()));
  auto        setter = il2cpp::GcHandle::Strong(settings_values_binding::DelegateFor(
      method, choice ? 4 : 3, director, choice ? state.choice_set.method() : state.slider_set.method()));
  const auto  query_setter =
      SettingsOptionContext::Class::Method<void(settings_binding::ReferenceDelegate*)>("set_QueryOptionState");
  auto query =
      il2cpp::GcHandle::Strong(settings_values_binding::DelegateFor(query_setter, 0, director, state.query.method()));
  if (!getter || !setter || !query)
    return false;
  auto label = il2cpp::GcHandle::Strong(
      reinterpret_cast<Il2CppObject*>(il2cpp::NewString(choice ? choice->state().id() : slider->state().id())));
  if (!label)
    return false;
  il2cpp::CallResult<void> added;
  if (choice) {
    auto names = il2cpp::GcHandle::Strong(
        il2cpp::Array<Il2CppString*>::New(il2cpp::detail::StringClass(), choice->labels().size()));
    auto* array = names.Get<il2cpp::Array<Il2CppString*>>();
    if (!array)
      return false;
    for (size_t index = 0; index < array->size(); ++index)
      array->Set(index, il2cpp::NewString(choice->item_id(static_cast<int>(index))));
    added =
        context->TryCall<"AddSelection", void(SettingsContainerContext*, Il2CppString*,
                                              settings_binding::ReferenceObject*, settings_binding::ReferenceDelegate*,
                                              settings_binding::ReferenceDelegate*, Il2CppString*)>(
            parent, reinterpret_cast<Il2CppString*>(label.Get()),
            reinterpret_cast<settings_binding::ReferenceObject*>(array),
            getter.Get<settings_binding::ReferenceDelegate>(), setter.Get<settings_binding::ReferenceDelegate>(),
            il2cpp::NewString(""));
  } else {
    added =
        context
            ->TryCall<"AddSlider", void(SettingsContainerContext*, Il2CppString*, settings_binding::ReferenceDelegate*,
                                        settings_binding::ReferenceDelegate*, settings_binding::ReferenceDelegate*,
                                        settings_binding::ReferenceObject*, bool, float, float)>(
                parent, reinterpret_cast<Il2CppString*>(label.Get()), getter.Get<settings_binding::ReferenceDelegate>(),
                setter.Get<settings_binding::ReferenceDelegate>(), query.Get<settings_binding::ReferenceDelegate>(),
                nullptr, false, slider->minimum(), slider->maximum());
  }
  auto       after = settings_pages_binding::Children(parent);
  const auto count = choice ? choice->labels().size() : size_t{1};
  if (!added || !after || after->size() != before->size() + count)
    return false; // This parent is still off-screen; caller discards the entire staged tree.
  for (size_t index = 0; index < before->size(); ++index)
    if ((*before)[index].Get() != (*after)[index].Get())
      return false;
  for (size_t index = 0; index < count; ++index) {
    auto* row = (*after)[before->size() + index].Get<SettingsOptionContext>();
    if (!row
        || (choice ? row->klass != SettingsSelectionContext::Class::Raw()
                   : row->klass != SettingsSliderContext::Class::Raw()))
      return false;
    const auto callbacks = settings_values_binding::Callbacks(row, getter.Get(), setter.Get());
    const auto id        = choice ? choice->item_id(static_cast<int>(index)) : slider->state().id();
    if (choice
        && !reinterpret_cast<SettingsSelectionContext*>(row)->ConfigureIdentity(static_cast<int32_t>(index), id,
                                                                                getter.Get(), setter.Get()))
      return false;
    if (!callbacks || !*callbacks || !row->HasLabel(id)
        || !row->TryCall<"set_QueryOptionState", void(settings_binding::ReferenceDelegate*)>(
            query.Get<settings_binding::ReferenceDelegate>()))
      return false;
    if (slider) {
      if (!reinterpret_cast<SettingsSliderContext*>(row)->ConfigureLabel(static_cast<int32_t>(slider->label())))
        return false;
    }
    const auto observed = settings_pages_binding::Query(row);
    if (!observed || observed.value != query.Get())
      return false;
    SettingsValueRow record;
    record.context = il2cpp::GcHandle::Weak(row);
    record.get     = il2cpp::GcHandle::Weak(getter.Get());
    record.set     = il2cpp::GcHandle::Weak(setter.Get());
    record.query   = il2cpp::GcHandle::Weak(query.Get());
    record.choice  = choice;
    record.slider  = slider;
    record.index   = static_cast<int>(index);
    if (!record.context || !record.get || !record.set || !record.query)
      return false;
    ValuesState().rows.insert_or_assign(row, std::move(record));
  }
  return true;
}
void SettingsValuesFrame()
{
  if (!ValuesThread())
    return;
  static Throttle lifecycle(std::chrono::milliseconds(250));
  if (!lifecycle.Due())
    return;
  auto& state = ValuesState();
  std::erase_if(state.rows, [](const auto& pair) { return !pair.second.context.Get(); });
  const std::vector<std::pair<il2cpp::Object*, std::shared_ptr<SettingsValueView>>> views(state.views.begin(),
                                                                                          state.views.end());
  for (const auto& [key, view] : views) {
    if (ValueView(key) != view || view->rendering || view->requesting || view->clearing)
      continue;
    auto       root  = il2cpp::GcHandle::Strong(view->widget.Get());
    const auto alive = root ? unity::TryAlive(root.Get()) : il2cpp::CallResult<bool>{true, false};
    if (!alive)
      continue;
    if (!alive.value) {
      if (view->state)
        view->state->Unbind();
      if (ValueView(key) == view)
        state.views.erase(key);
      continue;
    }
    auto* widget = root.Get<il2cpp::Object>();
    if (view->revoked || !IsPageVisitCurrent(view->visit)) {
      ReleaseValue(widget);
      continue;
    }
    // The page owner can retain an unchanged native row list. Refresh the
    // snapshot/presentation here so other single-choice rows and external edits
    // update without firing a user setter or allocating another native row.
    if (auto* selection = widget->As<SettingsSelectionWidget>())
      RenderValue(selection, [](auto* value) { (void)value->template TryCall<"SetWidgetData", void>(); });
    else if (auto* slider = widget->As<SettingsSliderWidget>())
      RenderValue(slider, [](auto* value) { (void)value->template TryCall<"SetWidgetData", void>(); });
  }
}
void InvalidateValues()
{
  if (!ValuesThread())
    return;
  for (const auto& [key, view] : ValuesState().views) {
    view->revoked = true;
    if (view->state)
      view->state->Invalidate();
  }
}
bool InstallSettingsValues()
{
  auto& state = ValuesState();
  if (!SettingsSectionDirector::QualifiedQueryState()
      || !SettingsSelectionWidget::Class::HasField<"_label", TextLocalizer*>()
      || !SettingsSelectionWidget::Class::HasField<"_toggle", unity::Toggle*>()
      || !SettingsSliderWidget::Class::HasField<"_label", TextLocalizer*>()
      || !SettingsSliderWidget::Class::HasField<"_valueLabel", TextLocalizer*>()
      || !SettingsSliderWidget::Class::HasField<"_slider", SettingsValueSlider*>()
      || !SettingsValueSlider::Class::Method<void(bool)>("set_interactable")
      || !SettingsValueSlider::Class::Method<void(float)>("SetValueWithoutNotify")
      || !state.choice_get.Initialize(
          SettingsSectionDirector::Class::Method<int()>("GetQualityOptionSelectedIndex").Info(), ValueChoiceGet)
      || !state.choice_set.Initialize(
          SettingsSectionDirector::Class::Method<void(int)>("OnQualityOptionSelected").Info(), ValueChoiceSet)
      || !state.slider_get.Initialize(SettingsSectionDirector::Class::Method<float()>("GetCurrentShadowsIndex").Info(),
                                      ValueSliderGet)
      || !state.slider_set.Initialize(
          SettingsSectionDirector::Class::Method<void(float)>("OnShadowsSettingChanged").Info(), ValueSliderSet)
      || !state.query.Initialize(
          SettingsSectionDirector::Class::Method<int()>("QueryShouldShowGenericPcSetting").Info(), ValueQuery)
      || !SettingsValueContext::AddChoiceMethod() || !SettingsValueContext::AddSliderMethod())
    return false;
  il2cpp::HookSet hooks("NativeSettingsValues");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSelectionWidget::Class::Method<void()>("SetWidgetData")),
                  SelectionValueRefresh);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSelectionWidget::Class::Method<void()>("OnDidBindContext")),
                  SelectionValueRefresh);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(SettingsSelectionWidget::Class::Method<void(bool)>("OnToggleValueChanged")),
                  SelectionValueChanged);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(SettingsSelectionWidget::Class::Method<void()>("OnAboutToReleaseContext")),
                  SelectionValueRelease);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSliderWidget::Class::Method<void()>("SetWidgetData")),
                  SliderValueRefresh);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSliderWidget::Class::Method<void()>("OnDidBindContext")),
                  SliderValueRefresh);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSliderWidget::Class::Method<void(float)>("OnSliderValueChanged")),
                  SliderValueChanged);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSliderWidget::Class::Method<void(float)>("UpdateValueLabel")),
                  SliderValueLabel);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsSliderWidget::Class::Method<void()>("OnAboutToReleaseContext")),
                  SliderValueRelease);
  if (!hooks.Commit())
    return false;
  state.thread = std::this_thread::get_id();
  state.active = RegisterPageItemAdapter(PageItemKind::Choice, AppendValue, ValueIdentity, InvalidateValues)
                 && RegisterPageItemAdapter(PageItemKind::Slider, AppendValue, ValueIdentity, InvalidateValues);
  return state.active;
}
} // namespace

const Feature kNativeSettingsValues{"NativeSettingsValues", &Config::Patches::nativesettingshooks,
                                    InstallSettingsValues, SettingsValuesFrame};
