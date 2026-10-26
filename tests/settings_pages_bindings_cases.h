#pragma once

// Included by il2cpp_bindings.cc after its controlled runtime/helpers, before main. These call production bindings.
#include "game/settings_pages.h"

namespace
{
int           settings_page_untyped_calls = 0, settings_page_typed_calls = 0, settings_page_category_calls = 0;
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
{ Require(false, "settings panel must dispatch through the non-generic Widget virtual slot"); }
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
  // Deliberately first: same name and arity, a different virtual slot and typed IList parameter.
  auto& typed = AddMethod(generic, "BindDataContext", reinterpret_cast<void*>(&SettingsPageTypedBind), T(void_class),
                          {T(provider), T(list)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  typed.slot  = 36;
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
  std::cout << "IL2CPP native settings category signatures and untyped virtual panel dispatch passed\n";
}
} // namespace
