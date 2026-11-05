#pragma once

// Included after il2cpp_bindings.cc's controlled runtime. Calls the production
// binding; native choice/slider bodies are never used as discovery probes.
#include "game/settings_actions.h"
#include "game/settings_values.h"
#include <functional>

namespace
{
std::deque<std::unique_ptr<char[]>> settings_values_strings;
Il2CppString*                       ValuesString(const char* text, uint32_t length)
{
  auto& bytes = settings_values_strings.emplace_back(
      std::make_unique<char[]>(sizeof(Il2CppString) + sizeof(Il2CppChar) * length));
  auto* value         = reinterpret_cast<Il2CppString*>(bytes.get());
  value->object.klass = &string_class.klass;
  value->length       = static_cast<int32_t>(length);
  for (uint32_t index = 0; index < length; ++index) {
    Require(static_cast<unsigned char>(text[index]) < 0x80, "fixture setting identifiers are ASCII");
    value->chars[index] = static_cast<Il2CppChar>(text[index]);
  }
  return value;
}
} // namespace
API(Il2CppString*, il2cpp_string_new_len, (const char* text, uint32_t length))
{ return ValuesString(text, length); }
END_API

namespace
{
struct SettingsValuesGeneric {
  std::array<const Il2CppType*, 1> arguments{};
  Il2CppGenericInst                inst{};
  Il2CppGenericClass               generic{};
};
std::deque<SettingsValuesGeneric> settings_values_generics;
FakeClass&                        ValuesClass(const char* ns, const char* name, FakeClass* parent = &object_class)
{
  const auto found = std::ranges::find_if(classes, [=](const auto& item) {
    return std::string_view(item.klass.namespaze) == ns && std::string_view(item.klass.name) == name;
  });
  return found == classes.end() ? Define(ns, name, IL2CPP_TYPE_CLASS, parent) : *found;
}
FakeClass& ValuesClosed(const char* name, FakeClass& definition, FakeClass& argument, FakeClass& parent)
{
  auto& value                              = Define("Fixture.SettingsValues", name, IL2CPP_TYPE_GENERICINST, &parent);
  auto& data                               = settings_values_generics.emplace_back();
  data.arguments[0]                        = T(argument);
  data.inst.type_argc                      = 1;
  data.inst.type_argv                      = data.arguments.data();
  data.generic.type                        = T(definition);
  data.generic.context.class_inst          = &data.inst;
  value.klass.byval_arg.data.generic_class = &data.generic;
  return value;
}
void ValuesField(FakeClass& owner, const char* name, FakeClass& value, std::size_t offset)
{
  AddField(owner, name, value, static_cast<int32_t>(offset));
  // The fake class-from-type API recognizes canonical generic type descriptors.
  owner.fields.back().type = T(value);
}
struct ValuesRefObject {
  Il2CppObject  header;
  Il2CppObject* reference = nullptr;
};
struct ValuesContextObject {
  Il2CppObject  header;
  Il2CppObject *get = nullptr, *set = nullptr;
  Il2CppObject* label = nullptr;
  int32_t       index = 0;
};
struct ValuesLocaleObject {
  Il2CppObject  header;
  Il2CppString* identifier = nullptr;
};
int     settings_values_identifier_sets = 0;
bool    settings_values_index_throws = false, settings_values_identifier_ignored = false;
int32_t ValuesChoiceIndex(Il2CppObject* object, const MethodInfo*)
{
  if (settings_values_index_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<ValuesContextObject*>(object)->index;
}
LocaleTextContext* ValuesChoiceLabel(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<LocaleTextContext*>(reinterpret_cast<ValuesContextObject*>(object)->label); }
Il2CppString* ValuesIdentifierGet(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<ValuesLocaleObject*>(object)->identifier; }
void ValuesIdentifierSet(Il2CppObject* object, Il2CppString* value, const MethodInfo*)
{
  ++settings_values_identifier_sets;
  if (!settings_values_identifier_ignored)
    reinterpret_cast<ValuesLocaleObject*>(object)->identifier = value;
}
struct ValuesListObject {
  Il2CppObject                 header;
  int32_t                      count = 1;
  std::array<Il2CppObject*, 2> entries{};
};
int  settings_values_count_calls = 0, settings_values_item_calls = 0, settings_values_trigger_calls = 0;
bool settings_values_count_throws = false;
std::function<void()> settings_values_on_count;
int32_t               ValuesCount(Il2CppObject* object, const MethodInfo*)
{
  ++settings_values_count_calls;
  if (settings_values_on_count)
    std::exchange(settings_values_on_count, {})();
  if (settings_values_count_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<ValuesListObject*>(object)->count;
}
settings_binding::ReferenceDelegate* ValuesItem(Il2CppObject* object, int32_t index, const MethodInfo*)
{
  ++settings_values_item_calls;
  return reinterpret_cast<settings_binding::ReferenceDelegate*>(
      reinterpret_cast<ValuesListObject*>(object)->entries.at(static_cast<std::size_t>(index)));
}
int32_t ValuesTrigger(Il2CppObject*, const MethodInfo*)
{
  ++settings_values_trigger_calls;
  return 0;
}
void ValuesWrapper(FakeClass& func, FakeClass& action, FakeClass& list, FakeClass& getter)
{
  ValuesField(func, "_callback", getter, offsetof(ValuesRefObject, reference));
  ValuesField(action, "_callbacks", list, offsetof(ValuesRefObject, reference));
  AddMethod(func, "Trigger", reinterpret_cast<void*>(&ValuesTrigger), T(int_class), {});
  AddMethod(list, "get_Count", reinterpret_cast<void*>(&ValuesCount), T(int_class), {});
  AddMethod(list, "get_Item", reinterpret_cast<void*>(&ValuesItem), T(delegate_base), {T(int_class)});
}

void SettingsValuesCallbackObservations()
{
  auto& option            = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& selection         = ValuesClass("Digit.Prime.GameSettings", "SelectionItemOptionContext", &option);
  auto& slider            = ValuesClass("Digit.Prime.GameSettings", "SliderOptionContext", &option);
  auto& safe_func         = ValuesClass("Digit.Client.Core", "SafeFunc`1");
  auto& safe_action       = ValuesClass("Digit.Client.Core", "SafeAction`1");
  auto& list_definition   = ValuesClass("System.Collections.Generic", "List`1");
  auto& func_definition   = ValuesClass("System", "Func`1", &delegate_base);
  auto& action_definition = ValuesClass("System", "Action`1", &delegate_base);
  auto& get_int           = ValuesClosed("FuncInt", func_definition, int_class, delegate_base);
  auto& set_int           = ValuesClosed("ActionInt", action_definition, int_class, delegate_base);
  auto& get_float         = ValuesClosed("FuncFloat", func_definition, float_class, delegate_base);
  auto& set_float         = ValuesClosed("ActionFloat", action_definition, float_class, delegate_base);
  auto& func_int          = ValuesClosed("SafeFuncInt", safe_func, int_class, object_class);
  auto& action_int        = ValuesClosed("SafeActionInt", safe_action, int_class, object_class);
  auto& list_int          = ValuesClosed("ListIntActions", list_definition, set_int, object_class);
  auto& func_float        = ValuesClosed("SafeFuncFloat", safe_func, float_class, object_class);
  auto& action_float      = ValuesClosed("SafeActionFloat", safe_action, float_class, object_class);
  auto& list_float        = ValuesClosed("ListFloatActions", list_definition, set_float, object_class);
  ValuesWrapper(func_int, action_int, list_int, get_int);
  ValuesWrapper(func_float, action_float, list_float, get_float);
  ValuesField(selection, "GetCallback", func_int, offsetof(ValuesContextObject, get));
  ValuesField(selection, "SetCallback", action_int, offsetof(ValuesContextObject, set));
  ValuesField(slider, "GetCallback", func_float, offsetof(ValuesContextObject, get));
  ValuesField(slider, "SetCallback", action_float, offsetof(ValuesContextObject, set));
  Il2CppObject getter{}, setter{}, foreign{};
  getter.klass = &get_int.klass;
  setter.klass = foreign.klass = &set_int.klass;
  ValuesListObject callbacks;
  callbacks.header.klass = &list_int.klass;
  callbacks.entries      = {&setter, nullptr};
  ValuesRefObject get, set;
  get.header.klass = &func_int.klass;
  get.reference    = &getter;
  set.header.klass = &action_int.klass;
  set.reference    = &callbacks.header;
  ValuesContextObject context;
  context.header.klass    = &selection.klass;
  context.get             = &get.header;
  context.set             = &set.header;
  auto*      row          = reinterpret_cast<SettingsOptionContext*>(&context);
  const auto roots_before = warp_roots.size();
  auto       owns         = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && *owns, "choice callbacks are read from the derived runtime fields through an OptionContext facade");
  Require(settings_values_trigger_calls == 0 && warp_roots.size() == roots_before,
          "callback ownership neither invokes a getter nor leaks temporary roots");

  callbacks.count = 0;
  owns            = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && !*owns, "an observed empty setter list is known unowned, not an unavailable zero");
  callbacks.count = -1;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter), "a negative native Count remains unavailable");
  callbacks.count      = 2;
  callbacks.entries[1] = &foreign;
  owns                 = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && !*owns, "our setter plus a foreign action does not establish exclusive row ownership");
  callbacks.count      = 1;
  callbacks.entries[0] = &foreign;
  owns                 = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && !*owns, "a different same-signature setter is known unowned");
  callbacks.entries[0] = &setter;
  get.reference        = &foreign;
  owns                 = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && !*owns, "getter identity is independent of matching row labels and setter signature");
  get.reference = &getter;
  context.get   = nullptr;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter), "a missing SafeFunc is unavailable");
  context.get                  = &get.header;
  settings_values_count_throws = true;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter), "a contained Count exception is unavailable");
  settings_values_count_throws = false;
  warp_root_failure            = true;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter),
          "failed wrapper custody declines before traversal");
  warp_root_failure = false;

  const auto* name        = func_int.fields[0].name;
  func_int.fields[0].name = "missing_callback";
  Require(!settings_values_binding::Callbacks(row, &getter, &setter),
          "missing callback metadata cannot become a null observation");
  func_int.fields[0].name = name;
  const auto count_before = settings_values_count_calls;
  get.header.klass        = &func_float.klass;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter) && settings_values_count_calls == count_before,
          "a same-named SafeFunc<float> cannot supply a choice's SafeFunc<int> contract");
  get.header.klass       = &func_int.klass;
  callbacks.header.klass = &list_float.klass;
  Require(!settings_values_binding::Callbacks(row, &getter, &setter),
          "the SafeAction list's closed delegate type must agree");
  callbacks.header.klass = &list_int.klass;
  owns                   = settings_values_binding::Callbacks(row, &getter, &setter);
  Require(owns && *owns && settings_values_trigger_calls == 0 && warp_roots.size() == roots_before,
          "refused observations leave the original qualified choice usable without invoking source delegates");

  auto& locale = ValuesClass("Digit.Client.UI", "LocaleTextContext");
  AddMethod(selection, "get_Index", reinterpret_cast<void*>(&ValuesChoiceIndex), T(int_class), {});
  auto& label_get = AddMethod(option, "get_LabelContext", reinterpret_cast<void*>(&ValuesChoiceLabel), T(locale), {});
  auto& identifier_get =
      AddMethod(locale, "get_Identifier", reinterpret_cast<void*>(&ValuesIdentifierGet), T(string_class), {});
  auto& identifier_set = AddMethod(locale, "set_Identifier", reinterpret_cast<void*>(&ValuesIdentifierSet),
                                   T(void_class), {T(string_class)});
  AddProperty(option, "LabelContext", &label_get);
  AddProperty(locale, "Identifier", &identifier_get, &identifier_set);
  constexpr std::string_view native_id    = "community_mod.ui.auto_confirm_instant_warp";
  constexpr std::string_view canonical_id = "community_mod.ui.auto_confirm_instant_warp.0";
  ValuesLocaleObject         label;
  label.header.klass = &locale.klass;
  label.identifier   = ValuesString(native_id.data(), static_cast<uint32_t>(native_id.size()));
  context.label      = &label.header;
  auto* concrete     = reinterpret_cast<SettingsSelectionContext*>(&context);
  Require(
      !row->HasLabel(canonical_id) && concrete->ConfigureIdentity(0, canonical_id, &getter, &setter)
          && row->HasLabel(canonical_id) && context.label == &label.header && settings_values_identifier_sets == 1,
      "production choice helper rewrites a native-formatted label at Index zero while retaining its LocaleTextContext");
  const auto sets      = settings_values_identifier_sets;
  label.identifier     = ValuesString(native_id.data(), static_cast<uint32_t>(native_id.size()));
  callbacks.entries[0] = &foreign;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && settings_values_identifier_sets == sets
              && row->HasLabel(native_id),
          "foreign callback identity refuses a canonical rewrite without changing native text");
  callbacks.entries[0] = &setter;
  context.index        = 1;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && settings_values_identifier_sets == sets,
          "native choice Index must match the expected row before rewriting");
  context.index                = 0;
  settings_values_index_throws = true;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && settings_values_identifier_sets == sets,
          "unavailable native Index cannot masquerade as the first choice");
  settings_values_index_throws = false;
  identifier_set.name          = "UnavailableIdentifierSetter";
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && settings_values_identifier_sets == sets,
          "missing Identifier setter metadata refuses before native mutation");
  identifier_set.name                                          = "set_Identifier";
  const auto* identifier_parameter                             = identifier_set.parameters[0];
  const_cast<const Il2CppType**>(identifier_set.parameters)[0] = T(object_class);
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && settings_values_identifier_sets == sets,
          "a related Object parameter cannot replace the exact Identifier String setter contract");
  const_cast<const Il2CppType**>(identifier_set.parameters)[0] = identifier_parameter;
  settings_values_identifier_ignored                           = true;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && row->HasLabel(native_id),
          "a successful but ignored Identifier setter does not authorize publication");
  settings_values_identifier_ignored = false;
  context.label                      = nullptr;
  const auto refused_sets            = settings_values_identifier_sets;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter)
              && settings_values_identifier_sets == refused_sets,
          "missing existing LocaleTextContext is not replaced");
  context.label        = &label.header;
  context.header.klass = &option.klass;
  Require(!concrete->ConfigureIdentity(0, canonical_id, &getter, &setter)
              && settings_values_identifier_sets == refused_sets,
          "base OptionContext cannot be rewritten as a choice");
  context.header.klass = &selection.klass;
  Require(concrete->ConfigureIdentity(0, canonical_id, &getter, &setter) && row->HasLabel(canonical_id)
              && settings_values_trigger_calls == 0 && warp_roots.size() == roots_before,
          "qualified canonical rewrite remains usable and releases custody without invoking source delegates");
}

int   settings_values_choice_calls = 0, settings_values_slider_calls = 0, settings_values_interactable_calls = 0;
bool  settings_values_interactable = true;
float settings_values_slider_value = 1.0f;
void  ValuesAddChoice(Il2CppObject*, SettingsContainerContext*, Il2CppString*, settings_binding::ReferenceObject*,
                      settings_binding::ReferenceDelegate*, settings_binding::ReferenceDelegate*, Il2CppString*,
                      const MethodInfo*)
{ ++settings_values_choice_calls; }
void ValuesAddSlider(Il2CppObject*, SettingsContainerContext*, Il2CppString*, settings_binding::ReferenceDelegate*,
                     settings_binding::ReferenceDelegate*, settings_binding::ReferenceDelegate*,
                     settings_binding::ReferenceObject*, bool, float, float, const MethodInfo*)
{ ++settings_values_slider_calls; }
void ValuesInteractable(Il2CppObject*, bool value, const MethodInfo*)
{
  ++settings_values_interactable_calls;
  settings_values_interactable = value;
}
void ValuesSliderSet(Il2CppObject*, float value, const MethodInfo*)
{ settings_values_slider_value = value; }
void SettingsValuesMethodObservations()
{
  auto& option                     = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& container                  = ValuesClass("Digit.Prime.GameSettings", "ContainerOptionContext", &option);
  auto& settings                   = ValuesClass("Digit.Prime.GameSettings", "SettingsContext");
  auto& locale                     = ValuesClass("Digit.Client.UI", "LocaleTextContext");
  auto& locale_array               = Define("Fixture.SettingsValues", "Locale[]", IL2CPP_TYPE_SZARRAY, &array_class);
  locale_array.klass.rank          = 1;
  locale_array.klass.element_class = &locale.klass;
  string_array_class.klass.rank    = 1;
  string_array_class.klass.element_class = &string_class.klass;
  AddMethod(settings, "AddSelection", reinterpret_cast<void*>(&ValuesAddChoice), T(void_class),
            {T(container), T(string_class), T(intptr_class), T(delegate_base), T(delegate_base), T(string_class)});
  auto& choice = AddMethod(
      settings, "AddSelection", reinterpret_cast<void*>(&ValuesAddChoice), T(void_class),
      {T(container), T(string_class), T(string_array_class), T(delegate_base), T(delegate_base), T(string_class)});
  AddMethod(settings, "AddSlider", reinterpret_cast<void*>(&ValuesAddSlider), T(void_class),
            {T(container), T(string_class), T(delegate_base), T(delegate_base), T(delegate_base), T(intptr_class),
             T(bool_class), T(float_class), T(float_class)});
  auto& slider = AddMethod(settings, "AddSlider", reinterpret_cast<void*>(&ValuesAddSlider), T(void_class),
                           {T(container), T(string_class), T(delegate_base), T(delegate_base), T(delegate_base),
                            T(locale_array), T(bool_class), T(float_class), T(float_class)});
  Require(SettingsValueContext::AddChoiceMethod().Info() == &choice
              && SettingsValueContext::AddSliderMethod().Info() == &slider,
          "typed native row lookup skips same-arity IntPtr overloads without calling them");
  Require(settings_values_choice_calls == 0 && settings_values_slider_calls == 0,
          "metadata discovery performs no native row append");
  const auto* strings                                  = choice.parameters[2];
  const_cast<const Il2CppType**>(choice.parameters)[2] = T(object_class);
  Require(!SettingsValueContext::AddChoiceMethod() && settings_values_choice_calls == 0,
          "an Object parameter cannot replace the native string reference vector");
  const_cast<const Il2CppType**>(choice.parameters)[2] = strings;
  locale_array.klass.rank                              = 2;
  Require(!SettingsValueContext::AddSliderMethod() && settings_values_slider_calls == 0,
          "a multidimensional localized-values array declines before invocation");
  locale_array.klass.rank = 1;
  Require(SettingsValueContext::AddChoiceMethod() && SettingsValueContext::AddSliderMethod(),
          "the original precise array declarations remain available after declined observations");

  auto& behaviour     = ValuesClass("UnityEngine", "Behaviour", &component_class);
  auto& selectable    = ValuesClass("UnityEngine.UI", "Selectable", &behaviour);
  auto& slider_type   = ValuesClass("UnityEngine.UI", "Slider", &selectable);
  auto& slider_widget = ValuesClass("Digit.Prime.GameSettings", "SliderOptionWidget");
  AddMethod(selectable, "set_interactable", reinterpret_cast<void*>(&ValuesInteractable), T(void_class),
            {T(bool_class)});
  AddMethod(slider_type, "SetValueWithoutNotify", reinterpret_cast<void*>(&ValuesSliderSet), T(void_class),
            {T(float_class)});
  ValuesField(slider_widget, "_slider", slider_type, offsetof(ValuesRefObject, reference));
  Il2CppObject native_slider{};
  native_slider.klass = &slider_type.klass;
  ValuesRefObject widget;
  widget.header.klass = &slider_widget.klass;
  widget.reference    = &native_slider;
  auto* typed_widget  = reinterpret_cast<SettingsSliderWidget*>(&widget);
  auto* bound_slider  = typed_widget->Slider();
  Require(bound_slider == reinterpret_cast<SettingsValueSlider*>(&native_slider),
          "slider source field retains the precise Slider binding");
  Require(bound_slider->TryCall<"set_interactable", void(bool)>(false).succeeded
              && settings_values_interactable_calls == 1 && !settings_values_interactable,
          "the Slider binding reaches Selectable's setter rather than searching Behaviour");
  Require(bound_slider->TryCall<"SetValueWithoutNotify", void(float)>(0.0f).succeeded
              && settings_values_slider_value == 0.0f,
          "zero is a valid checked slider value and is sent through the non-notifying native setter");
  Require(!bound_slider->TryCall<"MissingValueGetter", float>(),
          "a missing float observation stays unavailable rather than observed zero");
}
int  settings_action_custom_reads = 0, settings_action_unity_reads = 0;
bool ValuesCustomInteractable(Il2CppObject*, const MethodInfo*)
{
  ++settings_action_custom_reads;
  return false;
}
bool ValuesUnityInteractable(Il2CppObject*, const MethodInfo*)
{
  ++settings_action_unity_reads;
  return true;
}
void SettingsActionButtonObservations()
{
  auto& button = ValuesClass("Digit.Client.UI", "GenericButtonWidget");
  AddMethod(button, "get_Interactable", reinterpret_cast<void*>(&ValuesCustomInteractable), T(bool_class), {});
  AddMethod(button, "get_interactable", reinterpret_cast<void*>(&ValuesUnityInteractable), T(bool_class), {});
  Il2CppObject object{};
  object.klass        = &button.klass;
  const auto observed = reinterpret_cast<SettingsActionButton*>(&object)->TryInteractable();
  Require(observed && !observed.value && settings_action_custom_reads == 1 && settings_action_unity_reads == 0,
          "the game's custom button uses Interactable rather than Unity Selectable's differently cased property");
}
int32_t settings_slider_label        = 1;
int     settings_slider_label_sets   = 0;
bool    settings_slider_label_ignore = false, settings_slider_label_throw = false;
void    ValuesLabelTypeSet(Il2CppObject*, int32_t value, const MethodInfo*)
{
  ++settings_slider_label_sets;
  if (!settings_slider_label_ignore)
    settings_slider_label = value;
}
int32_t ValuesLabelTypeGet(Il2CppObject*, const MethodInfo*)
{
  if (settings_slider_label_throw)
    throw Il2CppExceptionWrapper(&thrown);
  return settings_slider_label;
}
void SettingsSliderLabelObservations()
{
  auto& option = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& slider = ValuesClass("Digit.Prime.GameSettings", "SliderOptionContext", &option);
  AddMethod(slider, "set_LabelType", reinterpret_cast<void*>(&ValuesLabelTypeSet), T(void_class), {T(int_class)});
  AddMethod(slider, "get_LabelType", reinterpret_cast<void*>(&ValuesLabelTypeGet), T(int_class), {});
  Il2CppObject object{};
  object.klass  = &slider.klass;
  auto* context = reinterpret_cast<SettingsSliderContext*>(&object);
  Require(context->ConfigureLabel(0) && settings_slider_label == 0 && settings_slider_label_sets == 1,
          "the production slider-label helper uses the concrete class; Value zero is a successful observation");
  Require(context->ConfigureLabel(1) && settings_slider_label == 1 && settings_slider_label_sets == 2,
          "Percentage configures and reads back the concrete slider property");
  settings_slider_label_ignore = true;
  Require(!context->ConfigureLabel(0), "a successful setter with mismatched readback declines publication");
  settings_slider_label_ignore = false;
  settings_slider_label_throw  = true;
  Require(!context->ConfigureLabel(0), "an unavailable label readback cannot masquerade as observed Value zero");
  settings_slider_label_throw = false;
  const auto sets             = settings_slider_label_sets;
  object.klass                = &option.klass;
  Require(!context->ConfigureLabel(0) && settings_slider_label_sets == sets,
          "a base OptionContext is refused without invoking the slider setter");
  object.klass = &slider.klass;
  Require(!context->ConfigureLabel(2) && settings_slider_label_sets == sets,
          "unrecognized label kinds are refused without a native mutation");
}
void SettingsValuesBindingObservations()
{
  SettingsValuesCallbackObservations();
  SettingsValuesMethodObservations();
  SettingsActionButtonObservations();
  SettingsSliderLabelObservations();
  std::cout << "IL2CPP native settings values: exact callbacks, array signatures and Slider source passed\n";
}
} // namespace
