#pragma once

// Exercise actual native bind, render, release and frame recovery. Components and
// managed observations are controlled; this is not a native game reproduction.
namespace
{
void ActionRebindText(ActionCleanupLabel* label, Il2CppString* text, const MethodInfo*)
{
  label->text       = il2cpp::ToUtf8(text);
  label->overridden = true;
}
void ActionRebindEnabled(ActionCleanupButton* button, bool enabled, const MethodInfo*)
{ button->enabled = enabled; }
std::function<void()> action_render_on_object;
bool                  action_render_object_fails = false;
unity::GameObject*    ActionRenderObject(ActionCleanupButton* button, const MethodInfo* method)
{
  if (action_render_object_fails)
    throw Il2CppExceptionWrapper(&thrown);
  auto* object = ActionCleanupButtonObject(button, method);
  if (action_render_on_object)
    std::exchange(action_render_on_object, {})();
  return object;
}

void SettingsActionRebindCases(bool rendering = false)
{
  auto& option        = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& container     = ValuesClass("Digit.Prime.GameSettings", "ContainerOptionContext", &option);
  auto& context_class = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionContext", &option);
  auto& widget_class  = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionWidget");
  auto& safe_class    = ValuesClass("Fixture.SettingsActions", "SafeAction");
  auto& list_class    = ValuesClass("Fixture.SettingsActions", "CallbackList");
  auto& label_class   = ValuesClass("Digit.Client.UI", "TextLocalizer", &unity_object_class);
  auto& button_class  = ValuesClass("Fixture.SettingsActionCleanup", "Button");
  ValuesField(safe_class, "_callbacks", list_class, offsetof(ValuesRefObject, reference));
  ValuesField(label_class, "_textOverride", bool_class, offsetof(ActionCleanupLabel, overridden));
  ValuesField(widget_class, "_label", label_class, offsetof(ActionCleanupWidget, label));
  ValuesField(widget_class, "_buttonLabel", label_class, offsetof(ActionCleanupWidget, button_label));
  ValuesField(widget_class, "_valueLabel", label_class, offsetof(ActionCleanupWidget, value_label));
  ValuesField(widget_class, "_button", button_class, offsetof(ActionCleanupWidget, button));
  AddMethod(list_class, "get_Count", reinterpret_cast<void*>(&ValuesCount), T(int_class), {});
  AddMethod(list_class, "get_Item", reinterpret_cast<void*>(&ValuesItem), T(delegate_base), {T(int_class)});
  AddMethod(option, "get_Parent", reinterpret_cast<void*>(&ActionConsumerParent), T(container), {});
  AddMethod(option, "get_QueryOptionState", reinterpret_cast<void*>(&ActionConsumerQuery), T(delegate_base), {});
  AddMethod(context_class, "get_Callback", reinterpret_cast<void*>(&ActionConsumerCallback), T(safe_class), {});
  const auto& get_context =
      AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&ActionCleanupContext), T(context_class), {});
  AddProperty(widget_class, "Context", &get_context);
  AddMethod(label_class, "CurrentText", reinterpret_cast<void*>(&ActionCleanupText), T(string_class), {});
  AddMethod(label_class, "ClearTextOverride", reinterpret_cast<void*>(&ActionCleanupClearText), T(void_class), {});
  AddMethod(label_class, "OverrideLocalizedText", reinterpret_cast<void*>(&ActionRebindText), T(void_class),
            {T(string_class)});
  AddMethod(button_class, "get_Interactable", reinterpret_cast<void*>(&ActionCleanupInteractable), T(bool_class), {});
  AddMethod(button_class, "OverrideInteractable", reinterpret_cast<void*>(&ActionRebindEnabled), T(void_class),
            {T(bool_class)});
  AddMethod(button_class, "ClearInteractable", reinterpret_cast<void*>(&ActionCleanupClearButton), T(void_class), {});
  AddMethod(button_class, "get_gameObject", reinterpret_cast<void*>(&ActionRenderObject), T(game_object_class), {});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&ActionCleanupActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&ActionCleanupSetActive), T(void_class),
            {T(bool_class)});

  auto&      state        = Actions();
  const auto roots_before = warp_roots.size();
  MethodInfo click_schema{}, query_schema{};
  click_schema.return_type = T(void_class);
  query_schema.return_type = T(int_class);
  Require(state.click.Initialize(&click_schema, InvokeAction) && state.query.Initialize(&query_schema, ActionQuery),
          "rebind fixture uses private owned callbacks");
  Il2CppObject                                 parent{&container.klass};
  std::array<Il2CppObject, 2>                  tokens{{{&object_class.klass}, {&object_class.klass}}};
  std::array<Il2CppDelegate, 2>                clicks{}, queries{};
  std::array<ValuesListObject, 2>              callbacks;
  std::array<ValuesRefObject, 2>               safes;
  std::array<SettingsActionConsumerContext, 2> contexts;
  std::array<ActionSetting, 2>                 actions;
  std::array<int, 2>                           invoked{};
  std::function<void()>                        on_read;
  std::function<void()>                        on_invoke;
  bool                                         hide_button = false;
  state.active                                             = true;
  state.thread                                             = std::this_thread::get_id();
  for (size_t i = 0; i < actions.size(); ++i) {
    const auto delegate = [&](Il2CppDelegate& value, const MethodInfo* method) {
      reinterpret_cast<Il2CppObject*>(&value)->klass = &delegate_base.klass;
      value.target                                   = &tokens[i];
      value.method                                   = method;
      value.method_ptr = value.invoke_impl = method->methodPointer;
      value.invoke_impl_this               = value.target;
    };
    delegate(clicks[i], state.click.method());
    delegate(queries[i], state.query.method());
    callbacks[i].header.klass = &list_class.klass;
    callbacks[i].entries      = {reinterpret_cast<Il2CppObject*>(&clicks[i]), nullptr};
    safes[i]                  = {{&safe_class.klass}, &callbacks[i].header};
    contexts[i]               = {{&context_class.klass},
                                 reinterpret_cast<SettingsContainerContext*>(&parent),
                                 reinterpret_cast<Il2CppObject*>(&queries[i]),
                                 &safes[i].header};
    actions[i].identity       = i ? "fixture.new" : "fixture.old";
    actions[i].read           = [&, i](size_t) {
      if (on_read)
        std::exchange(on_read, {})();
      return ActionSetting::Presentation{i ? "New row" : "Old row", hide_button ? "" : "Run",
                                         i ? "New value" : "Old value"};
    };
    actions[i].invoke = [&, i](size_t) {
      ++invoked[i];
      if (on_invoke)
        std::exchange(on_invoke, {})();
    };
    auto row     = std::make_shared<SettingsActionRow>();
    row->context = il2cpp::GcHandle::Weak(&contexts[i].header);
    row->parent  = il2cpp::GcHandle::Weak(&parent);
    row->click   = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&clicks[i]));
    row->query   = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&queries[i]));
    row->token   = il2cpp::GcHandle::Weak(&tokens[i]);
    row->action  = &actions[i];
    state.rows.emplace(reinterpret_cast<SettingsOptionContext*>(&contexts[i]), std::move(row));
  }
  ActionCleanupLabel  label{{&label_class.klass}}, button_label{{&label_class.klass}}, value{{&label_class.klass}};
  ActionCleanupObject object{{&game_object_class.klass}, true};
  ActionCleanupButton button{{&button_class.klass}, &object, true};
  ActionCleanupWidget widget{{&widget_class.klass}, nullptr, &label, &button_label, &value, &button};
  auto*               native_widget = reinterpret_cast<SettingsActionWidget*>(&widget);
  int                 originals     = 0;
  const auto          bind          = [&](size_t i) {
    SettingsActionWidget_SetWidgetData_Hook(
        [&](auto*) {
          ++originals;
          widget.context = reinterpret_cast<SettingsActionContext*>(&contexts[i]);
          for (auto* text : {&label, &button_label, &value}) {
            text->text       = "native";
            text->overridden = false;
          }
          button.enabled = true;
        },
        native_widget);
  };
  bind(0);
  auto old = ActionView(native_widget);
  if (!old || !old->ready) {
    for (const auto& error : errors)
      std::cerr << error << '\n';
    std::cerr << "view=" << bool(old) << " label=" << label.text << '\n';
  }
  Require(old && old->ready && label.text == "Old row", "ordinary bind renders owned action");
  InvokeAction(&tokens[0], nullptr);
  Require(invoked[0] == 1, "ordinary bound action invokes");
  if (rendering) {
    const auto frame = [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(260));
      ActionsFrame();
    };
    ActionCleanupLabel  replacement_label{{&label_class.klass}};
    ActionCleanupObject replacement_object{{&game_object_class.klass}, true};
    ActionCleanupButton replacement_button{{&button_class.klass}, &replacement_object, true};
    label.text             = "native";
    label.overridden       = false;
    action_cleanup_on_text = [&] { widget.label = &replacement_label; };
    RenderAction(native_widget, old);
    Require(label.text == "native" && !old->ready,
            "render must not write a label detached during its text observation");
    frame();
    Require(ActionView(native_widget) && ActionView(native_widget) != old && ActionView(native_widget)->ready
                && replacement_label.text == "Old row" && originals == 1,
            "detached presentation recovers current labels without replaying native bind");
    widget.label = &label;
    bind(0);
    old                      = ActionView(native_widget);
    button.enabled           = false;
    action_cleanup_on_button = [&] { widget.button = &replacement_button; };
    RenderAction(native_widget, old);
    Require(!button.enabled && !old->ready,
            "render must not override a button detached during interactability observation");
    frame();
    Require(ActionView(native_widget) && ActionView(native_widget) != old && ActionView(native_widget)->ready
                && ActionView(native_widget)->button.Get() == reinterpret_cast<Il2CppObject*>(&replacement_button),
            "detached presentation recovers the current button");
    widget.button = &button;
    bind(0);
    old                      = ActionView(native_widget);
    hide_button              = true;
    action_cleanup_on_active = [&] { button.object = &replacement_object; };
    RenderAction(native_widget, old);
    Require(object.active && !old->ready,
            "render must not hide an object detached during its active-state observation");
    frame();
    Require(ActionView(native_widget) && ActionView(native_widget) != old && ActionView(native_widget)->ready
                && !replacement_object.active && object.active,
            "recovered view applies visibility only to its current button object");
    hide_button = false;
    old         = ActionView(native_widget);
    RenderAction(native_widget, old);
    Require(old->ready && replacement_object.active, "current button visibility restores after presentation changes");
    action_render_object_fails = true;
    RenderAction(native_widget, old);
    Require(!old->ready && ActionView(native_widget) == old,
            "unknown button association cannot authorize presentation");
    action_render_object_fails = false;
    RenderAction(native_widget, old);
    Require(old->ready, "known button association allows a later render retry");
    hide_button = true;
    RenderAction(native_widget, old);
    Require(!replacement_object.active && old->ready, "visibility restoration case starts with an owned hidden object");
    hide_button             = false;
    action_render_on_object = [&] { button.object = &object; };
    RenderAction(native_widget, old);
    Require(!replacement_object.active && !old->ready,
            "stale gameObject getter result must not restore an object detached inside that getter");
    frame();
    old = ActionView(native_widget);
    Require(old && old->ready && object.active, "latest confirmed button object recovers after a stale getter");
    InvokeAction(&tokens[0], nullptr);
    Require(invoked[0] == 2, "only recovered current presentation remains actionable");
    ReleaseAction(native_widget);
    old.reset();
    state.views.clear();
    state.rows.clear();
    state.active = false;
    Require(warp_roots.size() == roots_before, "render fixture releases every managed root");
    std::cout << "Settings action rendering: attached labels/buttons/objects, retry, rebind and roots passed\n";
    return;
  }
  on_read = [&] { bind(1); };
  RenderAction(native_widget, old);
  Require(old->revoked && !old->ready && originals == 2,
          "reentrant native bind revokes active rendering and calls native original once");
  ActionsFrame();
  auto current = ActionView(native_widget);
  Require(current && current != old && current->action == &actions[1] && current->ready && label.text == "New row"
              && button_label.text == "Run" && value.text == "New value",
          "frame recovery must render and bind the replacement after deferred cleanup");
  InvokeAction(&tokens[0], nullptr);
  InvokeAction(&tokens[1], nullptr);
  Require(invoked[0] == 1 && invoked[1] == 1 && originals == 2,
          "recovered binding accepts only its new token and never replays native bind");
  const auto frame = [&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    ActionsFrame();
  };
  settings_values_on_count = [&] { bind(0); };
  frame();
  Require(ActionView(native_widget) && ActionView(native_widget)->ready
              && ActionView(native_widget)->action == &actions[0],
          "stale frame snapshot must not release a replacement installed during its ownership read");
  bind(1);
  current   = ActionView(native_widget);
  on_invoke = [&] { bind(0); };
  InvokeAction(&tokens[1], nullptr);
  Require(current->revoked && !current->ready, "native rebind during invocation retires the old command immediately");
  frame();
  Require(ActionView(native_widget) && ActionView(native_widget)->ready
              && ActionView(native_widget)->action == &actions[0],
          "frame also recovers binding deferred behind an invocation");

  current = ActionView(native_widget);
  on_read = [&] { bind(1); };
  RenderAction(native_widget, current);
  action_cleanup_context_fails = true;
  frame();
  Require(ActionView(native_widget) == current && current->revoked && !current->ready,
          "unknown cleanup observation retains revoked custody without authorizing a command");
  action_cleanup_context_fails = false;
  frame();
  Require(ActionView(native_widget) != current && ActionView(native_widget)->ready && label.text == "New row",
          "confirmed retry recovers rendering after unknown cleanup");

  current = ActionView(native_widget);
  on_read = [&] {
    bind(0);
    widget.button_label = nullptr;
  };
  RenderAction(native_widget, current);
  frame();
  Require(ActionView(native_widget) == current && current->revoked && !current->ready,
          "missing replacement component retains the pending binding without authorizing it");
  widget.button_label = &button_label;
  frame();
  Require(ActionView(native_widget) && ActionView(native_widget) != current && ActionView(native_widget)->ready
              && label.text == "Old row" && button_label.text == "Run",
          "replacement components arriving later recover rendering without another native bind");

  current = ActionView(native_widget);
  on_read = [&] {
    bind(1);
    bind(0);
  };
  RenderAction(native_widget, current);
  frame();
  Require(ActionView(native_widget)->ready && ActionView(native_widget)->action == &actions[0],
          "latest of multiple deferred binds wins");
  const auto before_nested = originals;
  SettingsActionWidget_SetWidgetData_Hook(
      [&](auto*) {
        ++originals;
        bind(1);
      },
      native_widget);
  Require(ActionView(native_widget)->ready && ActionView(native_widget)->action == &actions[1]
              && originals == before_nested + 2,
          "outer native bind cannot replace a newer nested binding or replay either original");

  bool       propagated   = false;
  const auto before_throw = originals;
  try {
    SettingsActionWidget_SetWidgetData_Hook(
        [&](auto*) {
          ++originals;
          throw std::runtime_error("native fixture bind failure");
        },
        native_widget);
  } catch (const std::runtime_error&) {
    propagated = true;
  }
  frame();
  Require(propagated && !ActionView(native_widget) && originals == before_throw + 1,
          "failed native original propagates once and cannot leave an unprepared recovery request");

  bind(0);
  current      = ActionView(native_widget);
  int releases = 0;
  on_read      = [&] {
    bind(1);
    SettingsActionWidget_OnAboutToReleaseContext_Hook(
        [&](auto*) {
          ++releases;
          widget.context = nullptr;
        },
        native_widget);
  };
  RenderAction(native_widget, current);
  frame();
  Require(!ActionView(native_widget) && releases == 1, "native release cancels deferred binding and runs once");

  bind(0);
  current = ActionView(native_widget);
  on_read = [&] {
    bind(1);
    ++settings_consumer_visit;
  };
  RenderAction(native_widget, current);
  frame();
  Require(!ActionView(native_widget), "page departure rejects a prepared replacement");

  bind(0);
  current = ActionView(native_widget);
  on_read = [&] {
    bind(1);
    InvalidateActions();
  };
  RenderAction(native_widget, current);
  frame();
  Require(!ActionView(native_widget) && state.rows.empty(), "adapter invalidation cancels every pending bind");
  const auto calls_before = invoked;
  InvokeAction(&tokens[0], nullptr);
  InvokeAction(&tokens[1], nullptr);
  Require(invoked == calls_before, "invalidated commands remain inert");
  const auto originals_before = originals;
  frame();
  Require(originals == originals_before && !ActionView(native_widget), "later frames cannot replay retired bindings");
  ReleaseAction(native_widget);
  old.reset();
  current.reset();
  state.views.clear();
  state.rows.clear();
  state.active = false;
  Require(warp_roots.size() == roots_before, "rebind fixture releases every managed root");
  std::cout << "Settings action rebind: render/invoke recovery, retry, latest-bind ownership, release/page/epoch "
               "cancellation and roots passed\n";
}
} // namespace
