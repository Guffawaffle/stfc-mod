#pragma once

// Exercise the actual post-bind command callback; publication and Unity rendering
// are controlled boundaries, not a native game reproduction.
#include "../src/mod/features/settings_actions.cc"

namespace mod_settings::native
{
bool AllowsSettingsSearchAction(std::string_view)
{ return false; }
} // namespace mod_settings::native

namespace
{
struct SettingsActionConsumerContext {
  Il2CppObject              header;
  SettingsContainerContext* parent;
  Il2CppObject *            query, *callback;
};
SettingsContainerContext* ActionConsumerParent(SettingsActionConsumerContext* context, const MethodInfo*)
{ return context->parent; }
settings_binding::ReferenceDelegate* ActionConsumerQuery(SettingsActionConsumerContext* context, const MethodInfo*)
{ return reinterpret_cast<settings_binding::ReferenceDelegate*>(context->query); }
settings_binding::ReferenceObject* ActionConsumerCallback(SettingsActionConsumerContext* context, const MethodInfo*)
{ return reinterpret_cast<settings_binding::ReferenceObject*>(context->callback); }

void SettingsActionConsumerCases()
{
  using namespace mod_settings;
  auto& option        = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& container     = ValuesClass("Digit.Prime.GameSettings", "ContainerOptionContext", &option);
  auto& context_class = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionContext", &option);
  auto& widget_class  = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionWidget");
  auto& safe_class    = ValuesClass("Fixture.SettingsActions", "SafeAction");
  auto& list_class    = ValuesClass("Fixture.SettingsActions", "CallbackList");
  ValuesField(safe_class, "_callbacks", list_class, offsetof(ValuesRefObject, reference));
  AddMethod(list_class, "get_Count", reinterpret_cast<void*>(&ValuesCount), T(int_class), {});
  AddMethod(list_class, "get_Item", reinterpret_cast<void*>(&ValuesItem), T(delegate_base), {T(int_class)});
  AddMethod(option, "get_Parent", reinterpret_cast<void*>(&ActionConsumerParent), T(container), {});
  AddMethod(option, "get_QueryOptionState", reinterpret_cast<void*>(&ActionConsumerQuery), T(delegate_base), {});
  AddMethod(context_class, "get_Callback", reinterpret_cast<void*>(&ActionConsumerCallback), T(safe_class), {});
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&SettingsConsumerGetContext), T(context_class), {});
  auto&      state = Actions();
  MethodInfo click_schema{}, query_schema{};
  click_schema.return_type = T(void_class);
  query_schema.return_type = T(int_class);
  Require(state.click.Initialize(&click_schema, InvokeAction) && state.query.Initialize(&query_schema, ActionQuery),
          "actual action ownership uses private matching callback schemas");
  Il2CppObject   token{&object_class.klass}, foreign_token{&object_class.klass}, parent{&container.klass};
  Il2CppDelegate click{}, query{}, foreign{};
  const auto     delegate = [&](Il2CppDelegate& value, const MethodInfo* method) {
    reinterpret_cast<Il2CppObject*>(&value)->klass = &delegate_base.klass;
    value.target                                   = &token;
    value.method                                   = method;
    value.method_ptr = value.invoke_impl = method->methodPointer;
    value.invoke_impl_this               = value.target;
  };
  delegate(click, state.click.method());
  delegate(query, state.query.method());
  ValuesListObject callbacks;
  callbacks.header.klass = &list_class.klass;
  callbacks.entries      = {reinterpret_cast<Il2CppObject*>(&click), nullptr};
  ValuesRefObject               safe{{&safe_class.klass}, &callbacks.header};
  SettingsActionConsumerContext context{{&context_class.klass},
                                        reinterpret_cast<SettingsContainerContext*>(&parent),
                                        reinterpret_cast<Il2CppObject*>(&query),
                                        &safe.header};
  SettingsActionConsumerContext replacement = context;
  auto*                         row         = reinterpret_cast<SettingsActionContext*>(&context);
  SettingsConsumerWidget        widget{{&widget_class.klass}, row};
  auto*                         native_widget = reinterpret_cast<SettingsActionWidget*>(&widget);
  int                           invoked       = 0;
  std::function<void()>         on_read;
  ActionSetting                 action;
  action.identity = "fixture.defaults.details";
  action.label    = "Defaults and Help";
  action.read     = [&](size_t) {
    if (on_read)
      std::exchange(on_read, {})();
    return ActionSetting::Presentation{"Setting details", "Details", "Saved"};
  };
  action.invoke = [&](size_t index) {
    Require(index == 0 && native::InvokingActionWidget() == native_widget,
            "actual command retains its index and synchronous invoking-widget custody");
    ++invoked;
    InvokeAction(&token, nullptr); // The same command cannot invoke itself reentrantly.
  };
  const auto roots_before = warp_roots.size();
  state.active            = true;
  state.thread            = std::this_thread::get_id();
  auto record             = std::make_shared<SettingsActionRow>();
  record->context         = il2cpp::GcHandle::Weak(row);
  record->parent          = il2cpp::GcHandle::Weak(&parent);
  record->click           = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&click));
  record->query           = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&query));
  record->token           = il2cpp::GcHandle::Weak(&token);
  record->action          = &action;
  state.rows.emplace(row, record);
  auto view     = std::make_shared<SettingsActionView>();
  view->widget  = il2cpp::GcHandle::Weak(native_widget);
  view->context = il2cpp::GcHandle::Weak(row);
  view->token   = il2cpp::GcHandle::Weak(&token);
  view->action  = &action;
  view->visit   = settings_consumer_visit;
  view->ready   = true;
  view->applied = {"Setting details", "Details", "Saved"};
  state.views.emplace(native_widget, view);
  InvokeAction(&token, nullptr);
  Require(invoked == 1 && !native::InvokingActionWidget() && !view->invoking && settings_consumer_refreshes == 1,
          "one owned command invokes once, restores synchronous custody and requests deferred refresh");
  InvokeAction(&foreign_token, nullptr);
  settings_consumer_page_refresh = true;
  InvokeAction(&token, nullptr);
  settings_consumer_page_refresh = false;
  fleet_native_capture           = true;
  InvokeAction(&token, nullptr);
  fleet_native_capture = false;
  Require(invoked == 1, "foreign token, page refresh and shortcut capture cannot authorize a command");
  on_read = [&] { settings_values_on_count = [&] { ++settings_consumer_visit; }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "page departure during the final ownership read must not invoke the departed action");
  view->visit = settings_consumer_visit;
  on_read     = [&] {
    settings_values_on_count = [&] { widget.context = reinterpret_cast<SettingsActionContext*>(&replacement); };
  };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "context replacement during final ownership reads must not invoke the old action");
  widget.context = row;
  on_read        = [&] { settings_values_on_count = [&] { state.views.erase(native_widget); }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "retired view cannot invoke through a retained shared record");
  state.views.emplace(native_widget, view);
  on_read = [&] { settings_values_on_count = [&] { settings_consumer_on_context = [&] { state.rows.erase(row); }; }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1 && state.rows.empty(), "row retirement during final context observation blocks invocation");
  state.rows.emplace(row, record);
  view->applied[2] = "Saving...";
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "changed save-status presentation requires a fresh render before invoking");
  view->applied[2]     = "Saved";
  callbacks.entries[0] = reinterpret_cast<Il2CppObject*>(&foreign);
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "foreign callback ownership cannot invoke a mod command");
  callbacks.entries[0] = reinterpret_cast<Il2CppObject*>(&click);
  InvokeAction(&token, nullptr);
  Require(invoked == 2 && !native::InvokingActionWidget(), "original owned row remains usable after refused inputs");
  state.views.clear();
  state.rows.clear();
  view.reset();
  record.reset();
  state.active = false;
  Require(warp_roots.size() == roots_before, "action consumer releases every admitted managed root");
  std::cout << "Settings action consumer: invocation, token/page/context/presentation guards and reentry passed\n";
}
} // namespace
