#pragma once

// Retained client273 native Settings navigation. Keep the untyped Widget virtual slot distinct from Widget<IList>.
#include "settings.h"
#include <optional>
#include <vector>

namespace settings_pages_binding
{
inline il2cpp::CallResult<settings_binding::ReferenceDelegate*> Query(SettingsOptionContext* row)
{ return row->TryCall<"get_QueryOptionState", settings_binding::ReferenceDelegate*>(); }
inline il2cpp::CallResult<SettingsContainerContext*> Parent(SettingsOptionContext* row)
{ return row->TryCall<"get_Parent", SettingsContainerContext*>(); }
inline il2cpp::CallResult<Il2CppString*> LabelIdentifier(SettingsOptionContext* row)
{
  const auto label = row->TryCall<"get_LabelContext", LocaleTextContext*>();
  if (!label)
    return {};
  auto root = il2cpp::GcHandle::Strong(label.value);
  if (label.value && !root)
    return {};
  return root ? root.Get<LocaleTextContext>()->TryCall<"get_Identifier", Il2CppString*>()
              : il2cpp::CallResult<Il2CppString*>{true, nullptr};
}
inline std::optional<std::vector<il2cpp::GcHandle>> Children(SettingsContainerContext* parent)
{
  auto       list_root = il2cpp::GcHandle::Strong(parent->Children());
  auto*      list      = list_root.Get<il2cpp::List<SettingsOptionContext*>>();
  const auto count     = list ? list->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return std::nullopt;
  std::vector<il2cpp::GcHandle> rows;
  rows.reserve(static_cast<std::size_t>(count.value));
  for (int32_t i = 0; i < count.value; ++i) {
    const auto value = list->TryCall<"get_Item", SettingsOptionContext*(int32_t)>(i);
    if (!value)
      return std::nullopt;
    auto root = il2cpp::GcHandle::Strong(value.value);
    if (value.value && !root)
      return std::nullopt;
    rows.push_back(std::move(root));
  }
  return rows;
}
inline il2cpp::CallResult<SettingsContainerContext*> NewStagingContainer()
{
  auto  root  = il2cpp::GcHandle::Strong(il2cpp::Allocate<SettingsContainerContext>());
  auto* value = root.Get<SettingsContainerContext>();
  if (!value)
    return {};
  const auto constructed = value->TryCall<".ctor", void>();
  return constructed ? il2cpp::CallResult<SettingsContainerContext*>{true, value}
                     : il2cpp::CallResult<SettingsContainerContext*>{};
}
} // namespace settings_pages_binding

struct SettingsCategoryContext : SettingsContainerContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "CategoryOptionContext">;
  il2cpp::CallResult<settings_binding::ReferenceDelegate*> TryQuery()
  { return TryCall<"get_QueryOptionState", settings_binding::ReferenceDelegate*>(); }
  il2cpp::CallResult<SettingsContainerContext*> TryParent()
  { return TryCall<"get_Parent", SettingsContainerContext*>(); }
};

struct SettingsPageContext : SettingsContext {
  using Class = SettingsContext::Class;
  static il2cpp::Method AddCategoryMethod()
  {
    const auto  method = Class::Method<SettingsCategoryContext*(SettingsContainerContext*, Il2CppString*, Il2CppString*,
                                                                settings_binding::ReferenceDelegate*)>("AddCategory");
    const auto* info   = method.Info();
    if (!info || !info->parameters || !settings_binding::Reference(info->return_type)
        || il2cpp_class_from_type(info->return_type) != SettingsCategoryContext::Class::Raw()
        || !settings_binding::Reference(info->parameters[0])
        || il2cpp_class_from_type(info->parameters[0]) != SettingsContainerContext::Class::Raw()
        || info->parameters[1]->byref || info->parameters[1]->type != IL2CPP_TYPE_STRING || info->parameters[2]->byref
        || info->parameters[2]->type != IL2CPP_TYPE_STRING || !settings_binding::Reference(info->parameters[3]))
      return {};
    return method;
  }
  static Il2CppObject* CategoryDelegate(SettingsSectionDirector* owner, const MethodInfo* method)
  {
    const auto add = AddCategoryMethod();
    return add ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(add.Info()->parameters[3]), owner,
                                           il2cpp::Method(method))
               : nullptr;
  }
  il2cpp::CallResult<SettingsCategoryContext*> TryAddCategory(SettingsContainerContext* parent, Il2CppString* id,
                                                              settings_binding::ReferenceDelegate* query)
  {
    const auto method = AddCategoryMethod();
    return il2cpp::detail::TryInvoke<SettingsCategoryContext*>(method.Info(), this, parent, id, id, query);
  }
  il2cpp::CallResult<SettingsContainerContext*> TrySelected()
  { return TryCall<"get_SelectedOption", SettingsContainerContext*>(); }
  static il2cpp::Method AddTextMethod()
  {
    const auto method =
        Class::Method<void(SettingsContainerContext*, Il2CppString*, settings_binding::ReferenceDelegate*,
                           settings_binding::ReferenceDelegate*)>("AddText");
    const auto* info     = method.Info();
    const auto  category = AddCategoryMethod();
    if (!info || !info->parameters || !category || !settings_binding::Reference(info->parameters[0])
        || il2cpp_class_from_type(info->parameters[0]) != SettingsContainerContext::Class::Raw()
        || info->parameters[1]->byref || info->parameters[1]->type != IL2CPP_TYPE_STRING
        || !settings_binding::Reference(info->parameters[2]) || !settings_binding::Reference(info->parameters[3])
        || il2cpp_class_from_type(info->parameters[3]) != il2cpp_class_from_type(category.Info()->parameters[3]))
      return {};
    return method;
  }
  static Il2CppObject* TextDelegate(SettingsSectionDirector* owner, const MethodInfo* method)
  {
    const auto add = AddTextMethod();
    return add ? il2cpp::NewClosedDelegate(il2cpp_class_from_type(add.Info()->parameters[2]), owner,
                                           il2cpp::Method(method))
               : nullptr;
  }
  il2cpp::CallResult<void> TryAddText(SettingsContainerContext* parent, Il2CppString* id,
                                      settings_binding::ReferenceDelegate* getter,
                                      settings_binding::ReferenceDelegate* query)
  {
    const auto method = AddTextMethod();
    return il2cpp::detail::TryInvoke<void>(method.Info(), this, parent, id, getter, query);
  }
};

struct SettingsDataProvider : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "IDataContextProvider">;
};
struct SettingsUntypedWidget : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "Widget">;
  static il2cpp::Method BindMethod()
  {
    const auto method =
        Class::Method<void(SettingsDataProvider*, settings_binding::ReferenceObject*)>("BindDataContext");
    const auto* info = method.Info();
    if (!info || !(info->flags & METHOD_ATTRIBUTE_VIRTUAL) || info->parameters_count != 2 || !info->parameters
        || !settings_binding::Reference(info->parameters[0]) || !settings_binding::Reference(info->parameters[1])
        || il2cpp_class_from_type(info->parameters[0]) != SettingsDataProvider::Class::Raw()
        || il2cpp_class_from_type(info->parameters[1]) != il2cpp::ObjectClass())
      return {};
    return method;
  }
};
struct SettingsOptionPanel : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "OptionTabPanelWidget">;
  il2cpp::CallResult<void> TryBind(il2cpp::Array<SettingsOptionContext*>* rows)
  {
    const auto schema = SettingsUntypedWidget::BindMethod();
    if (!schema || !rows)
      return {};
    auto* lists = il2cpp::Class<"mscorlib", "System.Collections", "IList">::Raw();
    if (!lists || !il2cpp_class_is_assignable_from(lists, rows->klass))
      return {};
    const auto* actual = il2cpp_object_get_virtual_method(this, schema.Info());
    if (!actual || (actual->flags & METHOD_ATTRIBUTE_STATIC) || !(actual->flags & METHOD_ATTRIBUTE_VIRTUAL)
        || actual->slot != schema.Info()->slot || actual->parameters_count != 2 || !actual->parameters
        || !actual->return_type || actual->return_type->byref || actual->return_type->type != IL2CPP_TYPE_VOID
        || !settings_binding::Reference(actual->parameters[0]) || !settings_binding::Reference(actual->parameters[1])
        || il2cpp_class_from_type(actual->parameters[0]) != SettingsDataProvider::Class::Raw()
        || il2cpp_class_from_type(actual->parameters[1]) != il2cpp::ObjectClass())
      return {};
    return il2cpp::detail::TryInvoke<void>(actual, this, static_cast<SettingsDataProvider*>(nullptr),
                                           reinterpret_cast<settings_binding::ReferenceObject*>(rows));
  }
};
struct SettingsPageController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "GameSettingsViewController">;
  il2cpp::CallResult<SettingsPageContext*> TrySettings()
  { return TryCall<"get_CanvasContext", SettingsPageContext*>(); }
  SettingsOptionPanel* Panel()
  { return Field<"_optionTabPanel", SettingsOptionPanel*>(); }
  TextLocalizer* Title()
  { return Field<"_title", TextLocalizer*>(); }
};
struct SettingsCategoryWidget : Widget<SettingsCategoryContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "CategoryOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
};
struct SettingsTextContext : SettingsOptionContext {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "TextOptionContext">;
};
struct SettingsTextWidget : Widget<SettingsTextContext> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.GameSettings", "TextOptionWidget">;
  TextLocalizer* Label()
  { return Field<"_label", TextLocalizer*>(); }
};
