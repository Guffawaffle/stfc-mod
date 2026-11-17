#pragma once

// Actual ReleaseAction/RestoreAction over controlled native components. No Unity
// pooling, native rendering or page-owned editor is created by this fixture.
namespace
{
struct ActionCleanupLabel {
  Il2CppObject header;
  bool         overridden = true;
  std::string  text       = "owned";
  int          clears     = 0;
};
struct ActionCleanupObject {
  Il2CppObject header;
  bool         active = false;
  int          sets   = 0;
};
struct ActionCleanupButton {
  Il2CppObject         header;
  ActionCleanupObject* object;
  bool                 enabled = false, failed = false;
  int                  clears = 0;
};
struct ActionCleanupWidget {
  Il2CppObject           header;
  SettingsActionContext* context;
  ActionCleanupLabel *   label, *button_label = nullptr, *value_label = nullptr;
  ActionCleanupButton*   button;
};
std::function<void()>  action_cleanup_on_text, action_cleanup_on_button, action_cleanup_on_active;
bool                   action_cleanup_context_fails = false;
SettingsActionContext* ActionCleanupContext(ActionCleanupWidget* widget, const MethodInfo*)
{
  if (action_cleanup_context_fails)
    throw Il2CppExceptionWrapper(&thrown);
  return widget->context;
}
Il2CppString* ActionCleanupText(ActionCleanupLabel* label, const MethodInfo*)
{
  const auto before = label->text;
  if (action_cleanup_on_text)
    std::exchange(action_cleanup_on_text, {})();
  return ValuesString(before.data(), static_cast<uint32_t>(before.size()));
}
void ActionCleanupClearText(ActionCleanupLabel* label, const MethodInfo*)
{
  ++label->clears;
  label->overridden = false;
  label->text       = "native";
}
bool ActionCleanupInteractable(ActionCleanupButton* button, const MethodInfo*)
{
  if (button->failed)
    throw Il2CppExceptionWrapper(&thrown);
  const auto before = button->enabled;
  if (action_cleanup_on_button)
    std::exchange(action_cleanup_on_button, {})();
  return before;
}
void ActionCleanupClearButton(ActionCleanupButton* button, const MethodInfo*)
{
  ++button->clears;
  button->enabled = true;
}
unity::GameObject* ActionCleanupButtonObject(ActionCleanupButton* button, const MethodInfo*)
{ return reinterpret_cast<unity::GameObject*>(button->object); }
bool ActionCleanupActive(ActionCleanupObject* object, const MethodInfo*)
{
  const auto before = object->active;
  if (action_cleanup_on_active)
    std::exchange(action_cleanup_on_active, {})();
  return before;
}
void ActionCleanupSetActive(ActionCleanupObject* object, bool active, const MethodInfo*)
{
  ++object->sets;
  object->active = active;
}

void SettingsActionCleanupCases()
{
  auto& context_class = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionContext");
  auto& widget_class  = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionWidget");
  auto& label_class   = ValuesClass("Digit.Client.UI", "TextLocalizer", &unity_object_class);
  auto& button_class  = ValuesClass("Fixture.SettingsActionCleanup", "Button");
  ValuesField(label_class, "_textOverride", bool_class, offsetof(ActionCleanupLabel, overridden));
  ValuesField(widget_class, "_label", label_class, offsetof(ActionCleanupWidget, label));
  ValuesField(widget_class, "_buttonLabel", label_class, offsetof(ActionCleanupWidget, button_label));
  ValuesField(widget_class, "_valueLabel", label_class, offsetof(ActionCleanupWidget, value_label));
  ValuesField(widget_class, "_button", button_class, offsetof(ActionCleanupWidget, button));
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&ActionCleanupContext), T(context_class), {});
  AddMethod(label_class, "CurrentText", reinterpret_cast<void*>(&ActionCleanupText), T(string_class), {});
  AddMethod(label_class, "ClearTextOverride", reinterpret_cast<void*>(&ActionCleanupClearText), T(void_class), {});
  AddMethod(button_class, "get_Interactable", reinterpret_cast<void*>(&ActionCleanupInteractable), T(bool_class), {});
  AddMethod(button_class, "ClearInteractable", reinterpret_cast<void*>(&ActionCleanupClearButton), T(void_class), {});
  AddMethod(button_class, "get_gameObject", reinterpret_cast<void*>(&ActionCleanupButtonObject), T(game_object_class),
            {});
  AddMethod(game_object_class, "get_activeSelf", reinterpret_cast<void*>(&ActionCleanupActive), T(bool_class), {});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&ActionCleanupSetActive), T(void_class),
            {T(bool_class)});
  Il2CppObject        context{&context_class.klass}, replacement{&context_class.klass};
  ActionCleanupLabel  label{{&label_class.klass}}, other_label{{&label_class.klass}};
  ActionCleanupObject object{{&game_object_class.klass}}, other_object{{&game_object_class.klass}};
  ActionCleanupButton button{{&button_class.klass}, &object}, other_button{{&button_class.klass}, &other_object};
  ActionCleanupWidget widget{
      {&widget_class.klass}, reinterpret_cast<SettingsActionContext*>(&context), &label, nullptr, nullptr, &button};
  auto*                               native_widget = reinterpret_cast<SettingsActionWidget*>(&widget);
  auto&                               state         = Actions();
  const auto                          roots_before  = warp_roots.size();
  std::shared_ptr<SettingsActionView> view;
  const auto                          arm = [&] {
    state.views.clear();
    view.reset();
    action_cleanup_on_text = action_cleanup_on_button = action_cleanup_on_active = {};
    action_cleanup_context_fails                                                 = false;
    label.overridden                                                             = true;
    label.text                                                                   = "owned";
    label.clears                                                                 = 0;
    button.failed                                                                = false;
    button.enabled                                                               = false;
    button.clears                                                                = 0;
    object.active                                                                = false;
    object.sets                                                                  = 0;
    widget.context       = reinterpret_cast<SettingsActionContext*>(&context);
    widget.label         = &label;
    widget.button        = &button;
    view                 = std::make_shared<SettingsActionView>();
    view->widget         = il2cpp::GcHandle::Weak(native_widget);
    view->context        = il2cpp::GcHandle::Weak(&context);
    view->labels[0]      = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&label));
    view->label_owned[0] = true;
    view->applied[0]     = "owned";
    view->button         = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&button));
    view->button_object  = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&object));
    view->interactable   = false;
    view->hidden_before  = true;
    view->ready          = true;
    state.views.emplace(native_widget, view);
  };
  arm();
  ReleaseAction(native_widget);
  Require(label.clears == 1 && button.clears == 1 && object.sets == 1 && object.active && state.views.empty(),
          "normal release restores owned overrides and retires the view");
  arm();
  widget.context = reinterpret_cast<SettingsActionContext*>(&replacement);
  ReleaseAction(native_widget);
  Require(label.clears == 0 && button.clears == 0 && object.sets == 0 && state.views.empty(),
          "already-pooled replacement context retains its presentation");
  arm();
  widget.label = &other_label;
  ReleaseAction(native_widget);
  Require(label.clears == 0 && other_label.clears == 0 && state.views.empty(),
          "release must not clear a retained label no longer attached to the widget");
  arm();
  widget.button = &other_button;
  ReleaseAction(native_widget);
  Require(button.clears == 0 && object.sets == 0 && other_button.clears == 0 && other_object.sets == 0,
          "release must not restore detached button components or their objects");
  arm();
  action_cleanup_on_text = [&] { widget.context = reinterpret_cast<SettingsActionContext*>(&replacement); };
  ReleaseAction(native_widget);
  Require(label.clears == 0 && button.clears == 0 && object.sets == 0,
          "context replacement during text observation stops old cleanup before mutation");
  arm();
  action_cleanup_on_button = [&] { widget.context = reinterpret_cast<SettingsActionContext*>(&replacement); };
  ReleaseAction(native_widget);
  Require(label.clears == 1 && button.clears == 0 && object.sets == 0,
          "context replacement during button observation stops subsequent restoration");
  arm();
  action_cleanup_on_active = [&] { widget.context = reinterpret_cast<SettingsActionContext*>(&replacement); };
  ReleaseAction(native_widget);
  Require(label.clears == 1 && button.clears == 1 && object.sets == 0,
          "context replacement during visibility observation cannot activate a stale object");
  arm();
  action_cleanup_context_fails = true;
  ReleaseAction(native_widget);
  Require(state.views.contains(native_widget) && view->revoked && !view->ready && label.clears == 0,
          "unknown context retains revoked cleanup custody without writing");
  action_cleanup_context_fails = false;
  ReleaseAction(native_widget);
  Require(state.views.empty() && label.clears == 1, "later confirmed ownership permits cleanup retry");
  arm();
  button.failed = true;
  ReleaseAction(native_widget);
  Require(label.clears == 1 && state.views.contains(native_widget), "partial cleanup retains unresolved work");
  label.overridden = true;
  label.text       = "owned"; // Another owner supplies identical text after our clear.
  button.failed    = false;
  ReleaseAction(native_widget);
  Require(label.clears == 1 && label.overridden && state.views.empty(),
          "cleanup retry must not reacquire an already released label override");
  for (bool rendering : {true, false}) {
    arm();
    view->rendering = rendering;
    view->invoking  = !rendering;
    ReleaseAction(native_widget);
    Require(view->revoked && !view->ready && state.views.contains(native_widget) && label.clears == 0
                && button.clears == 0 && object.sets == 0,
            "release during render or invocation defers component cleanup");
    view->rendering = view->invoking = false;
    ReleaseAction(native_widget);
    Require(state.views.empty() && label.clears == 1, "deferred cleanup completes after callback unwinds");
  }
  arm();
  auto replacement_view  = std::make_shared<SettingsActionView>();
  action_cleanup_on_text = [&] { state.views.insert_or_assign(native_widget, replacement_view); };
  ReleaseAction(native_widget);
  Require(label.clears == 0 && button.clears == 0 && object.sets == 0 && ActionView(native_widget) == replacement_view,
          "cleanup cannot mutate or erase a view registered during observation");
  state.views.clear();
  view.reset();
  replacement_view.reset();
  Require(warp_roots.size() == roots_before, "cleanup fixture releases all managed custody");
  std::cout << "Settings action cleanup: components, pooled/reentrant replacement, retry and deferred release passed\n";
}
} // namespace
