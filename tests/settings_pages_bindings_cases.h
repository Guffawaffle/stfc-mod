#pragma once

// Included by il2cpp_bindings.cc after its controlled runtime/helpers, before main. These call production bindings.
#include "game/settings_pages.h"

namespace
{
int           settings_page_untyped_calls = 0, settings_page_typed_calls = 0, settings_page_category_calls = 0;
int           settings_widget_base_bind_calls = 0, settings_widget_base_release_calls = 0;
bool          settings_page_bind_throws = false, settings_page_category_throws = false;
Il2CppObject  settings_page_category{};
Il2CppObject* settings_page_seen_parent = nullptr;
Il2CppObject* settings_page_seen_query  = nullptr;
Il2CppObject* settings_page_seen_rows   = nullptr;
void SettingsPageUntypedBind(Il2CppObject*, SettingsDataProvider* provider, settings_binding::ReferenceObject* rows,
                             const MethodInfo*)
{
  Require(!provider, "native settings section bind preserves the game's null provider");
  ++settings_page_untyped_calls;
  settings_page_seen_rows = rows;
  if (settings_page_bind_throws)
    throw Il2CppExceptionWrapper(&thrown);
}
void SettingsPageTypedBind(Il2CppObject*, SettingsDataProvider*, Il2CppObject*, const MethodInfo*)
{ ++settings_page_typed_calls; }
void SettingsPageBaseBind(Il2CppObject*, SettingsDataProvider*, Il2CppObject*, const MethodInfo*)
{ ++settings_widget_base_bind_calls; }
void SettingsWidgetBaseRelease(Il2CppObject*, const MethodInfo*)
{ ++settings_widget_base_release_calls; }
struct SettingsWidgetObject {
  Il2CppObject        header{};
  WidgetDataProvider* provider = nullptr;
  il2cpp::Object*     context  = nullptr;
  int                 binds = 0, releases = 0;
};
bool settings_widget_release_throws = false;
void SettingsWidgetBind(Il2CppObject* receiver, WidgetDataProvider* provider, il2cpp::Object* context,
                        const MethodInfo*)
{
  auto* widget = reinterpret_cast<SettingsWidgetObject*>(receiver);
  ++widget->binds;
  widget->provider = provider;
  widget->context  = context;
}
void SettingsWidgetRelease(Il2CppObject* receiver, const MethodInfo*)
{
  auto* widget = reinterpret_cast<SettingsWidgetObject*>(receiver);
  ++widget->releases;
  widget->context  = nullptr;
  widget->provider = nullptr;
  if (settings_widget_release_throws)
    throw Il2CppExceptionWrapper(&thrown);
}
settings_binding::ReferenceObject* SettingsWidgetContext(Il2CppObject* receiver, const MethodInfo*)
{
  return reinterpret_cast<settings_binding::ReferenceObject*>(
      reinterpret_cast<SettingsWidgetObject*>(receiver)->context);
}
Il2CppObject*     settings_widget_forced_receiver = nullptr;
const MethodInfo* settings_widget_forced_schema   = nullptr;
const MethodInfo* settings_widget_forced_actual   = nullptr;
const MethodInfo* SettingsWidgetVirtualResult(Il2CppObject* receiver, const MethodInfo* schema, bool& handled)
{
  handled = receiver == settings_widget_forced_receiver && schema == settings_widget_forced_schema;
  return handled ? settings_widget_forced_actual : nullptr;
}
const MethodInfo* settings_widget_shared_bind    = nullptr;
const MethodInfo* settings_widget_shared_release = nullptr;
int               settings_widget_boxed_calls    = 0;
Il2CppObject*     SettingsWidgetRuntimeInvoke(const MethodInfo* method, void* receiver, void** args,
                                              Il2CppException** exception, bool& handled)
{
  handled = method == settings_widget_shared_bind || method == settings_widget_shared_release;
  if (!handled)
    return nullptr;
  ++settings_widget_boxed_calls;
  Require(receiver == settings_widget_forced_receiver && exception, "generic widget invocation preserves its receiver");
  if (method == settings_widget_shared_bind) {
    Require(args && !args[0] && args[1], "generic object bind preserves null provider and supplied context");
    SettingsWidgetBind(static_cast<Il2CppObject*>(receiver), static_cast<WidgetDataProvider*>(args[0]),
                       static_cast<il2cpp::Object*>(args[1]), method);
  } else {
    Require(args && !args[0], "generic context release has no managed arguments");
    SettingsWidgetRelease(static_cast<Il2CppObject*>(receiver), method);
  }
  return nullptr;
}
SettingsCategoryContext* SettingsPageAddCategory(Il2CppObject*, SettingsContainerContext* parent, Il2CppString*,
                                                 Il2CppString*, settings_binding::ReferenceDelegate* query,
                                                 const MethodInfo*)
{
  ++settings_page_category_calls;
  settings_page_seen_parent = parent;
  settings_page_seen_query  = query;
  if (settings_page_category_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<SettingsCategoryContext*>(&settings_page_category);
}
void SettingsPageBindingObservations()
{
  auto& provider  = Define("Digit.Client.UI", "IDataContextProvider", IL2CPP_TYPE_CLASS, &object_class);
  auto& widget    = Define("Digit.Client.UI", "Widget", IL2CPP_TYPE_CLASS, &object_class);
  auto& generic   = Define("Digit.Client.UI", "Widget`1", IL2CPP_TYPE_CLASS, &widget);
  auto& panel     = Define("Digit.Prime.GameSettings", "OptionTabPanelWidget", IL2CPP_TYPE_CLASS, &generic);
  auto& list      = Define("System.Collections", "IList", IL2CPP_TYPE_CLASS, &object_class);
  auto& rows      = Define("Digit.Prime.GameSettings", "OptionContext[]", IL2CPP_TYPE_SZARRAY, &list);
  auto& option    = Define("Digit.Prime.GameSettings", "OptionContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& container = Define("Digit.Prime.GameSettings", "ContainerOptionContext", IL2CPP_TYPE_CLASS, &option);
  auto& category  = Define("Digit.Prime.GameSettings", "CategoryOptionContext", IL2CPP_TYPE_CLASS, &container);
  auto& settings  = Define("Digit.Prime.GameSettings", "SettingsContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& query     = Define("System", "SettingsOptionQuery", IL2CPP_TYPE_CLASS, &delegate_base);
  auto& schema    = AddMethod(widget, "BindDataContext", reinterpret_cast<void*>(&SettingsPageBaseBind), T(void_class),
                              {T(provider), T(object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  schema.slot     = 35;
  auto& release_schema = AddMethod(widget, "ReleaseDataContext", reinterpret_cast<void*>(&SettingsWidgetBaseRelease),
                                   T(void_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  release_schema.slot  = 36;
  // Deliberately first: same name and arity, a different virtual slot and typed IList parameter.
  auto& typed = AddMethod(generic, "BindDataContext", reinterpret_cast<void*>(&SettingsPageTypedBind), T(void_class),
                          {T(provider), T(list)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  typed.slot  = 39;
  auto& untyped =
      AddMethod(generic, "BindDataContext", reinterpret_cast<void*>(&SettingsPageUntypedBind), T(void_class),
                {T(provider), T(object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  untyped.slot = 35;
  Il2CppObject panel_object{};
  panel_object.klass = &panel.klass;
  struct RowsObject {
    Il2CppObject        header;
    Il2CppArrayBounds*  bounds = nullptr;
    il2cpp_array_size_t length = 0;
  } row_object{};
  row_object.header.klass = &rows.klass;
  auto* native_panel      = reinterpret_cast<SettingsOptionPanel*>(&panel_object);
  auto* native_rows       = reinterpret_cast<il2cpp::Array<SettingsOptionContext*>*>(&row_object);
  Require(SettingsUntypedWidget::BindMethod().Info() == &schema, "section bind resolves the non-generic Widget schema");
  Require(native_panel->TryBind(native_rows).succeeded && settings_page_untyped_calls == 1
              && settings_page_typed_calls == 0 && settings_page_seen_rows == &row_object.header,
          "native page binding resolves slot 35 despite an earlier same-arity typed overload");
  settings_page_bind_throws = true;
  Require(!native_panel->TryBind(native_rows) && settings_page_untyped_calls == 2 && settings_page_typed_calls == 0,
          "partial native panel bind exception stays unavailable and never retries another overload");
  settings_page_bind_throws                             = false;
  const auto* original_parameter                        = untyped.parameters[1];
  const_cast<const Il2CppType**>(untyped.parameters)[1] = T(list);
  Require(!native_panel->TryBind(native_rows) && settings_page_untyped_calls == 2,
          "a changed virtual target with IList in the object slot is rejected before invocation");
  const_cast<const Il2CppType**>(untyped.parameters)[1] = original_parameter;
  untyped.flags |= METHOD_ATTRIBUTE_STATIC;
  Require(!native_panel->TryBind(native_rows) && settings_page_untyped_calls == 2,
          "a static replacement cannot become the native instance bind");
  untyped.flags &= ~METHOD_ATTRIBUTE_STATIC;
  auto& foreign_rows      = Define("Game", "ForeignOptionArray", IL2CPP_TYPE_SZARRAY, &object_class);
  row_object.header.klass = &foreign_rows.klass;
  Require(!native_panel->TryBind(native_rows) && settings_page_untyped_calls == 2,
          "a reference array that does not implement IList cannot become a settings panel list");
  row_object.header.klass = &rows.klass;
  Require(native_panel->TryBind(native_rows).succeeded && settings_page_untyped_calls == 3,
          "the original qualified virtual target remains usable after refused metadata");

  auto& bound_class = Define("Fixture.UI", "ContextWidget", IL2CPP_TYPE_CLASS, &widget);
  auto& bound_typed =
      AddMethod(bound_class, "BindDataContext", reinterpret_cast<void*>(&SettingsPageTypedBind), T(void_class),
                {T(provider), T(list)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  bound_typed.slot = 39;
  auto& bind = AddMethod(bound_class, "BindDataContext", reinterpret_cast<void*>(&SettingsWidgetBind), T(void_class),
                         {T(provider), T(object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  bind.slot  = 35;
  auto& release =
      AddMethod(bound_class, "ReleaseDataContext", reinterpret_cast<void*>(&SettingsWidgetRelease), T(void_class), {},
                METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL | METHOD_ATTRIBUTE_FINAL);
  release.slot = 36;
  auto& context_getter =
      AddMethod(bound_class, "get_Context", reinterpret_cast<void*>(&SettingsWidgetContext), T(object_class), {});
  AddProperty(bound_class, "Context", &context_getter);
  SettingsWidgetObject bound{};
  bound.header.klass = &bound_class.klass;
  auto* lifecycle    = reinterpret_cast<UntypedWidget*>(&bound);
  auto* reader       = reinterpret_cast<Widget<settings_binding::ReferenceObject>*>(&bound);
  auto* context      = reinterpret_cast<il2cpp::Object*>(&row_object.header);
  Require(lifecycle->TryBindContext(nullptr, context).succeeded && bound.binds == 1 && !bound.provider
              && reader->Context() == reinterpret_cast<settings_binding::ReferenceObject*>(context),
          "shared Widget binds and reads the exact object context with a null provider");
  Require(lifecycle->TryBindContext(nullptr, context).succeeded && bound.binds == 2 && reader->Context() == context,
          "rebinding the same context still invokes native binding to refresh expiry state");
  Require(!lifecycle->TryBindContext(nullptr, nullptr) && bound.binds == 2,
          "null context is declined rather than impersonating native release");
  Require(lifecycle->TryReleaseContext().succeeded && bound.releases == 1 && !reader->Context(),
          "context release dispatches the derived sealed override and clears the typed context");

  Il2CppObject foreign_widget{};
  foreign_widget.klass = &foreign_rows.klass;
  auto* foreign        = reinterpret_cast<UntypedWidget*>(&foreign_widget);
  Require(!foreign->TryBindContext(nullptr, context) && !foreign->TryReleaseContext(),
          "a foreign receiver cannot bind or release through the Widget schema");
  const auto* bind_parameter                         = bind.parameters[1];
  const_cast<const Il2CppType**>(bind.parameters)[1] = T(list);
  Require(!lifecycle->TryBindContext(nullptr, context) && bound.binds == 2,
          "shared Widget declines a typed replacement in the actual object bind slot");
  const_cast<const Il2CppType**>(bind.parameters)[1] = bind_parameter;
  const auto* release_return                         = release.return_type;
  release.return_type                                = T(bool_class);
  Require(!lifecycle->TryReleaseContext() && bound.releases == 1,
          "shared Widget declines a non-void actual release signature");
  release.return_type = release_return;
  release.flags |= METHOD_ATTRIBUTE_STATIC;
  Require(!lifecycle->TryReleaseContext() && bound.releases == 1,
          "shared Widget declines a static actual release target");
  release.flags &= ~METHOD_ATTRIBUTE_STATIC;

  settings_widget_forced_receiver = &bound.header;
  virtual_method_override         = &SettingsWidgetVirtualResult;
  settings_widget_forced_schema   = &schema;
  MethodInfo wrong_slot           = bind;
  wrong_slot.slot                 = 39;
  settings_widget_forced_actual   = &wrong_slot;
  Require(!lifecycle->TryBindContext(nullptr, context) && bound.binds == 2,
          "a runtime target outside the object bind virtual slot is rejected");
  settings_widget_forced_schema = &release_schema;
  wrong_slot                    = release;
  wrong_slot.slot               = 39;
  Require(!lifecycle->TryReleaseContext() && bound.releases == 1,
          "a runtime target outside the release virtual slot is rejected");
  settings_widget_forced_actual = nullptr;
  Require(!lifecycle->TryReleaseContext() && bound.releases == 1, "a missing actual release remains unavailable");
  virtual_method_override = nullptr;

  bind.has_full_generic_sharing_signature = release.has_full_generic_sharing_signature = true;
  bind.invoker_method = release.invoker_method = &WarpInvokerSentinel;
  settings_widget_shared_bind                  = &bind;
  settings_widget_shared_release               = &release;
  tow_runtime_invoke_override                  = &SettingsWidgetRuntimeInvoke;
  Require(lifecycle->TryBindContext(nullptr, context).succeeded && bound.binds == 3 && reader->Context() == context,
          "shared generic Widget object bind uses managed invocation and preserves context identity");
  Require(lifecycle->TryReleaseContext().succeeded && bound.releases == 2 && !reader->Context()
              && settings_widget_boxed_calls == 2,
          "shared generic Widget release uses managed invocation and clears context");
  tow_runtime_invoke_override             = nullptr;
  bind.has_full_generic_sharing_signature = release.has_full_generic_sharing_signature = false;
  settings_widget_release_throws                                                       = true;
  Require(!lifecycle->TryReleaseContext() && bound.releases == 3,
          "a native release exception remains failed rather than a fabricated successful release");
  settings_widget_release_throws = false;
  Require(lifecycle->TryBindContext(nullptr, context).succeeded && bound.binds == 4
              && lifecycle->TryReleaseContext().succeeded && bound.releases == 4,
          "qualified bind and release remain usable after refused metadata and exceptions");
  Require(settings_widget_base_bind_calls == 0 && settings_widget_base_release_calls == 0
              && settings_page_typed_calls == 0,
          "all lifecycle calls bypass base implementations and earlier typed bind overloads");

  auto& add = AddMethod(settings, "AddCategory", reinterpret_cast<void*>(&SettingsPageAddCategory), T(category),
                        {T(container), T(string_class), T(string_class), T(query)});
  settings_page_category.klass = &category.klass;
  Il2CppObject settings_object{}, parent_object{}, query_object{};
  settings_object.klass      = &settings.klass;
  parent_object.klass        = &container.klass;
  query_object.klass         = &query.klass;
  auto*      native_settings = reinterpret_cast<SettingsPageContext*>(&settings_object);
  auto*      native_parent   = reinterpret_cast<SettingsContainerContext*>(&parent_object);
  auto*      native_query    = reinterpret_cast<settings_binding::ReferenceDelegate*>(&query_object);
  const auto added           = native_settings->TryAddCategory(native_parent, nullptr, native_query);
  Require(added && added.value == reinterpret_cast<SettingsCategoryContext*>(&settings_page_category)
              && settings_page_seen_parent == &parent_object && settings_page_seen_query == &query_object,
          "native category construction preserves its explicit staging parent and exact supplied query identity");
  settings_page_category_throws = true;
  Require(!native_settings->TryAddCategory(native_parent, nullptr, native_query) && settings_page_category_calls == 2,
          "category append-then-throw remains failed instead of a fabricated null/success observation");
  settings_page_category_throws = false;
  const auto* category_return   = add.return_type;
  add.return_type               = T(object_class);
  Require(!native_settings->TryAddCategory(native_parent, nullptr, native_query) && settings_page_category_calls == 2,
          "a base Object return cannot impersonate the sealed CategoryOptionContext contract");
  add.return_type = category_return;
  std::cout << "IL2CPP native settings categories and shared Widget bind/release lifecycle passed\n";
}
} // namespace
