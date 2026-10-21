#pragma once

#include "game_strings_data.h"
#include "str_utils.h"
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <utility>

namespace game_strings
{
namespace detail
{
  inline Il2CppClass* Class(const char* assembly, const char* ns, const char* name)
  {
    auto* domain = il2cpp_domain_get();
    auto* a      = domain ? il2cpp_domain_assembly_open(domain, assembly) : nullptr;
    auto* image  = a ? il2cpp_assembly_get_image(a) : nullptr;
    return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
  }

  class Root
  {
    Il2CppGCHandle handle_;

  public:
    explicit Root(Il2CppObject* object)
        : handle_(object ? il2cpp_gchandle_new(object, false) : 0)
    {
    }
    ~Root()
    {
      if (handle_)
        il2cpp_gchandle_free(handle_);
    }
    Root(const Root&)                    = delete;
    Root&         operator=(const Root&) = delete;
    Il2CppObject* get() const
    { return handle_ ? il2cpp_gchandle_get_target(handle_) : nullptr; }
  };

  inline bool Invoke(const MethodInfo* method, Il2CppObject* object, void** args, Il2CppObject** result = nullptr)
  {
    if (!method)
      return false;
    Il2CppException* exception = nullptr;
    auto*            answer    = il2cpp_runtime_invoke(method, object, args, &exception);
    if (exception)
      return false;
    if (result)
      *result = answer;
    return true;
  }

  inline Il2CppObject* Property(Il2CppObject* object, const char* name)
  { return object ? IL2CppClassHelper(object->klass).GetProperty(name).GetRaw<Il2CppObject>(object) : nullptr; }

  inline std::string Text(Il2CppObject* object)
  {
    if (!object || std::strcmp(il2cpp_class_get_namespace(object->klass), "System")
        || std::strcmp(il2cpp_class_get_name(object->klass), "String"))
      return {};
    auto* string = reinterpret_cast<Il2CppString*>(object);
    return il2cpp_string_length(string) <= 1024 ? to_string(string) : std::string{};
  }
} // namespace detail

// Call on the game's main thread after localization is ready. No managed
// pointers or translated strings are retained, so each call uses the current
// game language. Empty means unavailable; callers choose their visible fallback.
inline std::string Localize(const char* identifier, const char* category, std::int64_t id,
                            std::span<const float> textParameters = {})
{
  if (!identifier || !category || !id || textParameters.size() > 8)
    return {};
  for (float value : textParameters)
    if (!std::isfinite(value))
      return {};
  static auto* contextClass = detail::Class("Assembly-CSharp", "Digit.Client.UI", "LocaleTextContext");
  static auto* utility      = detail::Class("Assembly-CSharp", "Digit.Client.Localization", "LocaleUtilities");
  static auto* objectClass  = detail::Class("mscorlib", "System", "Object");
  static auto* int64Class   = detail::Class("mscorlib", "System", "Int64");
  if (!contextClass || !utility || !objectClass || !int64Class)
    return {};
  detail::Root context(il2cpp_object_new(contextClass));
  detail::Root array(reinterpret_cast<Il2CppObject*>(il2cpp_array_new(objectClass, 1)));
  detail::Root number(il2cpp_value_box(int64Class, &id));
  detail::Root key(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(identifier)));
  detail::Root group(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(category)));
  if (!context.get() || !array.get() || !number.get() || !key.get() || !group.get())
    return {};
  auto* values = reinterpret_cast<Il2CppObject**>(reinterpret_cast<Il2CppArraySize*>(array.get())->vector);
  il2cpp_gc_wbarrier_set_field(array.get(), reinterpret_cast<void**>(values), number.get());
  void*         ctor[]     = {key.get(), group.get()};
  void*         apply[]    = {array.get()};
  bool          no         = false;
  void*         localize[] = {context.get(), &no, &no};
  Il2CppObject* answer     = nullptr;
  if (!detail::Invoke(IL2CppClassHelper(contextClass).GetMethodInfo(".ctor", 2), context.get(), ctor)
      || !detail::Invoke(IL2CppClassHelper(contextClass).GetMethodInfo("ApplyIdentifierParameters", 1), context.get(),
                         apply))
    return {};
  if (!textParameters.empty()) {
    static auto* singleClass = detail::Class("mscorlib", "System", "Single");
    if (!singleClass)
      return {};
    detail::Root parameters(reinterpret_cast<Il2CppObject*>(il2cpp_array_new(objectClass, textParameters.size())));
    if (!parameters.get())
      return {};
    for (std::size_t i = 0; i < textParameters.size(); ++i) {
      float        value = textParameters[i];
      detail::Root boxed(il2cpp_value_box(singleClass, &value));
      if (!boxed.get())
        return {};
      auto* slots = reinterpret_cast<Il2CppObject**>(reinterpret_cast<Il2CppArraySize*>(parameters.get())->vector);
      il2cpp_gc_wbarrier_set_field(parameters.get(), reinterpret_cast<void**>(slots + i), boxed.get());
    }
    bool  yes     = true;
    void* force[] = {&yes};
    void* text[]  = {parameters.get()};
    if (!detail::Invoke(IL2CppClassHelper(contextClass).GetMethodInfo("set_ForceLocalizeParams", 1), context.get(),
                        force)
        || !detail::Invoke(IL2CppClassHelper(contextClass).GetMethodInfo("ApplyTextParameters", 1), context.get(),
                           text))
      return {};
  }
  if (!detail::Invoke(IL2CppClassHelper(utility).GetMethodInfo("Localize", 3), nullptr, localize, &answer))
    return {};
  auto text = detail::Text(answer);
  return IsResolvedName(text, identifier, id) ? text : std::string{};
}

// Example: Name(specService, NameKind::OfficerAbility, eventAbilityId).
// The ability's BuffSpec.IdRefs.LocaId supplies the title identifier parameter.
inline NameLookup InspectName(Il2CppObject* specService, NameKind kind, std::int64_t specId,
                              std::span<const float> textParameters = {})
{
  const auto key = Key(kind);
  if (!specService || !specId || !key.method || (kind == NameKind::TechnologyEffect && textParameters.size() != 2))
    return {};
  bool          ignoreWarning = true;
  void*         args[]        = {&specId, &ignoreWarning};
  Il2CppObject* object        = nullptr;
  if (!detail::Invoke(IL2CppClassHelper(specService->klass).GetMethodInfo(key.method, key.arguments), specService, args,
                      &object))
    return {{}, 0, NameStatus::SpecLookupFailed};
  detail::Root spec(object);
  if (!spec.get())
    return {{}, 0, NameStatus::SpecMissing};
  detail::Root refs(detail::Property(spec.get(), "IdRefs"));
  detail::Root boxed(detail::Property(refs.get(), "LocaId"));
  auto*        type = boxed.get() ? il2cpp_class_get_type(boxed.get()->klass) : nullptr;
  if (!type || type->byref || type->type != IL2CPP_TYPE_I8)
    return {{}, 0, NameStatus::LocalizationIdMissing};
  const auto loca = *static_cast<std::int64_t*>(il2cpp_object_unbox(boxed.get()));
  if (!loca)
    return {{}, 0, NameStatus::LocalizationIdMissing};
  auto text = Localize(key.identifier, key.category, loca, textParameters);
  if (kind == NameKind::TechnologyEffect)
    text = PlainDescription(text);
  const auto status = text.empty() ? NameStatus::TranslationMissing : NameStatus::Resolved;
  return {std::move(text), loca, status};
}

inline std::string Name(Il2CppObject* specService, NameKind kind, std::int64_t specId)
{ return InspectName(specService, kind, specId).text; }

// Native ForbiddenTechEvent uses its AbilityValue in both float text slots.
// The localized template controls percent/number formatting and scaling.
inline NameLookup InspectTechnologyEffect(Il2CppObject* specService, std::int64_t abilityId, float value)
{
  const std::array values{value, value};
  return InspectName(specService, NameKind::TechnologyEffect, abilityId, values);
}
} // namespace game_strings
